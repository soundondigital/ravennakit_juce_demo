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

#include "SettingsMainComponent.hpp"

#include "gui/lookandfeel/Constants.hpp"

SettingsMainComponent::SettingsMainComponent (ApplicationContext& context) : context_ (context)
{
    networkSettingsLabel_.setText ("Network Interfaces", juce::dontSendNotification);
    networkSettingsLabel_.setFont (juce::FontOptions (16.f, juce::Font::bold));
    networkSettingsLabel_.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (networkSettingsLabel_);

    primaryNetworkInterfaceLabel_.setText ("Primary", juce::dontSendNotification);
    addAndMakeVisible (primaryNetworkInterfaceLabel_);

    primaryNetworkInterfaceComboBox_.setTextWhenNothingSelected ("Primary Network Interface");
    primaryNetworkInterfaceComboBox_.onChange = [this] {
        selectNetworkInterfaces();
    };
    addAndMakeVisible (primaryNetworkInterfaceComboBox_);

    secondaryNetworkInterfaceLabel_.setText ("Secondary", juce::dontSendNotification);
    addAndMakeVisible (secondaryNetworkInterfaceLabel_);

    secondaryNetworkInterfaceComboBox_.setTextWhenNothingSelected ("Secondary Network Interface");
    secondaryNetworkInterfaceComboBox_.onChange = [this] {
        selectNetworkInterfaces();
    };
    addAndMakeVisible (secondaryNetworkInterfaceComboBox_);

    bonjourSettingsLabel_.setText ("Bonjour", juce::dontSendNotification);
    bonjourSettingsLabel_.setFont (juce::FontOptions (16.f, juce::Font::bold));
    bonjourSettingsLabel_.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (bonjourSettingsLabel_);

    enableSessionAdvertisementLabel_.setText ("Enable RAVENNA session advertisement", juce::dontSendNotification);
    addAndMakeVisible (enableSessionAdvertisementLabel_);

    enableSessionAdvertisement_.onClick = [this] {
        auto config = node_configuration_;
        config.enable_dnssd_session_advertisement = enableSessionAdvertisement_.getToggleState();
        context_.getRavennaNode().set_configuration (config).wait();
    };
    addAndMakeVisible (enableSessionAdvertisement_);

    enableNodeDiscoveryLabel_.setText ("Enable RAVENNA node discovery", juce::dontSendNotification);
    addAndMakeVisible (enableNodeDiscoveryLabel_);

    enableNodeDiscovery_.onClick = [this] {
        auto config = node_configuration_;
        config.enable_dnssd_node_discovery = enableNodeDiscovery_.getToggleState();
        context_.getRavennaNode().set_configuration (config).wait();
    };
    addAndMakeVisible (enableNodeDiscovery_);

    enableSessionDiscoveryLabel_.setText ("Enable RAVENNA session discovery", juce::dontSendNotification);
    addAndMakeVisible (enableSessionDiscoveryLabel_);

    enableSessionDiscovery_.onClick = [this] {
        auto config = node_configuration_;
        config.enable_dnssd_session_discovery = enableSessionDiscovery_.getToggleState();
        context_.getRavennaNode().set_configuration (config).wait();
    };
    addAndMakeVisible (enableSessionDiscovery_);

    buildInfoTitleLabel_.setText ("Version", juce::dontSendNotification);
    buildInfoTitleLabel_.setFont (juce::FontOptions (16.f, juce::Font::bold));
    addAndMakeVisible (buildInfoTitleLabel_);

    applicationVersionTitleLabel_.setText ("Application Version:", juce::dontSendNotification);
    addAndMakeVisible (applicationVersionTitleLabel_);

    applicationVersionLabel_.setText (PROJECT_VERSION_STRING, juce::dontSendNotification);
    addAndMakeVisible (applicationVersionLabel_);

    diagnosticsTitleLabel_.setText ("Diagnostics", juce::dontSendNotification);
    diagnosticsTitleLabel_.setFont (juce::FontOptions (16.f, juce::Font::bold));
    addAndMakeVisible (diagnosticsTitleLabel_);

    saveDiagnosticsButton_.setButtonText ("Save diagnostics package");
    saveDiagnosticsButton_.setColour (juce::TextButton::ColourIds::buttonColourId, Constants::Colours::grey);
    saveDiagnosticsButton_.onClick = [&] {
        saveDiagnosticsFile();
    };
    addAndMakeVisible (saveDiagnosticsButton_);

    revealLogsButton_.setButtonText ("Reveal log files");
    revealLogsButton_.setColour (juce::TextButton::ColourIds::buttonColourId, Constants::Colours::grey);
    revealLogsButton_.onClick = [] {
        logging::getLogFilesPath().revealToUser();
    };
    addAndMakeVisible (revealLogsButton_);

    updateNetworkInterfaces();

    context_.getRavennaNode().subscribe (this).wait();
}

SettingsMainComponent::~SettingsMainComponent()
{
    context_.getRavennaNode().unsubscribe (this).wait();
}

void SettingsMainComponent::resized()
{
    auto b = getLocalBounds().reduced (10);
    networkSettingsLabel_.setBounds (b.removeFromTop (28));

    int left = 84;
    constexpr int width = 380;

    auto row = b.removeFromTop (28);
    primaryNetworkInterfaceLabel_.setBounds (row.removeFromLeft (left));
    primaryNetworkInterfaceComboBox_.setBounds (row.withWidth (width));

    b.removeFromTop (10);

    row = b.removeFromTop (28);
    secondaryNetworkInterfaceLabel_.setBounds (row.removeFromLeft (left));
    secondaryNetworkInterfaceComboBox_.setBounds (row.withWidth (width));

    b.removeFromTop (20);

    left = 274;
    bonjourSettingsLabel_.setBounds (b.removeFromTop (28));

    row = b.removeFromTop (28);
    enableNodeDiscoveryLabel_.setBounds (row.removeFromLeft (left));
    enableNodeDiscovery_.setBounds (row.removeFromLeft (28));

    row = b.removeFromTop (28);
    enableSessionAdvertisementLabel_.setBounds (row.removeFromLeft (left));
    enableSessionAdvertisement_.setBounds (row.removeFromLeft (28));

    row = b.removeFromTop (28);
    enableSessionDiscoveryLabel_.setBounds (row.removeFromLeft (left));
    enableSessionDiscovery_.setBounds (row.removeFromLeft (28));

    b.removeFromTop (20);
    left = 135;
    buildInfoTitleLabel_.setBounds (b.removeFromTop (28));

    row = b.removeFromTop (28);

    applicationVersionTitleLabel_.setBounds (row.removeFromLeft (left));
    applicationVersionLabel_.setBounds (row.withWidth (width));

    b.removeFromTop (20);

    diagnosticsTitleLabel_.setBounds (b.removeFromTop (28));
    b.removeFromTop (10);
    row = b.removeFromTop (28).withTrimmedLeft (5);
    revealLogsButton_.setBounds (row.removeFromLeft (200));
    row.removeFromLeft (10);
    saveDiagnosticsButton_.setBounds (row.removeFromLeft (200));
}

void SettingsMainComponent::network_interface_config_updated (const rav::NetworkInterfaceConfig& config)
{
    RAV_ASSERT_NODE_MAINTENANCE_THREAD (context_.getRavennaNode());

    SafePointer safeThis (this);
    juce::MessageManager::callAsync ([safeThis, config] {
        if (!safeThis)
            return;

        const auto networkInterfaces = rav::NetworkInterfaceList::get_system_interfaces();

        if (const auto* primary = config.get_interface_for_rank (rav::rank::primary))
        {
            if (auto* iface = networkInterfaces.get_interface (*primary))
                safeThis->primaryNetworkInterfaceComboBox_.setText (iface->get_extended_display_name(), juce::dontSendNotification);
            else
                safeThis->primaryNetworkInterfaceComboBox_.setText (*primary + " (not found)", juce::dontSendNotification);
        }
        else
        {
            safeThis->primaryNetworkInterfaceComboBox_.setSelectedId (0, juce::dontSendNotification);
        }

        if (const auto* secondary = config.get_interface_for_rank (rav::rank::secondary))
        {
            if (auto* iface = networkInterfaces.get_interface (*secondary))
                safeThis->secondaryNetworkInterfaceComboBox_.setText (iface->get_extended_display_name(), juce::dontSendNotification);
            else
                safeThis->secondaryNetworkInterfaceComboBox_.setText (*secondary + " (not found)", juce::dontSendNotification);
        }
        else
        {
            safeThis->secondaryNetworkInterfaceComboBox_.setSelectedId (0, juce::dontSendNotification);
        }
    });
}

void SettingsMainComponent::ravenna_node_configuration_updated (const rav::RavennaNode::Configuration& configuration)
{
    RAV_ASSERT_NODE_MAINTENANCE_THREAD (context_.getRavennaNode());

    SafePointer safeThis (this);
    juce::MessageManager::callAsync ([safeThis, configuration] {
        if (!safeThis)
            return;

        safeThis->node_configuration_ = configuration;
        safeThis->enableNodeDiscovery_.setToggleState (configuration.enable_dnssd_node_discovery, juce::dontSendNotification);
        safeThis->enableSessionAdvertisement_.setToggleState (configuration.enable_dnssd_session_advertisement, juce::dontSendNotification);
        safeThis->enableSessionDiscovery_.setToggleState (configuration.enable_dnssd_session_discovery, juce::dontSendNotification);
    });
}

void SettingsMainComponent::updateNetworkInterfaces()
{
    const auto systemInterfaces = rav::NetworkInterfaceList::get_system_interfaces().get_interfaces();

    juce::Array<rav::NetworkInterface::Identifier> ids;

    primaryNetworkInterfaceComboBox_.clear();
    secondaryNetworkInterfaceComboBox_.clear();

    for (auto& iface : systemInterfaces)
    {
#if !RAV_LINUX
        // Linux doesn't really have a notion of ethernet types
        if (iface.get_type() != rav::NetworkInterface::Type::wired_ethernet)
            continue;
        // Linux doesn't give names to network interfaces
        if (iface.get_display_name().empty())
            continue;
#endif
        ids.add (iface.get_identifier());
        auto extended = iface.get_extended_display_name();
        primaryNetworkInterfaceComboBox_.addItem (extended, ids.size());
        secondaryNetworkInterfaceComboBox_.addItem (extended, ids.size());
    }

    networkInterfaces_ = std::move (ids);
}

void SettingsMainComponent::selectNetworkInterfaces() const
{
    rav::NetworkInterfaceConfig config;

    auto id = primaryNetworkInterfaceComboBox_.getSelectedId();
    RAV_ASSERT (id <= networkInterfaces_.size(), "Invalid id");
    if (id > 0)
        config.set_interface (rav::rank::primary, networkInterfaces_[id - 1]);

    id = secondaryNetworkInterfaceComboBox_.getSelectedId();
    RAV_ASSERT (id <= networkInterfaces_.size(), "Invalid id");
    if (id > 0)
        config.set_interface (rav::rank::secondary, networkInterfaces_[id - 1]);

    context_.getRavennaNode().set_network_interface_config (config).wait();
}

void SettingsMainComponent::saveDiagnosticsFile()
{
    diagnosticsFile_ = std::make_unique<DiagnosticsFile>();
    diagnosticsFile_->addLogfiles();
    diagnosticsFile_->addProjectAndBuildInfoFile();
    diagnosticsFile_->addUserInformationFile();
    diagnosticsFile_->addSystemInformationFile();
    diagnosticsFile_->addTextFile (context_.getApplicationStateJson(), "application_state_in_memory.json");
    diagnosticsFile_->addFile (context_.getApplicationStateFile(), {});

    diagnosticsFile_->saveDiagnosticsFile ([this] (const bool success, const juce::File& file) {
        if (success)
        {
            const auto options = juce::MessageBoxOptions()
                                     .withIconType (juce::MessageBoxIconType::InfoIcon)
                                     .withMessage ("Diagnostics file saved")
                                     .withButton ("Ok")
                                     .withButton ("Reveal");
            juce::NativeMessageBox::showAsync (options, [file] (const int btn) {
                if (btn == 1)
                    file.revealToUser();
            });
        }
        else
        {
            const auto options = juce::MessageBoxOptions()
                                     .withIconType (juce::MessageBoxIconType::WarningIcon)
                                     .withMessage ("There was an error saving the diagnostics file")
                                     .withButton ("Ok");
            juce::NativeMessageBox::showAsync (options, [file] (const int) {});
        }

        diagnosticsFile_.reset();
    });
}
