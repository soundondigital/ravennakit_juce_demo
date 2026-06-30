/*
 * Project: RAVENNAKIT demo application
 * Copyright (c) 2024-2025 Sound on Digital
 *
 * This file is part of the RAVENNAKIT demo application.
 *
 * The RAVENNAKIT demo application is licensed on a proprietary,
 * source-available basis. You are allowed to view, modify, and compile
 * this code for personal or internal evaluation only.
 *
 * You may NOT redistribute this file, any modified versions, or any
 * compiled binaries to third parties, and you may NOT use it for any
 * commercial purpose, except under a separate written agreement with
 * Sound on Digital.
 *
 * For the full license terms, see:
 *   LICENSE.md
 * or visit:
 *   https://ravennakit.com
 */

#include "AudioReceiversModel.hpp"
#include "util/Constants.hpp"

#include "ravennakit/core/audio/audio_buffer_view.hpp"
#include "ravennakit/core/audio/audio_data.hpp"

AudioReceiversModel::AudioReceiversModel (rav::RavennaNode& node) : node_ (node)
{
    node_.subscribe (this).wait();
    node_.subscribe_to_ptp_instance (&ptpSubscriber_).wait();
}

AudioReceiversModel::~AudioReceiversModel()
{
    node_.unsubscribe_from_ptp_instance (&ptpSubscriber_).wait();
    node_.unsubscribe (this).wait();
}

tl::expected<rav::Id, std::string> AudioReceiversModel::createReceiver (rav::RavennaReceiver::Configuration config) const
{
    return node_.create_receiver (std::move (config)).get();
}

void AudioReceiversModel::removeReceiver (const rav::Id receiverId) const
{
    node_.remove_receiver (receiverId).wait();
}

void AudioReceiversModel::updateReceiverConfiguration (const rav::Id senderId, rav::RavennaReceiver::Configuration config) const
{
    if (auto result = node_.update_receiver_configuration (senderId, std::move (config)).get(); !result)
    {
        RAV_LOG_ERROR ("Failed to update receiver configuration: {}", result.error());
    }
}

std::optional<std::string> AudioReceiversModel::getSdpTextForReceiver (const rav::Id receiverId) const
{
    return node_.get_sdp_text_for_receiver (receiverId).get();
}

bool AudioReceiversModel::subscribe (Subscriber* subscriber)
{
    if (subscribers_.add (subscriber))
    {
        for (const auto& receiver : receivers_)
        {
            subscriber->onAudioReceiverUpdated (receiver->receiverId_, &receiver->state_);
        }
        return true;
    }
    return false;
}

bool AudioReceiversModel::unsubscribe (const Subscriber* subscriber)
{
    return subscribers_.remove (subscriber);
}

void AudioReceiversModel::ravenna_receiver_added (const rav::RavennaReceiver& receiver)
{
    RAV_ASSERT_NODE_MAINTENANCE_THREAD (node_);

    executor_.callAsync ([this, streamId = receiver.get_id()] {
        RAV_ASSERT (findReceiver (streamId) == nullptr, "Receiver already exists");
        const auto& it = receivers_.emplace_back (std::make_unique<Receiver> (*this, streamId));
        if (targetFormat_.is_valid() && maxNumFramesPerBlock_ > 0)
            it->prepareOutput (targetFormat_, maxNumFramesPerBlock_);
        updateRealtimeSharedContext();
        for (auto* subscriber : subscribers_)
            subscriber->onAudioReceiverUpdated (streamId, &it->state_);
    });
}

void AudioReceiversModel::ravenna_receiver_removed (const rav::Id receiverId)
{
    RAV_ASSERT_NODE_MAINTENANCE_THREAD (node_);

    executor_.callAsync ([this, receiverId] {
        // No need to unsubscribe from the receiver (via ravenna_node), as the stream no longer exists at this point.
        for (auto it = receivers_.begin(); it != receivers_.end(); ++it)
        {
            if ((*it)->receiverId_ == receiverId)
            {
                std::unique_ptr<Receiver> tmp = std::move (*it); // Keep alive until after the context is updated
                receivers_.erase (it);
                updateRealtimeSharedContext();
                break;
            }
        }
        for (auto* subscriber : subscribers_)
            subscriber->onAudioReceiverUpdated (receiverId, nullptr);
    });
}

void AudioReceiversModel::audioDeviceIOCallbackWithContext (
    [[maybe_unused]] const float* const* inputChannelData,
    [[maybe_unused]] const int numInputChannels,
    float* const* outputChannelData,
    const int numOutputChannels,
    const int numSamples,
    [[maybe_unused]] const juce::AudioIODeviceCallbackContext& context)
{
    const auto& localClock = ptpSubscriber_.get_local_clock();
    auto ptpNow = localClock.now();

    TRACY_ZONE_SCOPED;

    // If time information is available, use that
    if (context.hostTimeNs != nullptr)
        ptpNow = localClock.get_adjusted_time (*context.hostTimeNs);

    RAV_ASSERT_DEBUG (numInputChannels >= 0, "Num input channels must be >= 0");
    RAV_ASSERT_DEBUG (numOutputChannels >= 0, "Num output channels must be >= 0");
    RAV_ASSERT_DEBUG (numSamples >= 0, "Num samples must be >= 0");

    rav::AudioBufferView outputBuffer { outputChannelData, static_cast<uint32_t> (numOutputChannels), static_cast<uint32_t> (numSamples) };
    outputBuffer.clear();

    if (!targetFormat_.is_valid())
        return;

    const auto rtpNow = ptpNow.to_rtp_timestamp32 (targetFormat_.sample_rate);

    if (!rtpTs_.has_value())
    {
        if (!localClock.is_locked())
            return;
        // The next line determines the PTP timestamp at the start of this block of audio.
        rtpTs_ = rtpNow;
    }

    // Positive means audio device is ahead of the PTP clock, negative means behind
    const auto drift = rav::WrappingUint32 (rtpNow).diff (*rtpTs_);
    auto ratio = static_cast<double> (static_cast<int32_t> (targetFormat_.sample_rate) + drift) /
                 static_cast<double> (targetFormat_.sample_rate);
    ratio = std::clamp (ratio, 1.0 - constants::jnd_pitch, 1.0 + constants::jnd_pitch);

    TRACY_PLOT ("Receiver drift", static_cast<double> (drift));
    TRACY_PLOT ("Receiver asrc ratio", ratio);

    RAV_ASSERT_DEBUG (resampler_ != nullptr, "No resampler set");

    const auto requiredInputNumFrames = resampleGetRequiredSamples (resampler_.get(), static_cast<int> (outputBuffer.num_frames()), ratio);

    auto intermediateBuffer = intermediateBuffer_.with_num_channels (static_cast<size_t> (numOutputChannels))
                                  .with_num_frames (requiredInputNumFrames);

    auto resamplerInputBuffer = resamplerBuffer_.with_num_channels (static_cast<size_t> (numOutputChannels))
                                    .with_num_frames (requiredInputNumFrames);

    resamplerInputBuffer.clear();

    const auto guard = realtimeSharedContext_.access_realtime();

    if (intermediateBuffer.num_frames() > 0)
    {
        for (auto* receiver : guard->receivers)
        {
            if (receiver->processBlock (intermediateBuffer, *rtpTs_, ptpNow))
                std::ignore = resamplerInputBuffer.add (intermediateBuffer);
        }
    }

    [[maybe_unused]] const auto result = resampleProcess (
        resampler_.get(),
        resamplerInputBuffer.data(),
        static_cast<int> (resamplerInputBuffer.num_frames()),
        outputBuffer.data(),
        static_cast<int> (outputBuffer.num_frames()),
        ratio);

    RAV_ASSERT_DEBUG (result.input_used == requiredInputNumFrames, "Num input frame mismatch");
    RAV_ASSERT_DEBUG (result.output_generated == outputBuffer.num_frames(), "Num output frame mismatch");

    *rtpTs_ += requiredInputNumFrames;
}

void AudioReceiversModel::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    TRACY_ZONE_SCOPED;

    RAV_ASSERT (device != nullptr, "Device expected to be not null");

    const auto newFormat = rav::AudioFormat {
        rav::AudioFormat::ByteOrder::le,
        rav::AudioEncoding::pcm_f32,
        rav::AudioFormat::ChannelOrdering::noninterleaved,
        static_cast<uint32_t> (device->getCurrentSampleRate()),
        static_cast<uint32_t> (device->getActiveOutputChannels().countNumberOfSetBits()),
    };

    if (!newFormat.is_valid())
    {
        RAV_LOG_WARNING ("Audio device format is not valid: {}", newFormat.to_string());
        return;
    }

    targetFormat_ = newFormat;

    maxNumFramesPerBlock_ = static_cast<uint32_t> (device->getCurrentBufferSizeSamples());

    resampler_.reset (resampleInit (static_cast<int> (targetFormat_.num_channels), 256, 320, 1.0, SUBSAMPLE_INTERPOLATE | BLACKMAN_HARRIS));

    if (resampler_ == nullptr)
        RAV_LOG_ERROR ("Failed to initialize resampler");

    const auto numOutputChannels = static_cast<size_t> (device->getActiveOutputChannels().countNumberOfSetBits());
    intermediateBuffer_.resize (numOutputChannels, maxNumFramesPerBlock_ * 2); // Times 2 to have room for asrc
    resamplerBuffer_.resize (numOutputChannels, maxNumFramesPerBlock_ * 2);    // Times 2 to have room for asrc

    for (const auto& stream : receivers_)
        stream->prepareOutput (targetFormat_, maxNumFramesPerBlock_);

    RAV_LOG_TRACE ("Audio device about to start");
}

void AudioReceiversModel::audioDeviceStopped()
{
    TRACY_ZONE_SCOPED;
    RAV_LOG_TRACE ("Audio device stopped");

    rtpTs_.reset();
    maxNumFramesPerBlock_ = 0;
    targetFormat_ = {};
    resampler_.reset();
}

AudioReceiversModel::Receiver::Receiver (AudioReceiversModel& owner, const rav::Id receiverId) : owner_ (owner), receiverId_ (receiverId)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    owner_.node_.subscribe_to_receiver (receiverId_, this).wait();
}

AudioReceiversModel::Receiver::~Receiver()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    owner_.node_.unsubscribe_from_receiver (receiverId_, this).wait();
}

void AudioReceiversModel::Receiver::prepareInput (const rav::AudioFormat& format)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    RAV_ASSERT (format.is_valid(), "Invalid format");
    state_.inputFormat = format;
    updateRealtimeSharedState();
}

void AudioReceiversModel::Receiver::prepareOutput (const rav::AudioFormat& format, const uint32_t maxNumFramesPerBlock)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    RAV_ASSERT (format.is_valid(), "Invalid format");
    RAV_ASSERT (maxNumFramesPerBlock > 0, "Num samples must be > 0");
    state_.outputFormat = format;
    state_.maxNumFramesPerBlock = maxNumFramesPerBlock;

    for (auto* subscriber : owner_.subscribers_)
        subscriber->onAudioReceiverUpdated (receiverId_, &state_);

    updateRealtimeSharedState();
}

bool AudioReceiversModel::Receiver::processBlock (
    rav::AudioBufferView<float>& outputBuffer,
    const uint32_t rtpTimestamp,
    const rav::ptp::Timestamp ptpTimestamp)
{
    TRACY_ZONE_SCOPED;

    outputBuffer.clear();

    auto guard = realtimeSharedState_.access_realtime();

    if (!guard->inputFormat.is_valid())
        return false;

    if (!guard->outputFormat.is_valid())
        return false;

    // If no conversion is needed, take the shortcut
    if (guard->inputFormat.sample_rate == guard->outputFormat.sample_rate)
        return owner_.node_.read_audio_data_realtime (receiverId_, outputBuffer, rtpTimestamp - guard->delayFrames, {}) != std::nullopt;

    if (guard->resampler == nullptr)
        return false;

    if (guard->rtpTimestamp == std::nullopt)
    {
        guard->rtpTimestamp = ptpTimestamp.from_rtp_timestamp32 (rtpTimestamp, guard->outputFormat.sample_rate)
                                  .to_rtp_timestamp32 (guard->inputFormat.sample_rate);
    }

    // Check how much the timestamp at the input frequency differs from the one of the output frequency.
    TRACY_PLOT (
        "Receiver timestamp diff",
        static_cast<int64_t> (ptpTimestamp.from_rtp_timestamp32 (*guard->rtpTimestamp, guard->inputFormat.sample_rate)
                                  .to_rtp_timestamp32 (guard->outputFormat.sample_rate)) -
            static_cast<int64_t> (rtpTimestamp));

    const auto requiredNumInputFrames = resampleGetRequiredSamples (
        guard->resampler.get(),
        static_cast<int> (outputBuffer.num_frames()),
        0.0); // + 1;

    auto resampleBuffer = guard->resampleBuffer.with_num_frames (requiredNumInputFrames);

    if (!owner_.node_.read_audio_data_realtime (receiverId_, resampleBuffer, *guard->rtpTimestamp - guard->delayFrames, {}))
        return false;

    [[maybe_unused]] const auto result = resampleProcess (
        guard->resampler.get(),
        resampleBuffer.data(),
        static_cast<int> (resampleBuffer.num_frames()),
        outputBuffer.data(),
        static_cast<int> (outputBuffer.num_frames()),
        0.0);

    RAV_ASSERT_DEBUG (result.input_used != 0, "No input was used");

    *guard->rtpTimestamp += result.input_used;

    // The first call to the resampler seems to give back one frame less than expected consistently, in which case return false to skip this
    // block.
    return result.output_generated == outputBuffer.num_frames();
}

void AudioReceiversModel::Receiver::ravenna_receiver_parameters_updated (const rav::rtp::AudioReceiver::ReaderParameters& parameters)
{
    RAV_ASSERT_NODE_MAINTENANCE_THREAD (owner_.node_);

    executor_.callAsync ([this, parameters] {
        state_.streams.clear();

        for (const auto& stream : parameters.streams)
            state_.streams.push_back (StreamState { stream, {} });

        if (parameters.audio_format.is_valid() && state_.inputFormat != parameters.audio_format)
            prepareInput (parameters.audio_format);

        for (auto* subscriber : owner_.subscribers_)
            subscriber->onAudioReceiverUpdated (receiverId_, &state_);
    });
}

void AudioReceiversModel::Receiver::ravenna_receiver_configuration_updated (
    const rav::RavennaReceiver& receiver,
    const rav::RavennaReceiver::Configuration& configuration)
{
    RAV_ASSERT_NODE_MAINTENANCE_THREAD (owner_.node_);
    auto receiverId = receiver.get_id();

    executor_.callAsync ([this, receiverId, configuration] {
        RAV_ASSERT (receiverId == receiverId_, "Receiver ID mismatch");
        state_.configuration = configuration;
        updateRealtimeSharedState();
        for (auto* subscriber : owner_.subscribers_)
            subscriber->onAudioReceiverUpdated (receiverId, &state_);
    });
}

void AudioReceiversModel::Receiver::ravenna_receiver_stream_state_updated (
    const rav::rtp::AudioReceiver::StreamInfo& stream_info,
    const rav::rtp::AudioReceiver::StreamState state)
{
    RAV_ASSERT_NODE_MAINTENANCE_THREAD (owner_.node_);

    executor_.callAsync ([this, stream_info, state] {
        bool changed = false;
        for (auto& streamState : state_.streams)
        {
            if (streamState.stream.session == stream_info.session)
            {
                streamState.state = state;
                changed = true;
            }
        }

        if (!changed)
            return;

        for (auto* subscriber : owner_.subscribers_)
            subscriber->onAudioReceiverUpdated (receiverId_, &state_);
    });
}

void AudioReceiversModel::Receiver::ravenna_receiver_stream_stats_updated (
    rav::Id receiver_id,
    size_t stream_index,
    const rav::rtp::PacketStats::Counters& stats)
{
    RAV_ASSERT_NODE_MAINTENANCE_THREAD (owner_.node_);
    executor_.callAsync ([this, receiver_id, stream_index, stats] {
        for (auto* subscriber : owner_.subscribers_)
            subscriber->onAudioReceiverStatsUpdated (receiver_id, stream_index, stats);
    });
}

void AudioReceiversModel::Receiver::updateRealtimeSharedState()
{
    RealtimeSharedState newState;

    newState.delayFrames = state_.configuration.delay_frames;
    newState.inputFormat = state_.inputFormat;
    newState.outputFormat = state_.outputFormat;

    if (newState.inputFormat.is_valid() && newState.outputFormat.is_valid() &&
        newState.inputFormat.sample_rate != newState.outputFormat.sample_rate)
    {
        newState.resampler.reset (resampleFixedRatioInit (
            static_cast<int> (state_.inputFormat.num_channels),
            256,
            320,
            state_.inputFormat.sample_rate,
            state_.outputFormat.sample_rate,
            0,
            SUBSAMPLE_INTERPOLATE | BLACKMAN_HARRIS | INCLUDE_LOWPASS));
        const auto ratio = static_cast<double> (state_.inputFormat.sample_rate) / static_cast<double> (state_.outputFormat.sample_rate);
        newState.resampleBuffer.resize (state_.inputFormat.num_channels, static_cast<uint32_t> (state_.maxNumFramesPerBlock * ratio) + 8);
    }

    if (!realtimeSharedState_.update (std::move (newState)))
    {
        RAV_LOG_ERROR ("Failed to update realtime shared state");
    }
}

AudioReceiversModel::Receiver* AudioReceiversModel::findReceiver (const rav::Id receiverId) const
{
    JUCE_ASSERT_MESSAGE_THREAD;
    for (const auto& receiver : receivers_)
        if (receiver->receiverId_ == receiverId)
            return receiver.get();
    return nullptr;
}

void AudioReceiversModel::updateRealtimeSharedContext()
{
    RealtimeSharedContext newContext;
    for (const auto& stream : receivers_)
        newContext.receivers.push_back (stream.get());
    if (!realtimeSharedContext_.update (std::move (newContext)))
    {
        RAV_LOG_ERROR ("Failed to update realtime shared context");
    }
}
