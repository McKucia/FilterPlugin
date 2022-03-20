
#include "PluginProcessor.h"
#include "PluginEditor.h"

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                    float sliderPosProportional, float rotaryStartAngle,
                                    float rotaryEndAngle, juce::Slider& slider)
{
    using namespace juce;
    
    auto bounds = Rectangle<float>(x, y, width, height);
    
    g.setColour(Colours::white);
    g.fillEllipse(bounds);
    
    g.setColour(Colours::green);
    g.drawEllipse(bounds, 2.0f);
    
    //jeżeli nasz slider jest rotary with labels
    if( auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        auto center = bounds.getCentre();
        Path p;
        
        Rectangle<float> r;
        r.setLeft(center.getX() - 3);
        r.setRight(center.getX() + 3);
        r.setTop(bounds.getY());
        r.setBottom(center.getY() - rswl->getTextHeight() * 1.5);
        
        p.addRoundedRectangle(r, 2.0f);
        
        jassert(rotaryEndAngle > rotaryStartAngle);
        auto sliderAngRad = jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);
        
        p.applyTransform(AffineTransform().rotation(sliderAngRad, center.getX(), center.getY()));
        
        g.fillPath(p);
        
        g.setFont(rswl->getTextHeight());
        
        auto text = rswl->getDisplayString();
        auto strWidth = g.getCurrentFont().getStringWidth(text);
        
        r.setSize(strWidth + 4, rswl->getTextHeight() + 2);
        r.setCentre(bounds.getCentre());
        
        g.setColour(Colours::black);
        g.fillRect(r);
        
        g.setColour(Colours::white);
        g.drawFittedText(text, r.toNearestInt(), Justification::centred, 1);
    }
}

juce::String RotarySliderWithLabels::getDisplayString() const
{
    //sprawdzamy czy param jest parametrem typu choice
    if( auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param) )
        return choiceParam->getCurrentChoiceName();
    
    juce::String str;
    bool addK = false;
    
    if( auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param) )
    {
        auto value = getValue();
        
        if(value > 999.0f) {
            value /= 1000.0f;
            addK = true;
        }
        
        str = juce::String(value, (addK ? 2 : 0));
    }
    else
    {
        jassertfalse;
    }
    
    if(suffix.isNotEmpty())
    {
        str << " ";
        if(addK)
            str << "k";
        str << suffix;
    }
    
    return str;
}

void RotarySliderWithLabels::paint(juce::Graphics& g)
{
    using namespace juce;
    
    const auto startAngle = degreesToRadians(180.0 + 45.0);
    const auto endAngle = degreesToRadians(180.0 - 45.0) + MathConstants<float>::twoPi;
    
    auto range = getRange();
    auto bounds = getSliderBounds();
    
    getLookAndFeel().drawRotarySlider(g,
                                      bounds.getX(),
                                      bounds.getY(),
                                      bounds.getWidth(),
                                      bounds.getHeight(),
                                      jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0),
                                      startAngle,
                                      endAngle,
                                      *this);
    
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const
{
    using namespace juce;
    
    auto bounds = getLocalBounds();
    auto size = jmin(getWidth(), getHeight());
    size -= getTextHeight() * 1.4;
    Rectangle<int> r(size, size);
    r.setCentre(bounds.getCentreX(), 0);
    r.setY(2);
    
    return r;
}

ResponsiveCurveComponent::ResponsiveCurveComponent(FilterPluginAudioProcessor& p) : audioProcessor(p)
{
    const auto& params = audioProcessor.getParameters();
    for(const auto& param : params) {
        param->addListener(this);
    }
    startTimerHz(60);
}

ResponsiveCurveComponent::~ResponsiveCurveComponent()
{
    const auto& params = audioProcessor.getParameters();
    for(const auto& param : params) {
        param->removeListener(this);
    }
}

void ResponsiveCurveComponent::parameterValueChanged (int parameterIndex, float newValue) {
    parametersChanged.set(true);
}

void ResponsiveCurveComponent::timerCallback() {
    if(parametersChanged.compareAndSetBool(false, true)) {
        //aktualizacja monochain
        auto chainSettings = getChainSettings(audioProcessor.apvts);
        auto peakCoefficients = makePeakFilter(chainSettings, audioProcessor.getSampleRate());
        updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
        
        auto lowCutCoefficients = makeLowCutFilter(chainSettings, audioProcessor.getSampleRate());
        auto highCutCoefficients = makeHighCutFilter(chainSettings, audioProcessor.getSampleRate());
        updateCutFilter(monoChain.get<ChainPositions::LowCut>(), lowCutCoefficients, chainSettings.lowCutSlope);
        updateCutFilter(monoChain.get<ChainPositions::HighCut>(), highCutCoefficients, chainSettings.highCutSlope);
        
        repaint();
    }
}

void ResponsiveCurveComponent::paint(juce::Graphics &g)
{
    using namespace juce;
    g.fillAll (Colours::black);
    
    auto responseArea = getLocalBounds();
    
    auto w = responseArea.getWidth();
    
    auto& lowCut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highCut = monoChain.get<ChainPositions::HighCut>();
    
    std::vector<float> mags;
    mags.resize(w);
    auto sampleRate = audioProcessor.getSampleRate();
    
    //w - szerokosc obszaru na którym będzie linia
    for(int i = 0; i < w; i++) {
        double mag = 1.0f;
        //zamieniamy szerokosc obszaru w 1/100 na czestotliwosci
        auto freq = mapToLog10((double)i / (double)w, 20.0, 20000.0);
        
        if(!monoChain.isBypassed<ChainPositions::Peak>())
            mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);
        
        if(!lowCut.isBypassed<0>())
            mag *= lowCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if(!lowCut.isBypassed<1>())
            mag *= lowCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if(!lowCut.isBypassed<2>())
            mag *= lowCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if(!lowCut.isBypassed<3>())
            mag *= lowCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        
        if(!highCut.isBypassed<0>())
            mag *= highCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if(!highCut.isBypassed<1>())
            mag *= highCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if(!highCut.isBypassed<2>())
            mag *= highCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if(!highCut.isBypassed<3>())
            mag *= highCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        
        mags[i] = Decibels::gainToDecibels(mag);
    }
    
    Path responseCurve;
    
    const double outputMin = responseArea.getBottom();
    const double outputMax = responseArea.getY();
    auto map = [outputMin, outputMax](double input) {
        return jmap(input, -24.0, 24.0, outputMin, outputMax);
    };
    
    responseCurve.startNewSubPath(responseArea.getX(), map(mags.front()));
    
    for(size_t i = 0; i < mags.size(); i++) {
        responseCurve.lineTo(responseArea.getX() + i, map(mags[i]));
    }
    
    g.setColour(Colours::green);
    g.drawRoundedRectangle(responseArea.toFloat(), 4.0f, 1.0f);
    
    g.setColour(Colours::white);
    g.strokePath(responseCurve, PathStrokeType(2.0f));
    
    g.setColour (juce::Colours::white);
}

FilterPluginAudioProcessorEditor::FilterPluginAudioProcessorEditor (FilterPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
    peakFreqSlider(*audioProcessor.apvts.getParameter("Peak Freq"), "Hz"),
    peakGainSlider(*audioProcessor.apvts.getParameter("Peak Gain"), "dB"),
    peakQualitySlider(*audioProcessor.apvts.getParameter("Peak Quality"), ""),
    lowCutFreqSlider(*audioProcessor.apvts.getParameter("LowCut Freq"), "Hz"),
    lowCutSlopeSlider(*audioProcessor.apvts.getParameter("LowCut Slope"), "db/Oct"),
    highCutFreqSlider(*audioProcessor.apvts.getParameter("HighCut Freq"), "Hz"),
    highCutSlopeSlider(*audioProcessor.apvts.getParameter("HighCut Slope"), "db/Oct"),

    responsiveCurveComponent(audioProcessor),
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
}

void FilterPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    
    responsiveCurveComponent.setBounds(responseArea);
    
    bounds.setBounds(bounds.getX(), bounds.getY() + 10, bounds.getWidth(), bounds.getHeight());
    
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
        &highCutSlopeSlider,
        &responsiveCurveComponent
    };
}

