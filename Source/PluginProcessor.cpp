/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Multiband_compAudioProcessor::Multiband_compAudioProcessor()

#if 0 //ndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#endif
{
    magicState.setGuiValueTree(BinaryData::final_upam, BinaryData::final_upamSize);
   
    magicState.setApplicationSettingsFile(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile(ProjectInfo::companyName)
        .getChildFile(ProjectInfo::projectName + juce::String(".settings")));

    using namespace Params;
    const auto& params = GetParams();

    auto floatHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName) {

        param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);

    };

    floatHelper(lowBandComp.attack, Names::Attack_Low_Band);
    floatHelper(lowBandComp.release, Names::Release_Low_Band);
    floatHelper(lowBandComp.threshold, Names::Threshold_Low_Band);

    floatHelper(midBandComp.attack, Names::Attack_Mid_Band);
    floatHelper(midBandComp.release, Names::Release_Mid_Band);
    floatHelper(midBandComp.threshold, Names::Threshold_Mid_Band);

    floatHelper(highBandComp.attack, Names::Attack_High_Band);
    floatHelper(highBandComp.release, Names::Release_High_Band);
    floatHelper(highBandComp.threshold, Names::Threshold_High_Band);

    auto choiceHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName) {

        param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);

    };

    choiceHelper(lowBandComp.ratio, Names::Ratio_Low_Band);
    choiceHelper(midBandComp.ratio, Names::Ratio_Mid_Band);
    choiceHelper(highBandComp.ratio, Names::Ratio_High_Band);

    auto boolHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName) {

        param = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);

    };

    boolHelper(lowBandComp.bypassed, Names::Bypassed_Low_Band);
    boolHelper(midBandComp.bypassed, Names::Bypassed_Mid_Band);
    boolHelper(highBandComp.bypassed, Names::Bypassed_High_Band);

    boolHelper(lowBandComp.mute, Names::Mute_Low_Band);
    boolHelper(midBandComp.mute, Names::Mute_Mid_Band);
    boolHelper(highBandComp.mute, Names::Mute_High_Band);

    boolHelper(lowBandComp.solo, Names::Solo_Low_Band);
    boolHelper(midBandComp.solo, Names::Solo_Mid_Band);
    boolHelper(highBandComp.solo, Names::Solo_High_Band);

    floatHelper(lowMidCrossover, Names::Low_Mid_Crossover_Freq);
    floatHelper(midHighCrossover, Names::Mid_High_Crossover_Freq);

    floatHelper(inputGainParam, Names::Gain_In);
    floatHelper(outputGainParam, Names::Gain_Out);

    LP1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    HP1.setType(juce::dsp::LinkwitzRileyFilterType::highpass);

    AP2.setType(juce::dsp::LinkwitzRileyFilterType::allpass);

    LP2.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    HP2.setType(juce::dsp::LinkwitzRileyFilterType::highpass);

    //invAP1.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
    //invAP2.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
}

Multiband_compAudioProcessor::~Multiband_compAudioProcessor()
{
}

//==============================================================================
const juce::String Multiband_compAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool Multiband_compAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool Multiband_compAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool Multiband_compAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double Multiband_compAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Multiband_compAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int Multiband_compAudioProcessor::getCurrentProgram()
{
    return 0;
}

void Multiband_compAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String Multiband_compAudioProcessor::getProgramName(int index)
{
    return {};
}

void Multiband_compAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}

//==============================================================================
void Multiband_compAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();
    spec.sampleRate = sampleRate;

    for (auto& comp : compressor) {
        comp.prepare(spec);
    }


    LP1.prepare(spec);
    HP1.prepare(spec);

    AP2.prepare(spec);

    LP2.prepare(spec);
    HP2.prepare(spec);

    //invAP1.prepare(spec);
    //invAP2.prepare(spec);

    //invAPBuffer.setSize(spec.numChannels, samplesPerBlock);

    inputGain.prepare(spec);
    outputGain.prepare(spec);

    inputGain.setRampDurationSeconds(0.05); //50ms
    outputGain.setRampDurationSeconds(0.05);

    for (auto& buffer : filterBuffers) {
        buffer.setSize(spec.numChannels, samplesPerBlock);
    }
}

void Multiband_compAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Multiband_compAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
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

void Multiband_compAudioProcessor::updateState() {
    for (auto& cmp : compressor)
        cmp.updateCompressorSettings();

    auto lowMidCutoffFreq = lowMidCrossover->get();
    LP1.setCutoffFrequency(lowMidCutoffFreq);
    HP1.setCutoffFrequency(lowMidCutoffFreq);


    auto midHighCutoffFreq = midHighCrossover->get();
    AP2.setCutoffFrequency(midHighCutoffFreq);
    LP2.setCutoffFrequency(midHighCutoffFreq);
    HP2.setCutoffFrequency(midHighCutoffFreq);


    inputGain.setGainDecibels(inputGainParam->get());
    outputGain.setGainDecibels(outputGainParam->get());
}

void Multiband_compAudioProcessor::splitBands(const juce::AudioBuffer<float>& inputBuffer) {

    for (auto& fb : filterBuffers) {
        fb = inputBuffer;
    }

    auto fb0Block = juce::dsp::AudioBlock<float>(filterBuffers[0]);
    auto fb1Block = juce::dsp::AudioBlock<float>(filterBuffers[1]);
    auto fb2Block = juce::dsp::AudioBlock<float>(filterBuffers[2]);

    auto fb0Ctx = juce::dsp::ProcessContextReplacing<float>(fb0Block);
    auto fb1Ctx = juce::dsp::ProcessContextReplacing<float>(fb1Block);
    auto fb2Ctx = juce::dsp::ProcessContextReplacing<float>(fb2Block);

    LP1.process(fb0Ctx);
    AP2.process(fb0Ctx);

    HP1.process(fb1Ctx);
    filterBuffers[2] = filterBuffers[1];
    LP2.process(fb1Ctx);

    HP2.process(fb2Ctx);

}

void Multiband_compAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());


    updateState();

    applyGain(buffer, inputGain);


    splitBands(buffer);


    for (size_t i = 0; i < filterBuffers.size(); ++i) {
        compressor[i].process(filterBuffers[i]);
    }


    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();


    buffer.clear();

    auto addFilterBand = [nc = numChannels, ns = numSamples](auto& inputBuffer, const auto& source) {
        for (auto i = 0; i < nc; ++i) {
            inputBuffer.addFrom(i, 0, source, i, 0, ns);
        }
    };

    auto bandsAreSoloed = false;
    for (auto& comp : compressor) {
        if (comp.solo->get()) {
            bandsAreSoloed = true;
            break;
        }
    }


    if (bandsAreSoloed) {
        for (rsize_t i = 0; i < compressor.size(); ++i) {
            auto& comp = compressor[i];
            if (comp.solo->get()) {
                addFilterBand(buffer, filterBuffers[i]);
            }
        }
    }
    else {
        for (rsize_t i = 0; i < compressor.size(); ++i) {
            auto& comp = compressor[i];
            if (!comp.mute->get())
                addFilterBand(buffer, filterBuffers[i]);
        }
    }

    applyGain(buffer, outputGain);

}

//==============================================================================

bool Multiband_compAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}
/*
juce::AudioProcessorEditor* Multiband_compAudioProcessor::createEditor()
{
    return new foleys::MagicPluginEditor (magicState);
    //return new Multiband_compAudioProcessorEditor(*this);
}*/



//==============================================================================
void Multiband_compAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void Multiband_compAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.

    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid()) {
        apvts.replaceState(tree);
    }

}

juce::AudioProcessorValueTreeState::ParameterLayout Multiband_compAudioProcessor::createParameterLayout() {

    APVTS::ParameterLayout layout;
    using namespace juce;
    using namespace Params;
    const auto& params = GetParams();

    auto thresholdRange = NormalisableRange<float>(-60, 12, 1, 1);
    auto gainRange = NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.f);
    auto attackReleseRange = NormalisableRange<float>(5, 500, 1, 1);

    auto choices = std::vector<double>{ 1,1.5,2,3,4,5,6,7,8,10,15,20,50,100 };
    juce::StringArray sa;
    for (auto choice : choices) {
        sa.add(juce::String(choice, 1));
    }

    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::Gain_In),
        params.at(Names::Gain_In),
        gainRange,
        0));

    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::Gain_Out),
        params.at(Names::Gain_Out),
        gainRange,
        0));

    //LOW======================================================================================= 
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::Threshold_Low_Band),
        params.at(Names::Threshold_Low_Band),
        thresholdRange,
        0));

    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::Attack_Low_Band),
        params.at(Names::Attack_Low_Band),
        attackReleseRange,
        50));

    layout.add(std::make_unique < AudioParameterFloat>(
        params.at(Names::Release_Low_Band),
        params.at(Names::Release_Low_Band),
        attackReleseRange,
        250));

    layout.add(std::make_unique<AudioParameterChoice>(
        params.at(Names::Ratio_Low_Band),
        params.at(Names::Ratio_Low_Band),
        sa,
        3));

    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::Bypassed_Low_Band),
        params.at(Names::Bypassed_Low_Band),
        false));

    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::Mute_Low_Band),
        params.at(Names::Mute_Low_Band),
        false));

    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::Solo_Low_Band),
        params.at(Names::Solo_Low_Band),
        false));

    //MID======================================================================================= 
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::Threshold_Mid_Band),
        params.at(Names::Threshold_Mid_Band),
        thresholdRange,
        0));
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::Attack_Mid_Band),
        params.at(Names::Attack_Mid_Band),
        attackReleseRange,
        50));
    layout.add(std::make_unique < AudioParameterFloat>(
        params.at(Names::Release_Mid_Band),
        params.at(Names::Release_Mid_Band),
        attackReleseRange,
        250));
    layout.add(std::make_unique<AudioParameterChoice>(
        params.at(Names::Ratio_Mid_Band),
        params.at(Names::Ratio_Mid_Band),
        sa,
        3));
    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::Bypassed_Mid_Band),
        params.at(Names::Bypassed_Mid_Band),
        false));
    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::Mute_Mid_Band),
        params.at(Names::Mute_Mid_Band),
        false));
    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::Solo_Mid_Band),
        params.at(Names::Solo_Mid_Band),
        false));

    //HIGH======================================================================================= 
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::Threshold_High_Band),
        params.at(Names::Threshold_High_Band),
        thresholdRange,
        0));
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::Attack_High_Band),
        params.at(Names::Attack_High_Band),
        attackReleseRange,
        50));
    layout.add(std::make_unique < AudioParameterFloat>(
        params.at(Names::Release_High_Band),
        params.at(Names::Release_High_Band),
        attackReleseRange,
        250));
    layout.add(std::make_unique<AudioParameterChoice>(
        params.at(Names::Ratio_High_Band),
        params.at(Names::Ratio_High_Band),
        sa,
        3));
    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::Bypassed_High_Band),
        params.at(Names::Bypassed_High_Band),
        false));
    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::Mute_High_Band),
        params.at(Names::Mute_High_Band),
        false));
    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::Solo_High_Band),
        params.at(Names::Solo_High_Band),
        false));


    //FREKVENCE======================================================================================= 
    layout.add(std::make_unique <AudioParameterFloat>(
        params.at(Names::Low_Mid_Crossover_Freq),
        params.at(Names::Low_Mid_Crossover_Freq),
        NormalisableRange<float>(20, 999, 1, 1),
        400));

    layout.add(std::make_unique <AudioParameterFloat>(
        params.at(Names::Mid_High_Crossover_Freq),
        params.at(Names::Mid_High_Crossover_Freq),
        NormalisableRange<float>(1000, 20000, 1, 1),
        2000));


    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Multiband_compAudioProcessor();
}