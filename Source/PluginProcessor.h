/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/

using Filter = juce::dsp::IIR::Filter<float>;
using Coefficients = Filter::CoefficientsPtr;
using Gain = juce::dsp::Gain<float>;
using Dist = juce::dsp::WaveShaper<float, std::function<float(float)>>;
using Convolution = juce::dsp::Convolution;

struct Settings
{
    float low_gain { 0 }, middle_gain { 0 }, treble_gain { 0 };
    float low_pass_freq { 0 }, high_shelf_freq { 0 }, high_shelf_gain { 0 }, high_shelf_q { 0 };
    float drive { 0 }, volume { 0 };
    float input_level { 0 }, output_level { 0 };
};

class SoftClippingPreampAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    SoftClippingPreampAudioProcessor();
    ~SoftClippingPreampAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    Settings getSettings();

    static juce::AudioProcessorValueTreeState::ParameterLayout CreateParameterLayout();
    juce::AudioProcessorValueTreeState m_apvts{ *this, nullptr, "Parameters", CreateParameterLayout() };

private:
    juce::Atomic<bool> irLoaded { false };

    enum ChainPositions 
    {
        Input,
        LowPass,
        Clipping,
        LowPass2,
        HighShelf,
        ToneStack,
        Volume,
        Cabinet,
        Output
    };

    using ProcessChain = juce::dsp::ProcessorChain<Gain, Filter, Dist, Filter, Filter, Filter, Gain, Convolution, Gain>;
    
    ProcessChain leftProcessChain, rightProcessChain;

    juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> makeClipperLowPass();
    juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> makeLowPass2(const Settings&);
    Coefficients makeHighShelf(const Settings&);
    Coefficients makeToneStackFilter(const Settings& settings);
    void makeAmplification(const Settings& settings, const ChainPositions pos);
    void makeWaveShaper(const Settings& settings);
    void makeConvolutionFilter(const Settings& settings);

    void updateCoefficients(Coefficients& old, const Coefficients& replacements);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoftClippingPreampAudioProcessor)
};
