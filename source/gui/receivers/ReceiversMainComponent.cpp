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

#include "ReceiversMainComponent.hpp"

ReceiversMainComponent::ReceiversMainComponent (ApplicationContext& context) :
    streamsContainer_ (context),
    miniDeviceSelector_ (context.getAudioDeviceManager(), MiniAudioDeviceSelectorComponent::Direction::Output)
{
    viewport_.setViewedComponent (&streamsContainer_, false);
    viewport_.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport_);

    addAndMakeVisible (miniDeviceSelector_);
}

void ReceiversMainComponent::paint (juce::Graphics&) {}

void ReceiversMainComponent::resized()
{
    auto b = getLocalBounds();

    miniDeviceSelector_.setBounds (b.removeFromBottom (50).reduced (10));

    viewport_.setBounds (b);
    streamsContainer_.setSize (viewport_.getWidth(), 10);
    streamsContainer_.resizeToFitContent();
}
