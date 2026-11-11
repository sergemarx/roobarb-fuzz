#pragma once
// CUSTARD: the jaunty, chipper top voice.
//
// The theme pairs the fuzzy low line with a bright, high-pitched synth tone. Here a
// pitch-tracked PolyBLEP pulse oscillator plays an octave above the guitar (with a
// quieter second octave on top), gated by the playing so it stays staccato and bouncy.
// Turning Custard up also narrows the pulse, opens the filter and shortens the decay.
#include "Filters.h"
#include "PitchTracker.h"

namespace rc
{
class ChirpVoice
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        tracker.prepare (sr);
        env.set (0.001f, 0.06f, sr);
        hp.setCutoff (380.f, sr);
        glide = 1.f - std::exp (-1.f / (0.012f * sr));
        atkCoeff = 1.f - std::exp (-1.f / (0.002f * sr));
        reset();
        setAmount (0.f);
    }

    void reset()
    {
        tracker.reset(); env.reset(); hp.reset(); lp1.reset(); lp2.reset();
        amp = 0.f; freq = 0.f; targetFreq = 0.f;
        phase1 = phase2 = lfoPhase = pwPhase = 0.f;
    }

    // amount: 0..1 (the Custard dial)
    void setAmount (float c)
    {
        amount = clampf (c, 0.f, 1.f);
        level = 0.5f * std::pow (amount, 1.3f);
        pulseW = 0.5f - 0.2f * amount;
        const float fc = lerpf (3200.f, 7500.f, amount);
        lp1.setCutoff (fc, sr);
        lp2.setCutoff (fc, sr);
        const float relSec = lerpf (0.20f, 0.07f, amount); // more Custard = more staccato
        relCoeff = 1.f - std::exp (-1.f / (relSec * sr));
    }

    float process (float x)
    {
        tracker.process (x);
        const float e = env.process (x);
        const bool on = tracker.isStable();

        const float target = on ? std::sqrt (clampf (e * 25.f, 0.f, 1.f)) : 0.f;
        amp += (target > amp ? atkCoeff : relCoeff) * (target - amp);

        if (on)
            targetFreq = tracker.frequency() * 2.f; // octave up
        if (freq <= 0.f)
            freq = targetFreq;
        else
            freq += glide * (targetFreq - freq);

        if (amp < 1.0e-4f || freq <= 0.f)
            return 0.f;

        lfoPhase = wrap (lfoPhase + 5.3f / sr);
        pwPhase = wrap (pwPhase + 0.8f / sr);
        const float vib = 1.f + 0.004f * amount * std::sin (2.f * kPi * lfoPhase);
        const float f = clampf (freq * vib, 60.f, 3500.f);

        const float dt1 = f / sr, dt2 = 2.f * f / sr;
        phase1 = wrap (phase1 + dt1);
        phase2 = wrap (phase2 + dt2);

        const float pw = pulseW + 0.07f * std::sin (2.f * kPi * pwPhase);
        float s = 0.7f * pulse (phase1, dt1, pw) + 0.3f * pulse (phase2, dt2, 0.5f);
        s = lp2.process (lp1.process (hp.process (s)));
        return s * amp * level;
    }

private:
    static float wrap (float p) { return p - std::floor (p); }

    static float polyBlep (float t, float dt)
    {
        if (t < dt)       { t /= dt; return t + t - t * t - 1.f; }
        if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }

    static float pulse (float phase, float dt, float pw)
    {
        float v = phase < pw ? 1.f : -1.f;
        v += polyBlep (phase, dt);
        v -= polyBlep (wrap (phase - pw + 1.f), dt);
        return v;
    }

    float sr = 48000.f, amount = 0.f, level = 0.f, pulseW = 0.5f;
    float glide = 0.f, atkCoeff = 0.f, relCoeff = 0.f;
    float amp = 0.f, freq = 0.f, targetFreq = 0.f;
    float phase1 = 0.f, phase2 = 0.f, lfoPhase = 0.f, pwPhase = 0.f;
    PitchTracker tracker;
    EnvFollower env;
    OnePoleHP hp;
    OnePoleLP lp1, lp2;
};
} // namespace rc
