#pragma once
// Cheap monophonic pitch tracker for the Custard voice: filtered Schmitt-trigger
// zero-crossing period measurement, with a two-period agreement check so attack
// transients and octave glitches don't leak through.
#include "Filters.h"

namespace rc
{
class PitchTracker
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        hp.setCutoff (60.f, sr);
        env.set (0.002f, 0.05f, sr);
        reset();
    }

    void reset()
    {
        hp.reset(); lp1.reset(); lp2.reset(); env.reset();
        freq = 0.f; samplesSince = 0.f; lastPeriod = 0.f; stable = 0; state = false; voiced = false;
        setLowpass (500.f);
    }

    void process (float x)
    {
        const float s = lp2.process (lp1.process (hp.process (x)));
        const float e = env.process (s);
        const float th = std::max (0.25f * e, 0.0005f);
        voiced = e > 0.004f;

        samplesSince += 1.f;

        if (! state && s > th) // rising crossing
        {
            state = true;
            const float period = samplesSince;
            samplesSince = 0.f;

            if (period >= sr / 1000.f && period <= sr / 70.f)
            {
                if (lastPeriod > 0.f && std::fabs (period - lastPeriod) < 0.08f * lastPeriod)
                {
                    ++stable;
                    freq = sr / (0.5f * (period + lastPeriod));
                    setLowpass (clampf (freq * 1.8f, 150.f, 1000.f)); // follow the note to dodge harmonic errors
                }
                else
                    stable = 0;

                lastPeriod = period;
            }
            else
            {
                lastPeriod = 0.f;
                stable = 0;
            }
        }
        else if (state && s < -th)
            state = false;

        if (samplesSince > sr / 55.f) // nothing crossing: note has stopped
        {
            stable = 0;
            lastPeriod = 0.f;
        }
    }

    bool isStable() const { return voiced && stable >= 1 && freq > 0.f; }
    float frequency() const { return freq; }

private:
    void setLowpass (float fc) { lp1.setCutoff (fc, sr); lp2.setCutoff (fc, sr); }

    float sr = 48000.f, freq = 0.f, samplesSince = 0.f, lastPeriod = 0.f;
    int stable = 0;
    bool state = false, voiced = false;
    OnePoleHP hp;
    OnePoleLP lp1, lp2;
    EnvFollower env;
};
} // namespace rc
