
#include "PluginProcessor.h"
#include "PluginEditor.h"
FilterPluginAudioProcessor::FilterPluginAudioProcessor()
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
}

FilterPluginAudioProcessor::~FilterPluginAudioProcessor()
{
}
const juce::String FilterPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool FilterPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool FilterPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool FilterPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double FilterPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int FilterPluginAudioProcessor::getNumPrograms()
{
    return 1;
}

int FilterPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void FilterPluginAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String FilterPluginAudioProcessor::getProgramName (int index)
{
    return {};
}

void FilterPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}
void FilterPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;
    
    leftChain.prepare(spec);
    rightChain.prepare(spec);
    
    auto chainSettings = getChainSettings(apvts);
    
    updateFilters();
}

void FilterPluginAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FilterPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void FilterPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    updateFilters();
    
    juce::dsp::AudioBlock<float> block(buffer);
    
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);
    
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
    
    leftChain.process(leftContext);
    rightChain.process(rightContext);

}
bool FilterPluginAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* FilterPluginAudioProcessor::createEditor()
{
    return new FilterPluginAudioProcessorEditor(*this);
}

void FilterPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream moc(destData, true);
    apvts.state.writeToStream(moc);
}

void FilterPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if(tree.isValid()) {
        apvts.replaceState(tree);
        updateFilters();
    }
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts) {
    
    ChainSettings settings;
    
    settings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load();
    settings.highCutFreq = apvts.getRawParameterValue("HighCut Freq")->load();
    settings.peakFreq = apvts.getRawParameterValue("Peak Freq")->load();
    settings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load();
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());
    
    return settings;
}

void updateCoefficients(Coefficients& old, const Coefficients& replacements) {
    *old = *replacements;
}

template<int Index,typename ChainType, typename CoefficientType>
void FilterPluginAudioProcessor::update(ChainType& chain, const CoefficientType& coefficients) {
    
    updateCoefficients(chain.template get<Index>().coefficients, coefficients[Index]);
    chain.template setBypassed<Index>(false);
}

template<typename ChainType, typename CoefficientType>
void FilterPluginAudioProcessor::updateCutFilter(ChainType& leftLowCut,
                     const CoefficientType& cutCoefficients,
                     const Slope& lowCutSlope) {
    
    leftLowCut.template setBypassed<0>(true);
    leftLowCut.template setBypassed<1>(true);
    leftLowCut.template setBypassed<2>(true);
    leftLowCut.template setBypassed<3>(true);
    
    switch(lowCutSlope) {
        case Slope_48:
        {
            update<3>(leftLowCut, cutCoefficients);
        }
        case Slope_36:
        {
            update<2>(leftLowCut, cutCoefficients);
        }
        case Slope_24:
        {
            update<1>(leftLowCut, cutCoefficients);
        }
        case Slope_12:
        {
            update<0>(leftLowCut, cutCoefficients);
        }
    }
}

void FilterPluginAudioProcessor::updateLowCutFilters(const ChainSettings &chainSettings) {
    
    auto lowCutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
                                                                                                         getSampleRate(),
                                                                                                         (chainSettings.lowCutSlope + 1) * 2);
    
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();
    
    updateCutFilter(leftLowCut, lowCutCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(rightLowCut, lowCutCoefficients, chainSettings.lowCutSlope);
}

void FilterPluginAudioProcessor::updateHighCutFilters(const ChainSettings &chainSettings) {
    
    auto highCutCoefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq,
                                                                                                          getSampleRate(),
                                                                                                          (chainSettings.highCutSlope + 1) * 2);
    
    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();
    
    updateCutFilter(leftHighCut, highCutCoefficients, chainSettings.highCutSlope);
    updateCutFilter(rightHighCut, highCutCoefficients, chainSettings.highCutSlope);
}

Coefficients makePeakFilter(const ChainSettings& chainSettings, double sampleRate) {
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, chainSettings.peakFreq, chainSettings.peakQuality, juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
}

void FilterPluginAudioProcessor::updatePeakFilter(const ChainSettings& chainSettings) {
    
    auto peakCoefficients = makePeakFilter(chainSettings, getSampleRate());
    
    updateCoefficients(leftChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
}

void FilterPluginAudioProcessor::updateFilters() {
    
    auto chainSettings = getChainSettings(apvts);
    
    updateLowCutFilters(chainSettings);
    updateHighCutFilters(chainSettings);
    updatePeakFilter(chainSettings);
}

juce::AudioProcessorValueTreeState::ParameterLayout FilterPluginAudioProcessor::createParameterLayout() {
    
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq", "LowCut Freq", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), 20.0f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("HighCut Freq", "HighCut Freq", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), 20000.0f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Freq", "Peak Freq", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), 750.0f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Gain", "Peak Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.5f, 1.0f), 0.0f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Quality", "Peak Quality", juce::NormalisableRange<float>(0.1f, 10.0f, 0.05f, 1.0f), 1.0f));
    
    juce::StringArray stringArray;
    for(int i = 0; i < 4; i++){
        juce::String str;
        str << (12 + i * 12);
        str << " db/Oct";
        stringArray.add(str);
    }
    
    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope", "LowCut Slope", stringArray, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope", "HighCut Slope", stringArray, 0));

    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FilterPluginAudioProcessor();
}
