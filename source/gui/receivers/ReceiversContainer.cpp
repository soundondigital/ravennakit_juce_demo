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

#include "ReceiversContainer.hpp"

#include "gui/lookandfeel/Constants.hpp"
#include "gui/lookandfeel/ThisLookAndFeel.hpp"
#include "gui/widgets/SdpViewerComponent.hpp"
#include "ravennakit/core/support.hpp"

ReceiversContainer::ReceiversContainer (ApplicationContext& context) : context_ (context)
{
    createButton.onClick = [this] {
        auto config = rav::RavennaReceiver::Configuration::default_config();
        config.auto_update_sdp = false;
        std::ignore = context_.getAudioReceivers().createReceiver (std::move (config));
    };
    addAndMakeVisible (createButton);

    if (!context_.getAudioReceivers().subscribe (this))
    {
        RAV_LOG_ERROR ("Failed to add subscriber");
    }
}

ReceiversContainer::~ReceiversContainer()
{
    if (!context_.getAudioReceivers().unsubscribe (this))
    {
        RAV_LOG_ERROR ("Failed to remove subscriber");
    }
}

void ReceiversContainer::resized()
{
    auto b = getLocalBounds().reduced (kMargin);

    for (auto i = 0; i < rows_.size(); ++i)
    {
        rows_.getUnchecked (i)->setBounds (b.removeFromTop (kRowHeight));
        b.removeFromTop (kMargin);
    }

    b.removeFromTop (kMargin * 2);
    createButton.setBounds (b.withSizeKeepingCentre (200, kButtonHeight));
}

void ReceiversContainer::resizeToFitContent()
{
    setSize (getWidth(), rows_.size() * kRowHeight + kMargin + kMargin * rows_.size() + kMargin * 2 + kButtonHeight);
}

void ReceiversContainer::onAudioReceiverUpdated (rav::Id receiverId, const AudioReceiversModel::ReceiverState* state)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (state != nullptr)
    {
        for (const auto& row : rows_)
        {
            if (row->getId() == receiverId)
            {
                row->update (*state);
                return;
            }
        }

        auto* row = rows_.add (std::make_unique<Row> (context_.getAudioReceivers(), receiverId));
        RAV_ASSERT (row != nullptr, "Failed to create row");
        row->update (*state);
        addAndMakeVisible (row);
        resizeToFitContent();
    }
    else
    {
        for (auto i = 0; i < rows_.size(); ++i)
        {
            if (rows_.getUnchecked (i)->getId() == receiverId)
            {
                rows_.remove (i);
                resizeToFitContent();
                return;
            }
        }
    }
}

void ReceiversContainer::onAudioReceiverStatsUpdated (
    const rav::Id receiverId,
    const size_t streamIndex,
    const rav::rtp::PacketStats::Counters& stats)
{
    for (auto* row : rows_)
        if (row->getId() == receiverId)
            row->update (streamIndex, stats);
}

ReceiversContainer::SessionInfoComponent::SessionInfoComponent (const juce::String& title)
{
    titleLabel_.setText (title, juce::dontSendNotification);
    titleLabel_.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel_);

    sessionInfoLabel_.setText ("Session: ", juce::dontSendNotification);
    addAndMakeVisible (sessionInfoLabel_);

    packetTimeLabel_.setText ("Packet time: ", juce::dontSendNotification);
    addAndMakeVisible (packetTimeLabel_);

    statusLabel_.setText ("Status: ", juce::dontSendNotification);
    addAndMakeVisible (statusLabel_);
}

void ReceiversContainer::SessionInfoComponent::update (const AudioReceiversModel::StreamState* state)
{
    if (state == nullptr)
        return;

    sessionInfoLabel_.setText ("Session: " + state->stream.session.to_string(), juce::dontSendNotification);
    packetTimeLabel_.setText ("Packet time: " + juce::String (state->stream.packet_time_frames), juce::dontSendNotification);
    statusLabel_.setText (juce::String ("Status: ") + rav::rtp::to_string (state->state), juce::dontSendNotification);
}

void ReceiversContainer::SessionInfoComponent::resized()
{
    auto b = getLocalBounds();
    titleLabel_.setBounds (b.removeFromTop (20));
    sessionInfoLabel_.setBounds (b.removeFromTop (20));
    packetTimeLabel_.setBounds (b.removeFromTop (20));
    statusLabel_.setBounds (b.removeFromTop (20));
}

ReceiversContainer::PacketStatsComponent::PacketStatsComponent()
{
    titleLabel_.setText ("Packet Stats", juce::dontSendNotification);
    titleLabel_.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel_);

    addAndMakeVisible (jitterMaxLabel_);
    addAndMakeVisible (droppedLabel_);
    addAndMakeVisible (duplicatesLabel_);
    addAndMakeVisible (outOfOrderLabel_);
    addAndMakeVisible (tooLateLabel_);
}

void ReceiversContainer::PacketStatsComponent::update (const rav::rtp::PacketStats::Counters& stats)
{
    jitterMaxLabel_.setText ("Jitter (ms): " + juce::String (stats.jitter), juce::dontSendNotification);
    droppedLabel_.setText ("Dropped: " + juce::String (stats.dropped), juce::dontSendNotification);
    duplicatesLabel_.setText ("Duplicates: " + juce::String (stats.duplicates), juce::dontSendNotification);
    outOfOrderLabel_.setText ("Out of Order: " + juce::String (stats.out_of_order), juce::dontSendNotification);
    tooLateLabel_.setText ("Too Late: " + juce::String (stats.too_late), juce::dontSendNotification);
}

void ReceiversContainer::PacketStatsComponent::resized()
{
    auto b = getLocalBounds();
    titleLabel_.setBounds (b.removeFromTop (20));
    jitterMaxLabel_.setBounds (b.removeFromTop (20));
    droppedLabel_.setBounds (b.removeFromTop (20));
    duplicatesLabel_.setBounds (b.removeFromTop (20));
    outOfOrderLabel_.setBounds (b.removeFromTop (20));
    tooLateLabel_.setBounds (b.removeFromTop (20));
}

ReceiversContainer::Row::Row (AudioReceiversModel& audioReceivers, const rav::Id receiverId) :
    audioReceivers_ (audioReceivers),
    receiverId_ (receiverId)
{
    constexpr float fontSize = 14.0f;

    addAndMakeVisible (audioFormatLabel_);

    settingsLabel_.setFont (juce::FontOptions (fontSize, juce::Font::bold));
    settingsLabel_.setText ("Settings", juce::dontSendNotification);
    addAndMakeVisible (settingsLabel_);

    delaySettingLabel_.setText ("Delay (samples)", juce::dontSendNotification);
    addAndMakeVisible (delaySettingLabel_);

    delayEditor_.setInputRestrictions (10, "0123456789");
    delayEditor_.onReturnKey = [this] {
        const auto value = static_cast<uint32_t> (delayEditor_.getText().getIntValue());
        auto config = configuration_;
        config.delay_frames = value;
        audioReceivers_.updateReceiverConfiguration (receiverId_, std::move (config));
        unfocusAllComponents();
    };
    delayEditor_.onEscapeKey = [this] {
        delayEditor_.setText (juce::String (configuration_.delay_frames));
        unfocusAllComponents();
    };
    delayEditor_.onFocusLost = [this] {
        delayEditor_.onReturnKey();
    };
    addAndMakeVisible (delayEditor_);

    showSdpButton_.setButtonText ("SDP");
    showSdpButton_.setColour (juce::TextButton::ColourIds::buttonColourId, Constants::Colours::grey);
    showSdpButton_.onClick = [this] {
        auto content = std::make_unique<SdpViewerComponent> (rav::sdp::to_string (configuration_.sdp).value_or (""));
        content->setSize (400, 400);
        content->onApply = [this] (rav::sdp::SessionDescription sdp) {
            auto config = configuration_;
            config.sdp = std::move (sdp);
            config.auto_update_sdp = false;
            audioReceivers_.updateReceiverConfiguration (receiverId_, std::move (config));
        };

        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = "SDP Text";
        o.content.setOwned (content.release());
        o.componentToCentreAround = getTopLevelComponent();
        o.dialogBackgroundColour = Constants::Colours::windowBackground;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = true;
        o.useBottomRightCornerResizer = false;
        if (auto* dialog = o.launchAsync())
            dialog->setResizeLimits (400, 400, 99999, 99999);
    };
    addAndMakeVisible (showSdpButton_);

    onOffButton_.setButtonText ("On");
    onOffButton_.setClickingTogglesState (true);
    onOffButton_.setColour (juce::TextButton::ColourIds::buttonColourId, Constants::Colours::grey);
    onOffButton_.setColour (juce::TextButton::ColourIds::buttonOnColourId, Constants::Colours::green);
    onOffButton_.onClick = [this] {
        auto config = configuration_;
        config.enabled = onOffButton_.getToggleState();
        audioReceivers_.updateReceiverConfiguration (receiverId_, std::move (config));
    };
    addAndMakeVisible (onOffButton_);

    deleteButton_.setButtonText ("Delete");
    deleteButton_.setColour (juce::TextButton::ColourIds::buttonColourId, Constants::Colours::red);
    deleteButton_.onClick = [this] {
        audioReceivers_.removeReceiver (receiverId_);
    };
    addAndMakeVisible (deleteButton_);

    addAndMakeVisible (primarySessionInfo_);
    addAndMakeVisible (primaryPacketStats_);

    addAndMakeVisible (secondarySessionInfo_);
    addAndMakeVisible (secondaryPacketStats_);

    warningLabel_.setColour (juce::Label::ColourIds::textColourId, Constants::Colours::warning);
    addAndMakeVisible (warningLabel_);

    // Label and description

    labelLabel_.setText ("Label", juce::dontSendNotification);
    addAndMakeVisible (labelLabel_);

    labelEditor_.onReturnKey = [this] {
        auto config = configuration_;
        config.label = labelEditor_.getText().toStdString();
        audioReceivers_.updateReceiverConfiguration (receiverId_, std::move (config));
        unfocusAllComponents();
    };
    labelEditor_.onFocusLost = [this] {
        labelEditor_.onReturnKey();
    };
    labelEditor_.onEscapeKey = [this] {
        labelEditor_.setText (juce::String (configuration_.label));
        unfocusAllComponents();
    };
    labelEditor_.setIndents (8, 8);
    addAndMakeVisible (labelEditor_);

    descriptionLabel_.setText ("Description", juce::dontSendNotification);
    addAndMakeVisible (descriptionLabel_);

    descriptionEditor_.onReturnKey = [this] {
        auto config = configuration_;
        config.description = descriptionEditor_.getText().toStdString();
        audioReceivers_.updateReceiverConfiguration (receiverId_, std::move (config));
        unfocusAllComponents();
    };
    descriptionEditor_.onFocusLost = [this] {
        descriptionEditor_.onReturnKey();
    };
    descriptionEditor_.onEscapeKey = [this] {
        descriptionEditor_.setText (juce::String (configuration_.description));
        unfocusAllComponents();
    };
    descriptionEditor_.setIndents (8, 8);
    addAndMakeVisible (descriptionEditor_);

    sessionNameLabel_.setFont (juce::FontOptions (fontSize, juce::Font::bold));
    sessionNameLabel_.setText ("Source: (none)", juce::dontSendNotification);
    addAndMakeVisible (sessionNameLabel_);

    update (0, {});
    update (1, {});
}

rav::Id ReceiversContainer::Row::getId() const
{
    return receiverId_;
}

void ReceiversContainer::Row::update (const AudioReceiversModel::ReceiverState& state)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    configuration_ = state.configuration;

    sessionNameLabel_.setText (
        "Source: " + (state.configuration.session_name.empty() ? "(none)" : state.configuration.session_name),
        juce::dontSendNotification);

    warningLabel_.setColour (juce::Label::ColourIds::textColourId, Constants::Colours::warning);
    if (state.configuration.sdp.session_name.empty())
        warningLabel_.setText ("Warning: no SDP available", juce::dontSendNotification);
    else if (!state.outputFormat.is_valid())
        warningLabel_.setText ("Warning: invalid audio device settings", juce::dontSendNotification);
    else if (!state.inputFormat.is_valid())
        warningLabel_.setText ("Warning: invalid input format", juce::dontSendNotification);
    else if (state.inputFormat.num_channels != state.outputFormat.num_channels)
        warningLabel_.setText ("Warning: channel count mismatch", juce::dontSendNotification);
    else if (state.inputFormat.sample_rate != state.outputFormat.sample_rate)
    {
        warningLabel_.setText (
            fmt::format ("Sample rate conversion engaged ({} to {})", state.inputFormat.sample_rate, state.outputFormat.sample_rate),
            juce::dontSendNotification);
        warningLabel_.setColour (juce::Label::ColourIds::textColourId, Constants::Colours::textDisabled);
    }
    else
        warningLabel_.setText ({}, juce::dontSendNotification);

    const auto* pri = !state.streams.empty() ? &state.streams[0] : nullptr;
    const auto* sec = state.streams.size() >= 2 ? &state.streams[1] : nullptr;

    audioFormatLabel_.setText (state.inputFormat.to_string(), juce::dontSendNotification);

    primarySessionInfo_.update (pri);
    secondarySessionInfo_.update (sec);

    secondarySessionInfo_.setVisible (sec != nullptr);
    secondaryPacketStats_.setVisible (sec != nullptr);

    if (!delayEditor_.hasKeyboardFocus (true))
        delayEditor_.setText (juce::String (state.configuration.delay_frames));

    if (!labelEditor_.hasKeyboardFocus (true))
        labelEditor_.setText (juce::String (state.configuration.label));

    if (!descriptionEditor_.hasKeyboardFocus (true))
        descriptionEditor_.setText (juce::String (state.configuration.description));

    onOffButton_.setToggleState (state.configuration.enabled, juce::dontSendNotification);
    onOffButton_.setButtonText (state.configuration.enabled ? "On" : "Off");

    repaint();
}

void ReceiversContainer::Row::update (const size_t streamIndex, const rav::rtp::PacketStats::Counters& stats)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (streamIndex == 0)
        primaryPacketStats_.update (stats);
    if (streamIndex == 1)
        secondaryPacketStats_.update (stats);

    repaint();
}

void ReceiversContainer::Row::paint (juce::Graphics& g)
{
    g.setColour (Constants::Colours::rowBackground);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
}

void ReceiversContainer::Row::resized()
{
    constexpr int rowHeight = 20;
    constexpr int buttonHeight = 28;

    auto b = getLocalBounds().reduced (kMargin);

    auto buttonBounds = b.removeFromRight (89);
    onOffButton_.setBounds (buttonBounds.removeFromTop (buttonHeight));
    buttonBounds.removeFromTop (6);
    deleteButton_.setBounds (buttonBounds.removeFromTop (buttonHeight));
    buttonBounds.removeFromTop (6);
    showSdpButton_.setBounds (buttonBounds.removeFromTop (buttonHeight));

    constexpr int kColumnWidth = 250;

    auto leftSection = b.removeFromLeft (kColumnWidth * 2);
    auto rightSection = b;

    warningLabel_.setBounds (rightSection.removeFromBottom (16).withTrimmedLeft (1));

    auto column1 = leftSection.removeFromLeft (kColumnWidth);
    auto column2 = leftSection.removeFromLeft (kColumnWidth);
    auto column3 = rightSection.removeFromLeft (150);
    auto column4 = rightSection.removeFromLeft (150);
    auto column5 = rightSection.removeFromLeft (kColumnWidth).withTrimmedRight (20);

    sessionNameLabel_.setBounds (column1.removeFromTop (rowHeight));
    audioFormatLabel_.setBounds (column1.removeFromTop (rowHeight));

    settingsLabel_.setBounds (column2.removeFromTop (rowHeight));
    auto delayBounds = column2.removeFromTop (rowHeight);
    delaySettingLabel_.setBounds (delayBounds.removeFromLeft (115));
    delayEditor_.setBounds (delayBounds.withHeight (24).withWidth (60).translated (0, -1));

    column1.removeFromTop (17);
    column2.removeFromTop (17);

    primarySessionInfo_.setBounds (column1.removeFromTop (rowHeight * 4));
    secondarySessionInfo_.setBounds (column2.removeFromTop (rowHeight * 4));

    primaryPacketStats_.setBounds (column3.removeFromTop (rowHeight * 6));
    secondaryPacketStats_.setBounds (column4.removeFromTop (rowHeight * 6));

    labelLabel_.setBounds (column5.removeFromTop (rowHeight));
    labelEditor_.setBounds (column5.removeFromTop (31));

    column5.removeFromTop (8);

    descriptionLabel_.setBounds (column5.removeFromTop (rowHeight));
    descriptionEditor_.setBounds (column5.removeFromTop (31));
}
