
#include "PluginProcessor.h"
#include "PluginEditor.h"
FilterPluginAudioProcessorEditor::FilterPluginAudioProcessorEditor (FilterPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
    peakFreqSliderAttachment(audioProcessor.apvts, "Peak Freq", peakFreqSlider),
    peakGainSliderAttachment(audioProcessor.apvts, "Peak Gain", peakGainSlider),
    peakQualitySliderAttachment(audioProcessor.apvts, "Peak Quality", peakQualitySlider),
    lowCutFreqSliderAttachment(audioProcessor.apvts, "LowCut Freq", lowCutFreqSlider),
    highCutFreqSliderAttachment(audioProcessor.apvts, "HighCut Freq", highCutFreqSlider),
    lowCutSlopeSliderAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlopeSlider),
    highCutSlopeSliderAttachment(audioProcessor.apvts, "HighCut Slope", highCutSlopeSlider)
{
    for(auto* comp : getComps()) {
        addAndMakeVisible(comp);
    }
    
    setSize (600, 400);
}

FilterPluginAudioProcessorEditor::~FilterPluginAudioProcessorEditor()
{
}

void FilterPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (Colours::black);
    
    auto bounds = getLocalBounds();
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    
    auto w = responseArea.getWidth();
    
    auto& lowCut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highCut = monoChain.get<ChainPositions::HighCut>();
    
    std::vector<float> mags;
    mags.resize(w);
    auto sampleRate = audioProcessor.getSampleRate();
    
    //w - szerokosc obszaru na której będzie linia
    for(int i = 0; i < w; i++) {
        double mag = 1.0f;
        //zamieniamy szerokosc obszaru w 1/100 na czestotliwosci
        auto freq = mapToLog10((double)i / (double)w, 20.0, 20000.0);
        
        if(!monoChain.isBypassed<ChainPositions::Peak>()) {
            mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);
            std::cout << mag << std::endl;
        }
    }

    g.setColour (juce::Colours::white);
}

void FilterPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    
    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto rightCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);
    
    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(lowCutArea.getHeight() * 0.5));
    highCutFreqSlider.setBounds(rightCutArea.removeFromTop(rightCutArea.getHeight() * 0.5));
    
    lowCutSlopeSlider.setBounds(lowCutArea);
    highCutSlopeSlider.setBounds(rightCutArea);
    
    peakFreqSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.33));
    peakGainSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.5));
    peakQualitySlider.setBounds(bounds);
}

std::vector<juce::Component*> FilterPluginAudioProcessorEditor::getComps()
{
    return {
        &peakFreqSlider,
        &peakGainSlider,
        &peakQualitySlider,
        &lowCutFreqSlider,
        &highCutFreqSlider,
        &lowCutSlopeSlider,
        &highCutSlopeSlider
    };
}

