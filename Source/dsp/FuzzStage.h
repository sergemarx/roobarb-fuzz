#pragma once
// ROOBARB: the fuzz.
//
// Voicing notes (from research on the 1974 theme):
//  * Forum consensus is a fuzzed synth line (or very tight, clipped guitar) - so the stage
//    is tight in the lows, mid-forward, and chops the top off for a "scuzzy" rather than
//    fizzy top end.
//  * The recurring description of the fuzz is the "velcro ripping" sound of a starved /
//    nearly dead battery in a germanium Fuzz Face or Big Muff. We model that with a bias
//    that drifts as the input envelope falls, so decaying notes gate and sputter.
//
// Designed to run inside a 4x oversampler: construct with the *oversampled* sample rate.
#include "Filters.h"

namespace rc
{
class FuzzStage
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        hpIn.setCutoff (110.f, sr);   // keeps low-end tight before the clipper
        dcBlock.setCutoff (30.f, sr); // asymmetric clipping makes DC
        env.set (0.001f, 0.09f, sr);
        midLo.setCutoff (500.f, sr);
        midHi.setCutoff (2200.f, sr);
        reset();
        setAmount (0.f);
    }

    void reset()
    {
        hpIn.reset(); dcBlock.reset(); env.reset();
        lp1.reset(); lp2.reset(); midLo.reset(); midHi.reset();
    }

    // amount: 0..1 (the Roobarb dial)
    void setAmount (float r)
    {
        amount = clampf (r, 0.f, 1.f);
        wet = clampf (amount * 12.f, 0.f, 1.f);                     // clean at 0, fuzz fully in by ~8%
        preGain = std::pow (10.f, (8.f + amount * 40.f) / 20.f);    // +8 dB .. +48 dB
        starve = smoothstep (0.35f, 1.f, amount);                   // sputter only on high settings
        const float fc = lerpf (6800.f, 3600.f, amount);
        lp1.setCutoff (fc, sr);
        lp2.setCutoff (fc * 1.15f, sr);
        midBoost = lerpf (0.4f, 0.9f, amount);
    }

    float process (float x)
    {
        const float in = hpIn.process (x);

        // Dying-battery behaviour: as the envelope falls the bias creeps up and the
        // clipper starts to gate / spit.
        const float gate = clampf (env.process (in) * 12.f, 0.f, 1.f);
        const float sag = starve * (1.f - gate);
        const float bias = 0.08f + 3.5f * sag;

        const float v = in * preGain + bias;

        // Asymmetric clipping: hard-ish on the positive half, softer and smaller on the negative.
        float y = v >= 0.f ? std::tanh (1.7f * v) : 0.78f * std::tanh (1.25f * v);
        y = y / (1.f + 0.25f * std::fabs (y));
        y = dcBlock.process (y);

        // Scuzzy top-end roll-off plus a forward mid hump.
        const float f = lp2.process (lp1.process (y));
        const float mid = midHi.process (f) - midLo.process (f);
        const float out = (f + midBoost * mid) * 0.42f;

        return x * (1.f - wet) + out * wet;
    }

private:
    float sr = 48000.f, amount = 0.f, wet = 0.f, preGain = 1.f, starve = 0.f, midBoost = 0.f;
    OnePoleHP hpIn, dcBlock;
    EnvFollower env;
    OnePoleLP lp1, lp2, midLo, midHi;
};
} // namespace rc
