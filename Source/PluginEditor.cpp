/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPosProportional, float rotaryStartAngle,
    float rotaryEndAngle, juce::Slider& slider) 
{
    using namespace juce;
    
    //get bounding box
    auto bounds = Rectangle<float>(x, y, width, height);
    //circle fill
    g.setColour(Colour(97u, 18u, 167u));
    g.fillEllipse(bounds);
    //circle border
    g.setColour(Colour(255u, 154u, 1u));
    g.drawEllipse(bounds, 1.f);

    //assert type
    if (auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        //knob thumb rectangle
        auto centre = bounds.getCentre();
        Path p;
        Rectangle<float> r;

        r.setLeft(centre.getX() - 2);
        r.setRight(centre.getX() + 2);
        r.setTop(bounds.getY());
        r.setBottom(centre.getY() - rswl->getTextHeight()*1.5);

        p.addRoundedRectangle(r, 2.f);
        //make sure angle is ok
        jassert(rotaryStartAngle < rotaryEndAngle);

        //map slider's normalised value to radian angle
        auto sliderAngRad = jmap(sliderPosProportional, 0.f, 1.f, rotaryStartAngle, rotaryEndAngle);
        //rotate thumb rectangle about component's centre
        p.applyTransform(AffineTransform().rotated(sliderAngRad, centre.getX(), centre.getY()));

        //draw
        g.fillPath(p);

        //value text
        g.setFont(rswl->getTextHeight()); //default font at that height
        auto text = rswl->getDisplayString();
        auto stringWidth = g.getCurrentFont().getStringWidth(text);
        
        r.setSize(stringWidth + 4, rswl->getTextHeight() + 2);
        r.setCentre(bounds.getCentre());

        g.setColour(Colours::black);
        g.fillRect(r);

        g.setColour(Colours::white);
        g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
    }   
}

//==============================================================================

void RotarySliderWithLabels::paint(juce::Graphics& g)
{
    using namespace juce;
    
    //0 deg is at 12 o'clock position
    auto startAng = degreesToRadians(180.f + 45.f); //7 o'clock
    auto endAng = degreesToRadians(180.f - 45.f) + MathConstants<float>::twoPi; //5 o'clock

    auto range = getRange();

    auto sliderBounds = getSliderBounds();

    //debugging draw slider bounds & local bounds
    /*g.setColour(Colours::red);
    g.drawRect(getLocalBounds());
    g.setColour(Colours::yellow);
    g.drawRect(sliderBounds);*/

    getLookAndFeel().drawRotarySlider(g, sliderBounds.getX(), sliderBounds.getY(), sliderBounds.getWidth(),
        sliderBounds.getHeight(), jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0), startAng, endAng, *this);

    //labels
    auto centre = sliderBounds.toFloat().getCentre();
    auto radius = sliderBounds.getWidth() * 0.5f;

    g.setColour(Colour(0u, 172u, 1u));
    g.setFont(getTextHeight());

    auto numChoices = labels.size();
    for (int i = 0; i < numChoices; ++i)
    {
        auto pos = labels[i].pos;
        //make sure it's normalised
        jassert(0.f <= pos);
        jassert(pos <= 1.f);
        //map to rads
        auto ang = jmap(pos, 0.f, 1.f, startAng, endAng);

        auto c = centre.getPointOnCircumference(radius + getTextHeight() * 0.5f + 1, ang);

        Rectangle<float> r;
        auto str = labels[i].label;
         
        r.setSize(g.getCurrentFont().getStringWidth(str), getTextHeight());
        r.setCentre(c);
        //move down a little just in case
        r.setY(r.getY() + getTextHeight());
        g.drawFittedText(str, r.toNearestInt(), juce::Justification::centred, 1);
    }
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const
{
    auto bounds =  getLocalBounds();
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight()); //get smallest

    //text padding
    size -= getTextHeight() * 2;

    juce::Rectangle<int> r;
    r.setSize(size, size);
    //place top-centre
    r.setCentre(bounds.getCentreX(), 0);
    r.setY(2);

    return r;
}

juce::String RotarySliderWithLabels::getDisplayString() const
{
    //choice params
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param))
        return choiceParam->getCurrentChoiceName();
    //gain, freq params
    juce::String str;
    bool addK = false;

    if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
    {
        float val = getValue();
        if (val > 999.f)
        {
            val /= 1000.f;
            addK = true;
        }
        str = juce::String(val, (addK ? 2 : 0));
    }
    else
    {
        jassertfalse; // this shouldn't happen
    }

    if (suffix.isNotEmpty())
    {
        str << " "; // space
        if (addK)
            str << "k";
        str << suffix;
    }

    return str;
}

//==============================================================================

ResponseCurveComponent::ResponseCurveComponent(SimpleEQAudioProcessor& p): audioProcessor(p)
{
    //listen to apvts param updates
    const auto& params = audioProcessor.getParameters();
    for (auto param : params) {
        param->addListener(this);
    }

    //Atomic var boolean check timer
    startTimerHz(60);
}
ResponseCurveComponent::~ResponseCurveComponent()
{
    //deregister as listener to apvts parameter updates
    const auto& params = audioProcessor.getParameters();
    for (auto param : params) {
        param->removeListener(this);
    }
}

void ResponseCurveComponent::parameterValueChanged(int parameterIndex, float newValue) {
    parametersChanged.set(true);
}

void ResponseCurveComponent::timerCallback() {
    if (parametersChanged.compareAndSetBool(false, true)) {
        //update monochain
        auto chainSettings = getChainSettings(audioProcessor.apvts);

        //peak 
        auto peakCoefficients = makePeakFilter(chainSettings, audioProcessor.getSampleRate());
        updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);

        //cut filters
        auto lowCutCoefficients = makeLowCutFilter(chainSettings, audioProcessor.getSampleRate());
        auto highCutCoefficients = makeHighCutFilter(chainSettings, audioProcessor.getSampleRate());
        updateCutFilter(monoChain.get < ChainPositions::LowCut>(), lowCutCoefficients, chainSettings.lowCutSlope);
        updateCutFilter(monoChain.get<ChainPositions::HighCut>(), highCutCoefficients, chainSettings.highCutSlope);

        //signal a repaint of responseCurve
        repaint();
    }
}

void ResponseCurveComponent::paint(juce::Graphics& g)
{
    using namespace juce;
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(Colours::black);

    auto responseArea = getLocalBounds();
    auto width = responseArea.getWidth();

    //get chain elements
    auto& lowCut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highCut = monoChain.get<ChainPositions::HighCut>();

    auto sampleRate = audioProcessor.getSampleRate();

    std::vector<double> mags;
    //preallocate space 
    mags.resize(width);

    //calculate magnittude for each pixel
    for (int i = 0; i < width; ++i) {
        double mag = 1.f;

        //map normalised pixel number to its frequency in human hearing range
        auto freq = mapToLog10(double(i) / double(width), 20.0, 20'000.0);
        //get magnitude for pixel frequency if the band is not bypassed
        //peak filter
        if (!monoChain.isBypassed<ChainPositions::Peak>())
            mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);

        //lowCut filter
        if (!lowCut.isBypassed<0>())
            mag *= lowCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!lowCut.isBypassed<1>())
            mag *= lowCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!lowCut.isBypassed<2>())
            mag *= lowCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!lowCut.isBypassed<3>())
            mag *= lowCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);

        //highCut filter
        if (!highCut.isBypassed<0>())
            mag *= highCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!highCut.isBypassed<1>())
            mag *= highCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!highCut.isBypassed<2>())
            mag *= highCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!highCut.isBypassed<3>())
            mag *= highCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);

        //convert mag to decibels and store
        mags[i] = Decibels::gainToDecibels(mag);
    }

    //build path
    Path responseCurve;

    //min and max window positions
    const double outputMin = responseArea.getBottom();
    const double outputMax = responseArea.getY();

    //map decibel mag to responseArea
    auto map = [outputMin, outputMax](double input) {
        return jmap<double>(input, -24, 24, outputMin, outputMax);
        };

    //start new sub-path from left edge with the first magnitude
    responseCurve.startNewSubPath(responseArea.getX(), map(mags.front()));
    //create line-tos for every other magnitude
    for (size_t i = 1; i < mags.size(); ++i) {
        responseCurve.lineTo(responseArea.getX() + i, map(mags[i]));
    };

    //orange border
    g.setColour(Colours::orange);
    g.drawRoundedRectangle(responseArea.toFloat(), 4.f, 1.f);

    //draw response curve path
    g.setColour(Colours::white);
    g.strokePath(responseCurve, PathStrokeType(2.f));
}

//==============================================================================

SimpleEQAudioProcessorEditor::SimpleEQAudioProcessorEditor (SimpleEQAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
    peakFreqSlider(*audioProcessor.apvts.getParameter("Peak Freq"), "Hz"),
    peakGainSlider(*audioProcessor.apvts.getParameter("Peak Gain"), "dB"),
    peakQualitySlider(*audioProcessor.apvts.getParameter("Peak Quality"), ""),
    lowCutFreqSlider(*audioProcessor.apvts.getParameter("LowCut Freq"), "Hz"),
    highCutFreqSlider(*audioProcessor.apvts.getParameter("HighCut Freq"), "Hz"),
    lowCutSlopeSlider(*audioProcessor.apvts.getParameter("LowCut Slope"), "dB/Oct"),
    highCutSlopeSlider(*audioProcessor.apvts.getParameter("HighCut Slope"), "dB/Oct"),
    responseCurveComponent(audioProcessor), 
    peakFreqSliderAttachment(audioProcessor.apvts, "Peak Freq", peakFreqSlider), 
    peakGainSliderAttachment(audioProcessor.apvts, "Peak Gain", peakGainSlider),
    peakQualitySliderAttachment(audioProcessor.apvts, "Peak Quality", peakQualitySlider),
    lowCutFreqSliderAttachment(audioProcessor.apvts, "LowCut Freq", lowCutFreqSlider),
    highCutFreqSliderAttachment(audioProcessor.apvts, "HighCut Freq", highCutFreqSlider),
    lowCutSlopeSliderAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlopeSlider),
    highCutSlopeSliderAttachment(audioProcessor.apvts, "HighCut Slope", highCutSlopeSlider)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    peakFreqSlider.labels.add({ 0.f, "20Hz" });
    peakFreqSlider.labels.add({ 1.f, "20kHz" });

    for (auto* comp : getComps()) {
        addAndMakeVisible(comp);
    }

    setSize (600, 400);
}

SimpleEQAudioProcessorEditor::~SimpleEQAudioProcessorEditor()
{
}

//==============================================================================
void SimpleEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (Colours::black);
}

void SimpleEQAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    
    auto bounds = getLocalBounds();
    
    //response curve
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    responseCurveComponent.setBounds(responseArea);

    //sliders
    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);

    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(lowCutArea.getHeight()*0.5));
    lowCutSlopeSlider.setBounds(lowCutArea);

    highCutFreqSlider.setBounds(highCutArea.removeFromTop(highCutArea.getHeight() * 0.5));
    highCutSlopeSlider.setBounds(highCutArea);

    peakFreqSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.33));
    peakGainSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.5));
    peakQualitySlider.setBounds(bounds);
}

std::vector<juce::Component*> SimpleEQAudioProcessorEditor::getComps() {
    return{
        &peakFreqSlider,
         &peakGainSlider,
         &peakQualitySlider,
         &lowCutFreqSlider,
         &highCutFreqSlider,
         &lowCutSlopeSlider,
         &highCutSlopeSlider,
         &responseCurveComponent,
    };
};
