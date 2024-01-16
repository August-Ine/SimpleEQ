/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

struct ResponseCurveComponent :juce::Component, juce::AudioProcessorParameter::Listener,
    juce::Timer 
{
    ResponseCurveComponent(SimpleEQAudioProcessor&);
    ~ResponseCurveComponent();

    //juce::AudioProcessorParameter::Listener overrides
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {}; //EMPTY IMPLEMENTATION

    //juce::Timer override
    void timerCallback() override;

    void paint(juce::Graphics&) override;

private:
    SimpleEQAudioProcessor& audioProcessor;
    MonoChain monoChain;

    juce::Atomic<bool> parametersChanged{ false };
    
};
//==============================================================================

struct LookAndFeel :juce::LookAndFeel_V4
{
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
        float sliderPosProportional, float rotaryStartAngle,
        float rotaryEndAngle, juce::Slider&) override;
};


//==============================================================================
struct RotarySliderWithLabels : juce::Slider
{
    RotarySliderWithLabels(juce::RangedAudioParameter& rap, const juce::String& unitSuffix) :juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
        juce::Slider::TextEntryBoxPosition::NoTextBox), param(&rap), suffix(unitSuffix) 
    {
        setLookAndFeel(&lnf);
    }
    ~RotarySliderWithLabels()
    {
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override;

    juce::Rectangle<int> getSliderBounds() const;
    int getTextHeight() const { return 14; };
    juce::String getDisplayString() const
    {
        return juce::String{ "text" };
    };

private:
    juce::RangedAudioParameter* param;
    juce::String suffix;

    LookAndFeel lnf;
};

//==============================================================================
/**
*/
class SimpleEQAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    SimpleEQAudioProcessorEditor (SimpleEQAudioProcessor&);
    ~SimpleEQAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SimpleEQAudioProcessor& audioProcessor;

    RotarySliderWithLabels peakFreqSlider, 
        peakGainSlider,
        peakQualitySlider, 
        lowCutFreqSlider, 
        highCutFreqSlider,
        lowCutSlopeSlider, 
        highCutSlopeSlider;

    ResponseCurveComponent responseCurveComponent;

    //attachment aliases
    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;

    //slider Attachments
    Attachment peakFreqSliderAttachment, 
        peakGainSliderAttachment, 
        peakQualitySliderAttachment, 
        lowCutFreqSliderAttachment, 
        highCutFreqSliderAttachment,
        lowCutSlopeSliderAttachment, 
        highCutSlopeSliderAttachment;

    //helper to get editor components in a vec
    std::vector<juce::Component*> getComps();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleEQAudioProcessorEditor)
};
