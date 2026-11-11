#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

// Chrome-ringed, coloured-cap pedal knob with a printed 0-10 scale.
class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
};

// Big chrome stomp switch. Toggle state == bypassed.
class FootSwitch : public juce::Button
{
public:
    FootSwitch() : juce::Button ("bypass") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

class RoobarbEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RoobarbEditor (RoobarbAudioProcessor&);
    ~RoobarbEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void buildTextures();

    RoobarbAudioProcessor& audioProcessor;
    PedalLookAndFeel lnf;

    juce::Slider roobarbKnob, custardKnob;
    FootSwitch footSwitch;

    juce::AudioProcessorValueTreeState::SliderAttachment roobarbAttach, custardAttach;
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttach;

    juce::Image bodyTexture;
    juce::Rectangle<float> body, plate;
    int boilFrame = 0; // three-frame "boil", like hand-animated line work

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoobarbEditor)
};
