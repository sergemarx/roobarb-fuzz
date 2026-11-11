#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "ChirpVoice.h"
#include "FuzzStage.h"

class RoobarbAudioProcessor : public juce::AudioProcessor
{
public:
    RoobarbAudioProcessor();
    ~RoobarbAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Roobarb Fuzz"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.3; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

private:
    static constexpr int kChunk = 64;

    rc::FuzzStage fuzz;        // runs at 4x
    rc::ChirpVoice chirp;      // runs at the host rate
    juce::dsp::Oversampling<float> oversampler { 1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };

    // Everything that doesn't pass through the oversampler is delayed to line up with the fuzz.
    juce::dsp::DelayLine<float> chirpDelay { 4096 }, dryDelay { 4096 };

    juce::SmoothedValue<float> roobarbSmooth, custardSmooth, activeSmooth;
    std::atomic<float>* roobarbParam = nullptr;
    std::atomic<float>* custardParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    std::vector<float> dry, chirpOut, fuzzed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoobarbAudioProcessor)
};
