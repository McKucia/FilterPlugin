#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

struct LookAndFeel : juce::LookAndFeel_V4
{
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};

struct RotarySliderWithLabels: juce::Slider
{
public:
    RotarySliderWithLabels(juce::RangedAudioParameter& rap, const juce::String& unitSuffix): juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
        juce::Slider::TextEntryBoxPosition::NoTextBox),
        param(&rap),
        suffix(unitSuffix)
    {
        setLookAndFeel(&lf);
    }
    
    ~RotarySliderWithLabels()
    {
        setLookAndFeel(nullptr);
    }
    
    void paint(juce::Graphics& g) override;
    juce::Rectangle<int> getSliderBounds() const;
    int getTextHeight() const { return 14; }
    juce::String getDisplayString() const;
private:
    LookAndFeel lf;
    
    juce::RangedAudioParameter *param;
    juce::String suffix;
};

struct ResponsiveCurveComponent : public juce::Component,
juce::AudioProcessorParameter::Listener,
juce::Timer
{
public:
    ResponsiveCurveComponent(FilterPluginAudioProcessor&);
    ~ResponsiveCurveComponent();
    //Listener
    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override { };
    void timerCallback() override;
    void paint (juce::Graphics&) override;
    
private:
    FilterPluginAudioProcessor& audioProcessor;
    
    MonoChain monoChain;
    juce::Atomic<bool> parametersChanged { false };
};

class FilterPluginAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    FilterPluginAudioProcessorEditor (FilterPluginAudioProcessor&);
    ~FilterPluginAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FilterPluginAudioProcessor& audioProcessor;
    
    RotarySliderWithLabels peakFreqSlider,
    peakGainSlider,
    peakQualitySlider,
    lowCutFreqSlider,
    highCutFreqSlider,
    lowCutSlopeSlider,
    highCutSlopeSlider;
    
    ResponsiveCurveComponent responsiveCurveComponent;
    
    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;
    
    Attachment peakFreqSliderAttachment,
    peakGainSliderAttachment,
    peakQualitySliderAttachment,
    lowCutFreqSliderAttachment,
    highCutFreqSliderAttachment,
    lowCutSlopeSliderAttachment,
    highCutSlopeSliderAttachment;
    
    std::vector<juce::Component*> getComps();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterPluginAudioProcessorEditor)
};
