#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout RoobarbAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "roobarb", 1 }, "Roobarb (Fuzz)",
                                                       NormalisableRange<float> (0.f, 1.f, 0.001f), 0.55f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "custard", 1 }, "Custard (Chirp)",
                                                       NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

RoobarbAudioProcessor::RoobarbAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    roobarbParam = apvts.getRawParameterValue ("roobarb");
    custardParam = apvts.getRawParameterValue ("custard");
    bypassParam = apvts.getRawParameterValue ("bypass");
}

bool RoobarbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo())
           && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void RoobarbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto blockSize = juce::jmax (samplesPerBlock, kChunk);

    oversampler.reset();
    oversampler.initProcessing ((size_t) kChunk);

    fuzz.prepare ((float) sampleRate * 4.f);
    chirp.prepare ((float) sampleRate);

    const float latency = oversampler.getLatencyInSamples();
    setLatencySamples (juce::roundToInt (latency));

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
    for (auto* d : { &chirpDelay, &dryDelay })
    {
        d->prepare (spec);
        d->setMaximumDelayInSamples (256);
        d->setDelay (latency);
        d->reset();
    }

    roobarbSmooth.reset (sampleRate, 0.03);
    custardSmooth.reset (sampleRate, 0.03);
    activeSmooth.reset (sampleRate, 0.015);
    roobarbSmooth.setCurrentAndTargetValue (roobarbParam->load());
    custardSmooth.setCurrentAndTargetValue (custardParam->load());
    activeSmooth.setCurrentAndTargetValue (bypassParam->load() > 0.5f ? 0.f : 1.f);

    dry.assign ((size_t) blockSize, 0.f);
    chirpOut.assign ((size_t) blockSize, 0.f);
    fuzzed.assign ((size_t) blockSize, 0.f);
}

// Mono in, dual-mono out: it's a guitar pedal. Stereo input is summed.
void RoobarbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    roobarbSmooth.setTargetValue (roobarbParam->load());
    custardSmooth.setTargetValue (custardParam->load());
    activeSmooth.setTargetValue (bypassParam->load() > 0.5f ? 0.f : 1.f);

    if ((int) dry.size() < numSamples)
    {
        dry.resize ((size_t) numSamples);
        chirpOut.resize ((size_t) numSamples);
        fuzzed.resize ((size_t) numSamples);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        float s = 0.f;
        for (int ch = 0; ch < numIn; ++ch)
            s += buffer.getReadPointer (ch)[i];
        dry[(size_t) i] = numIn > 0 ? s / (float) numIn : 0.f;
    }

    for (int start = 0; start < numSamples; start += kChunk)
    {
        const int n = juce::jmin (kChunk, numSamples - start);

        const float r = roobarbSmooth.skip (n);
        const float c = custardSmooth.skip (n);
        fuzz.setAmount (r);
        chirp.setAmount (c);

        // Custard (host rate), then delay to match the oversampler's latency.
        for (int i = 0; i < n; ++i)
        {
            const float v = chirp.process (dry[(size_t) (start + i)]);
            chirpDelay.pushSample (0, v);
            chirpOut[(size_t) (start + i)] = chirpDelay.popSample (0);
        }

        // Roobarb (4x oversampled).
        std::copy_n (dry.begin() + start, n, fuzzed.begin() + start);
        float* chans[] = { fuzzed.data() + start };
        juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
        auto up = oversampler.processSamplesUp (block);
        auto* u = up.getChannelPointer (0);
        for (size_t k = 0; k < up.getNumSamples(); ++k)
            u[k] = fuzz.process (u[k]);
        oversampler.processSamplesDown (block);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        dryDelay.pushSample (0, dry[(size_t) i]);
        const float delayedDry = dryDelay.popSample (0);
        const float active = activeSmooth.getNextValue();

        const float wetSum = fuzzed[(size_t) i] + chirpOut[(size_t) i];
        const float out = active * wetSum + (1.f - active) * delayedDry;

        for (int ch = 0; ch < numOut; ++ch)
            buffer.getWritePointer (ch)[i] = out;
    }
}

juce::AudioProcessorEditor* RoobarbAudioProcessor::createEditor() { return new RoobarbEditor (*this); }

void RoobarbAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void RoobarbAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RoobarbAudioProcessor(); }
