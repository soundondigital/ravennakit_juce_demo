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

#pragma once

#include "application/ApplicationContext.hpp"
#include "models/AudioReceiversModel.hpp"
#include "util/MessageThreadExecutor.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

class ReceiversContainer : public juce::Component, public AudioReceiversModel::Subscriber
{
public:
    explicit ReceiversContainer (ApplicationContext& context);
    ~ReceiversContainer() override;

    void resized() override;

    void resizeToFitContent();

    void onAudioReceiverUpdated (rav::Id receiverId, const AudioReceiversModel::ReceiverState* state) override;
    void onAudioReceiverStatsUpdated (rav::Id receiverId, size_t streamIndex, const rav::rtp::PacketStats::Counters& stats) override;

private:
    static constexpr int kRowHeight = 158;
    static constexpr int kMargin = 10;
    static constexpr int kButtonHeight = 30;

    class SessionInfoComponent : public Component
    {
    public:
        explicit SessionInfoComponent (const juce::String& title);
        void update (const AudioReceiversModel::StreamState* state);
        void resized() override;

    private:
        juce::Label titleLabel_ { "title" };
        juce::Label sessionInfoLabel_ { "session_info" };
        juce::Label packetTimeLabel_ { " packet_time" };
        juce::Label statusLabel_ { "status" };
    };

    class PacketStatsComponent : public Component
    {
    public:
        PacketStatsComponent();
        void update (const rav::rtp::PacketStats::Counters& stats);
        void resized() override;

    private:
        juce::Label titleLabel_ { "title" };
        juce::Label jitterMaxLabel_ { "jitter_max" };
        juce::Label droppedLabel_ { "dropped" };
        juce::Label duplicatesLabel_ { "duplicates" };
        juce::Label outOfOrderLabel_ { "out_of_order" };
        juce::Label tooLateLabel_ { "too_late" };
    };

    class Row : public Component
    {
    public:
        explicit Row (AudioReceiversModel& audioReceivers, rav::Id receiverId);

        rav::Id getId() const;

        void update (const AudioReceiversModel::ReceiverState& state);
        void update (size_t streamIndex, const rav::rtp::PacketStats::Counters& stats);
        void paint (juce::Graphics& g) override;
        void resized() override;

    private:
        AudioReceiversModel& audioReceivers_;
        juce::TextEditor delayEditor_;
        rav::Id receiverId_;
        rav::RavennaReceiver::Configuration configuration_;

        // Label description
        juce::Label labelLabel_;
        juce::TextEditor labelEditor_;
        juce::Label descriptionLabel_;
        juce::TextEditor descriptionEditor_;

        // Session
        juce::Label sessionNameLabel_ { "session_name" };
        juce::Label audioFormatLabel_ { "audio_format" };
        juce::Label settingsLabel_ { "settings" };
        juce::Label delaySettingLabel_ { "delay" };

        // Buttons
        juce::TextButton showSdpButton_ { "" };
        juce::TextButton onOffButton_ { "Start" };
        juce::TextButton deleteButton_ { "" };

        // Warning
        juce::Label warningLabel_ { "warning" };

        SessionInfoComponent primarySessionInfo_ { "Primary" };
        PacketStatsComponent primaryPacketStats_;

        SessionInfoComponent secondarySessionInfo_ { "Secondary" };
        PacketStatsComponent secondaryPacketStats_;

        MessageThreadExecutor executor_;
    };

    ApplicationContext& context_;
    juce::OwnedArray<Row> rows_;
    juce::TextButton createButton { "Create receiver" };

    MessageThreadExecutor executor_;
};
