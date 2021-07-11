/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Constants.h"

//==============================================================================
SoftClippingPreampAudioProcessor::SoftClippingPreampAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    Settings settings = getSettings();
    makeConvolutionFilter(settings);
}

SoftClippingPreampAudioProcessor::~SoftClippingPreampAudioProcessor()
{
}

//==============================================================================
const juce::String SoftClippingPreampAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SoftClippingPreampAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SoftClippingPreampAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SoftClippingPreampAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SoftClippingPreampAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SoftClippingPreampAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SoftClippingPreampAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SoftClippingPreampAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SoftClippingPreampAudioProcessor::getProgramName (int index)
{
    return {};
}

void SoftClippingPreampAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

Coefficients makeMiddleBand(const Settings& settings, double sampleRate)
{
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                                990.f,
                                                                0.3,
                                                                juce::Decibels::decibelsToGain(settings.middle_gain * 10));
}

//==============================================================================
void SoftClippingPreampAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;

    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;

    auto settings = getSettings();

    leftProcessChain.reset();
    leftProcessChain.prepare(spec);
    rightProcessChain.reset();
    rightProcessChain.prepare(spec);


    makeAmplification(settings, ChainPositions::Input);

    auto lowPassFilter = makeClipperLowPass();
    updateCoefficients(leftProcessChain.get<ChainPositions::LowPass>().coefficients, lowPassFilter[0]); // it's a first order filter, so 0 must exist
    updateCoefficients(rightProcessChain.get<ChainPositions::LowPass>().coefficients, lowPassFilter[0]);

    makeWaveShaper(settings);
    leftProcessChain.get<ChainPositions::ToneStack>().reset();
    rightProcessChain.get<ChainPositions::ToneStack>().reset();

    auto lowPass2 = makeLowPass2(settings);
    updateCoefficients(leftProcessChain.get<ChainPositions::LowPass2>().coefficients, lowPass2[0]);
    updateCoefficients(rightProcessChain.get<ChainPositions::LowPass2>().coefficients, lowPass2[0]);

    auto highShelf = makeHighShelf(settings);
    updateCoefficients(leftProcessChain.get<ChainPositions::HighShelf>().coefficients, highShelf);
    updateCoefficients(rightProcessChain.get<ChainPositions::HighShelf>().coefficients, highShelf);
    leftProcessChain.setBypassed<ChainPositions::HighShelf>(true);
    rightProcessChain.setBypassed<ChainPositions::HighShelf>(true);

    auto coefficients = makeToneStackFilter(settings);
    updateCoefficients(leftProcessChain.get<ChainPositions::ToneStack>().coefficients, coefficients);
    updateCoefficients(rightProcessChain.get<ChainPositions::ToneStack>().coefficients, coefficients);
    leftProcessChain.get<ChainPositions::ToneStack>().reset();
    rightProcessChain.get<ChainPositions::ToneStack>().reset();

    makeAmplification(settings, ChainPositions::Volume);

    makeConvolutionFilter(settings);

    makeAmplification(settings, ChainPositions::Output);
}

void SoftClippingPreampAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SoftClippingPreampAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SoftClippingPreampAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto settings = getSettings();

    makeAmplification(settings, ChainPositions::Input);

    auto lowPassFilter = makeClipperLowPass();
    updateCoefficients(leftProcessChain.get<ChainPositions::LowPass>().coefficients, lowPassFilter[0]); // it's a first order filter, so 0 must exist
    updateCoefficients(rightProcessChain.get<ChainPositions::LowPass>().coefficients, lowPassFilter[0]);

    makeWaveShaper(settings);

    auto coefficients = makeToneStackFilter(settings);
    updateCoefficients(leftProcessChain.get<ChainPositions::ToneStack>().coefficients, coefficients);
    updateCoefficients(rightProcessChain.get<ChainPositions::ToneStack>().coefficients, coefficients);

    auto lowPass2 = makeLowPass2(settings);
    updateCoefficients(leftProcessChain.get<ChainPositions::LowPass2>().coefficients, lowPass2[0]);
    updateCoefficients(rightProcessChain.get<ChainPositions::LowPass2>().coefficients, lowPass2[0]);

    auto highShelf = makeHighShelf(settings);
    updateCoefficients(leftProcessChain.get<ChainPositions::HighShelf>().coefficients, highShelf);
    updateCoefficients(rightProcessChain.get<ChainPositions::HighShelf>().coefficients, highShelf);
    leftProcessChain.setBypassed<ChainPositions::HighShelf>(true);
    rightProcessChain.setBypassed<ChainPositions::HighShelf>(true);

    makeAmplification(settings, ChainPositions::Volume);

    //makeConvolutionFilter(settings);

    makeAmplification(settings, ChainPositions::Output);


    juce::dsp::AudioBlock<float> block(buffer);

    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    juce::dsp::ProcessContextReplacing<float> leftProcContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightProcContext(rightBlock);


    leftProcessChain.process(leftProcContext);
    rightProcessChain.process(rightProcContext);


}

//==============================================================================
bool SoftClippingPreampAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SoftClippingPreampAudioProcessor::createEditor()
{
    //return new SoftClippingPreampAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void SoftClippingPreampAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, true);
    m_apvts.state.writeToStream(mos);
}

void SoftClippingPreampAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);

    if (tree.isValid()) {
        m_apvts.replaceState(tree);

        Settings settings = getSettings();
        
        makeAmplification(settings, ChainPositions::Input);

        auto lowPassFilter = makeClipperLowPass();
        updateCoefficients(leftProcessChain.get<ChainPositions::LowPass>().coefficients, lowPassFilter[0]);
        updateCoefficients(rightProcessChain.get<ChainPositions::LowPass>().coefficients, lowPassFilter[0]);

        makeWaveShaper(settings);
        
        auto lowPass2 = makeLowPass2(settings);
        updateCoefficients(leftProcessChain.get<ChainPositions::LowPass2>().coefficients, lowPass2[0]);
        updateCoefficients(rightProcessChain.get<ChainPositions::LowPass2>().coefficients, lowPass2[0]);

        auto highShelf = makeHighShelf(settings);
        updateCoefficients(leftProcessChain.get<ChainPositions::HighShelf>().coefficients, highShelf);
        updateCoefficients(rightProcessChain.get<ChainPositions::HighShelf>().coefficients, highShelf);

        makeToneStackFilter(settings);
        leftProcessChain.get<ChainPositions::ToneStack>().reset();
        rightProcessChain.get<ChainPositions::ToneStack>().reset();

        makeAmplification(settings, ChainPositions::Volume);

        makeConvolutionFilter(settings);

        makeAmplification(settings, ChainPositions::Output);

    }
}

Settings SoftClippingPreampAudioProcessor::getSettings()
{
    Settings settings;

    // Input Volume
    settings.input_level = m_apvts.getRawParameterValue(Parameters::k_input_level)->load();

    // Drive
    settings.drive = m_apvts.getRawParameterValue(Parameters::k_drive)->load();

    // Low pass freq
    settings.low_pass_freq = m_apvts.getRawParameterValue(Parameters::k_low_pass_freq)->load();

    // High Shelf freq
    settings.high_shelf_freq = m_apvts.getRawParameterValue(Parameters::k_high_shelf_freq)->load();

    // High Shelf gain
    settings.high_shelf_gain = m_apvts.getRawParameterValue(Parameters::k_high_shelf_gain)->load();

    // High Shelf q
    settings.high_shelf_q = m_apvts.getRawParameterValue(Parameters::k_high_shelf_q)->load();

    // Bass
    settings.low_gain = m_apvts.getRawParameterValue(Parameters::k_bass)->load();

    // Mid
    settings.middle_gain = m_apvts.getRawParameterValue(Parameters::k_mid)->load();
    
    // Treble
    settings.treble_gain = m_apvts.getRawParameterValue(Parameters::k_treble)->load();
    
    // Volume
    settings.volume = m_apvts.getRawParameterValue(Parameters::k_volume)->load();

    // Output level
    settings.output_level = m_apvts.getRawParameterValue(Parameters::k_output_level)->load();

    return settings;
}

juce::AudioProcessorValueTreeState::ParameterLayout SoftClippingPreampAudioProcessor::CreateParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // input
    layout.add(std::make_unique<juce::AudioParameterFloat>(Parameters::k_input_level,
                                                           Parameters::k_input_level,
                                                           juce::NormalisableRange<float>(-50.f, 10.f, .1f),
                                                           0.f));

    // Drive
    layout.add(std::make_unique<juce::AudioParameterFloat>(Parameters::k_drive,
                                                           Parameters::k_drive,
                                                           juce::NormalisableRange<float>(10.851f, 300.234f),
                                                           11.f));

    // Low Pass Freq
    layout.add(std::make_unique<juce::AudioParameterFloat>(Parameters::k_low_pass_freq,
                                                           Parameters::k_low_pass_freq,
                                                           juce::NormalisableRange<float>(50.f, 1000.f, 1.f, 0.5f),
                                                           500.f));

    // High Shelf Freq
    layout.add(std::make_unique<juce::AudioParameterFloat>(Parameters::k_high_shelf_freq,
                                                           Parameters::k_high_shelf_freq,
                                                           juce::NormalisableRange<float>(1500.f, 15000.f, 1.f, 0.5f),
                                                           2000.f));

    // High Shelf Gain
    layout.add(std::make_unique<juce::AudioParameterFloat>(Parameters::k_high_shelf_gain,
                                                            Parameters::k_high_shelf_gain,
                                                            juce::NormalisableRange<float>(-24.f, 10.f, .1f, 0.25f),
                                                            0.f));

    // High Shelf Q
    layout.add(std::make_unique<juce::AudioParameterFloat>(Parameters::k_high_shelf_q,
                                                            Parameters::k_high_shelf_q,
                                                            juce::NormalisableRange<float>(0.5f, 5.f, .2f),
                                                            0.6f));

    // Bass
    layout.add(std::make_unique<juce::AudioParameterFloat>(Parameters::k_bass,
                                                           Parameters::k_bass,
                                                           juce::NormalisableRange<float>(0.001f, 0.999f, 0.001f, 0.25f),
                                                           0.5f));

    // Middle
    layout.add(std::make_unique<juce::AudioParameterFloat>(Parameters::k_mid,
                                                           Parameters::k_mid,
                                                           juce::NormalisableRange<float>(0.001f, 0.999f),
                                                           0.5f));

    // Treble
    layout.add(std::make_unique<juce::AudioParameterFloat>(Parameters::k_treble,
                                                           Parameters::k_treble,
                                                           juce::NormalisableRange<float>(0.001f, 0.999f, 0.01f),
                                                           0.5f));

    // Volume
    layout.add(std::make_unique<juce::AudioParameterFloat>(Parameters::k_volume,
                                                           Parameters::k_volume,
                                                           juce::NormalisableRange<float>(-30.f, 10.f, 1.f, .3f),
                                                           0.f));

    // Output Volume
    layout.add(std::make_unique<juce::AudioParameterFloat>(Parameters::k_output_level,
                                                           Parameters::k_output_level,
                                                           juce::NormalisableRange<float>(-50.f, 10.f, 1.f),
                                                           0.f));

    return layout;
}

juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> SoftClippingPreampAudioProcessor::makeClipperLowPass()
{
    return juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(350.484f, getSampleRate(), 1);
}

juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> SoftClippingPreampAudioProcessor::makeLowPass2(const Settings& settings)
{
    return juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(settings.low_pass_freq, getSampleRate(), 1);
}

Coefficients SoftClippingPreampAudioProcessor::makeHighShelf(const Settings& settings)
{
    return juce::dsp::IIR::Coefficients<float>::makeHighShelf(getSampleRate(), settings.high_shelf_freq, settings.high_shelf_q, juce::Decibels::gainToDecibels(settings.high_shelf_gain));
}

Coefficients SoftClippingPreampAudioProcessor::makeToneStackFilter(const Settings& settings)
{
    double l = (double)settings.low_gain;
    double m = (double)settings.middle_gain;
    double t = (double)settings.treble_gain;

    double c = 2 * getSampleRate(); // s <- c * ((1 - z^-1) / (1 + z^-1)). c <- 2/T. Taken from Jatin Chowdhury, ADC20 

    // few optimisations
    constexpr double c1r1 = C1 * R1;
    constexpr double c3r3 = C3 * R3;
    constexpr double c1r2Plusc2r2 = C1 * R2 + C2 * R2;
    constexpr double c1r3Plusc2r3 = C1 * R3 + C2 * R3;
    constexpr double c1c2r1r4Plusc1c3r1r4 = C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4;
    constexpr double c1c3r3r3Plusc2c3r3r3 = C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3;
    constexpr double c1c3r1r3Plusc1c3r3r3Plusc2c3r3r3 = C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3;
    constexpr double c1c2r1r2Plusc1c2r2r4Plusc1c3r2r4 = C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4;
    constexpr double c1c3r2r3Plusc2c3r2r3 = C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3;
    constexpr double c1c2r1r3Plusc1c2r3r4Plusc1c3r3r4 = C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4;
    constexpr double c1c2c3r1r2r3Plusc1c2c3r2r3r4 = C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4;
    constexpr double c1c2c3r1r3r3Plusc1c2c3r2r2r4 = C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R2 * R2 * R4;
    constexpr double c1c2c3r1r3r3Plusc1c2c3r3r3r4 = C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4;
    constexpr double c1c2c3r1r3r4 = C1 * C2 * C3 * R1 * R3 * R4;
    constexpr double c1c2c3r1r2r4 = C1 * C2 * C3 * R1 * R2 * R4;

    constexpr double c1r3 = C1 * R3;
    constexpr double c2r3 = C2 * R3;
    constexpr double c2r4 = C2 * R4;
    constexpr double c3r4 = C3 * R4;
    constexpr double a1_sum = c1r1 + c1r3 + c2r3 + c2r4 + c3r4;
    constexpr double c1c3r1r3 = C1 * C3 * R1 * R3;
    constexpr double c2c3r3r4 = C2 * C3 * R3 * R4;
    constexpr double c1c3r3r3 = C1 * C3 * R3 * R3;
    constexpr double c2c3r3r3 = C2 * C3 * R3 * R3;
    constexpr double m_a2_sum = c1c3r1r3 - c2c3r3r4 + c1c3r3r3 + c2c3r3r3;
    constexpr double l_a2_sum = C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4;
    constexpr double a2_sum_2 = C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4;
    constexpr double a3_lm_sum = C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4;
    constexpr double a3_mm_sum = C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4;
    constexpr double a3_m_sum = C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4;
    constexpr double c1c3c3r1r2r4 = C1 * C2 * C3 * R1 * R2 * R4;




    double b1 = (t * c1r1) + (m * c3r3)
                   + (l * c1r2Plusc2r2) + c1r3Plusc2r3;
    double b2 = t * c1c2r1r4Plusc1c3r1r4 -
                     (m * m) * c1c3r3r3Plusc2c3r3r3
                    + m * c1c3r1r3Plusc1c3r3r3Plusc2c3r3r3
                    + l * c1c2r1r2Plusc1c2r2r4Plusc1c3r2r4
                    + l * m * c1c3r2r3Plusc2c3r2r3
                    + c1c2r1r3Plusc1c2r3r4Plusc1c3r3r4;
    double b3 = l * m * c1c2c3r1r2r3Plusc1c2c3r2r3r4 -
                     m * m * c1c2c3r1r3r3Plusc1c2c3r3r3r4 +
                     m * c1c2c3r1r3r3Plusc1c2c3r3r3r4 +
                     t * c1c2c3r1r3r4 - t * m * c1c2c3r1r3r4 +
                     t * l * c1c2c3r1r2r4;

    double a0 = 1.0;
    double a1 = a1_sum
                 + m * c3r3 + l * c1r2Plusc2r2;
    double a2 = m * m_a2_sum + l * m * c1c3r2r3Plusc2c3r2r3
                - m * m * c1c3r3r3Plusc2c3r3r3 + l * l_a2_sum
                + a2_sum_2;
    double a3 = l * m * a3_lm_sum
                - m * m * a3_mm_sum
                + m * a3_m_sum + l * c1c2c3r1r2r4
                + c1c2c3r1r3r4;

    double B0 = -b1 * c - b2 * c * c - b3 * c * c * c;
    double B1 = -b1 * c + b2 * c * c + 3 * b3 * c * c * c;
    double B2 = b1 * c + b2 * c * c - 3 * b3 * c * c * c;
    double B3 = b1 * c - b2 * c * c + b3 * c * c * c;

    double A0 = -a0 - a1 * c - a2 * c * c - a3 * c * c * c;
    double A1 = -3 * a0 - a1 * c + a2 * c * c + 3 * a3 * c * c * c;
    double A2 = -3 * a0 + a1 * c + a2 * c * c - 3 * a3 * c * c * c;
    double A3 = -a0 + a1 * c - a2 * c * c + a3 * c * c * c;

    // https://ccrma.stanford.edu/~dtyeh/papers/yeh06_dafx.pdf Page 2

    return *new juce::dsp::IIR::Coefficients<float>((float)B0, (float)B1, (float)B2, (float)B3, (float)A0, (float)A1, (float)A2, (float)A3);
}

void SoftClippingPreampAudioProcessor::makeAmplification(const Settings& settings, const ChainPositions pos)
{
    switch (pos)
    {
    case Input:
        leftProcessChain.get<ChainPositions::Input>().setGainDecibels(settings.input_level);
        rightProcessChain.get<ChainPositions::Input>().setGainDecibels(settings.input_level);
    break;

    case Volume:
        leftProcessChain.get<ChainPositions::Volume>().setGainDecibels(settings.volume);
        rightProcessChain.get<ChainPositions::Volume>().setGainDecibels(settings.volume);
    break;

    case Output:
        leftProcessChain.get<ChainPositions::Output>().setGainDecibels(settings.output_level);
        rightProcessChain.get<ChainPositions::Output>().setGainDecibels(settings.output_level);
    break;

    default:
    break;
    }
}

void SoftClippingPreampAudioProcessor::makeWaveShaper(const Settings& settings)
{
    leftProcessChain.get<ChainPositions::Clipping>().functionToUse = [&settings] (float x) {
        return ( (2.f / juce::MathConstants<float>::pi) * std::atan(settings.drive * x) ) + x;
    };
    rightProcessChain.get<ChainPositions::Clipping>().functionToUse = [&settings](float x) {
        return ( (2.f / juce::MathConstants<float>::pi) * std::atan(settings.drive * x) ) + x;
    };
}

void SoftClippingPreampAudioProcessor::makeConvolutionFilter(const Settings& settings)
{

    if (irLoaded.compareAndSetBool(true, false)) {
        auto dir = juce::File::getCurrentWorkingDirectory();

        int numTries = 0;

        while (!dir.getChildFile("Resources").exists() && numTries++ < 15)
            dir = dir.getParentDirectory();

        auto& leftConvolution = leftProcessChain.template get<ChainPositions::Cabinet>();
        auto& rightConvolution = rightProcessChain.template get<ChainPositions::Cabinet>();
        leftConvolution.loadImpulseResponse(dir.getChildFile("Resources").getChildFile("Mesa Boogie Mark V.wav"),
                                            juce::dsp::Convolution::Stereo::no,
                                            juce::dsp::Convolution::Trim::no,
                                            0);
        rightConvolution.loadImpulseResponse(dir.getChildFile("Resources").getChildFile("Mesa Boogie Mark V.wav"),
                                             juce::dsp::Convolution::Stereo::no,
                                             juce::dsp::Convolution::Trim::no,
                                             0);
    }
}

void SoftClippingPreampAudioProcessor::updateCoefficients(Coefficients& old, const Coefficients& replacements)
{
    *old = *replacements;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SoftClippingPreampAudioProcessor();
}
