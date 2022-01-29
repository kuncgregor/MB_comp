/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Multiband_compAudioProcessor::Multiband_compAudioProcessor()
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
    using namespace Params;
    const auto& params = GetParams();

    auto floatHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName) {

        param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);

    };

    floatHelper(compressor.attack, Names::Attack_Low_Band);
    floatHelper(compressor.release, Names::Release_Low_Band);
    floatHelper(compressor.threshold, Names::Threshold_Low_Band);

    auto choiceHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName) {

        param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);

    };

    choiceHelper(compressor.ratio, Names::Ratio_Low_Band);

    auto boolHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName) {

        param = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);

    };

    boolHelper(compressor.bypassed, Names::Bypassed_Low_Band);
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

void Multiband_compAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String Multiband_compAudioProcessor::getProgramName (int index)
{
    return {};
}

void Multiband_compAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void Multiband_compAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();
    spec.sampleRate = sampleRate;

    compressor.prepare(spec);
}

void Multiband_compAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Multiband_compAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void Multiband_compAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    compressor.updateCompressorSettings();
    compressor.process(buffer);
}

//==============================================================================
bool Multiband_compAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* Multiband_compAudioProcessor::createEditor()
{
    return new Multiband_compAudioProcessorEditor (*this);
}

//==============================================================================
void Multiband_compAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void Multiband_compAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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

    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Threshold_Low_Band),params.at(Names::Threshold_Low_Band),
                                                    NormalisableRange<float>(-60, 12, 1, 1),
                                                    0));

    auto attackReleseRange = NormalisableRange<float>(5, 500, 1, 1);

    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Attack_Low_Band),
        params.at(Names::Attack_Low_Band),
                                                    attackReleseRange, 
                                                    50));

    layout.add(std::make_unique < AudioParameterFloat>(params.at(Names::Release_Low_Band),
        params.at(Names::Release_Low_Band),
                                                        attackReleseRange,
                                                        250));

    auto choices = std::vector<double>{ 1,1.5,2,3,4,5,6,7,8,10,15,20,50,100 };
    juce::StringArray sa;
    for (auto choice : choices) {
        sa.add(juce::String(choice, 1));
    }

    layout.add(std::make_unique<AudioParameterChoice>(params.at(Names::Ratio_Low_Band),
        params.at(Names::Ratio_Low_Band),
        sa,
        3));

    layout.add(std::make_unique<AudioParameterBool>(params.at(Names::Bypassed_Low_Band),
        params.at(Names::Bypassed_Low_Band),
        false));

    

    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Multiband_compAudioProcessor();
}
