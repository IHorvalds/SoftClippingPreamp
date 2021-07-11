/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class SoftClippingPreampAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    SoftClippingPreampAudioProcessorEditor (SoftClippingPreampAudioProcessor&);
    ~SoftClippingPreampAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SoftClippingPreampAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoftClippingPreampAudioProcessorEditor)
};
