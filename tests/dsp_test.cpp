// JUCE-free sanity tests for the DSP. Build: g++ -std=c++17 -O2 -I Source/dsp tests/dsp_test.cpp
#include "ChirpVoice.h"
#include "FuzzStage.h"

#include <cstdio>
#include <vector>

using namespace rc;

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("FAIL: "); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

// Plucked-string-ish test tone: harmonics fall as 1/n, upper ones decay faster.
static std::vector<float> pluck (float f, float sr, float seconds, float amp = 0.3f)
{
    std::vector<float> out ((size_t) (seconds * sr));
    for (size_t i = 0; i < out.size(); ++i)
    {
        const float t = (float) i / sr;
        float s = 0.f;
        for (int n = 1; n <= 12; ++n)
            s += std::sin (2.f * kPi * f * (float) n * t) / (float) n * std::exp (-t * (0.8f + 0.9f * (float) n));
        out[i] = amp * s * (1.f - std::exp (-t * 800.f));
    }
    return out;
}

static float goertzel (const float* x, size_t n, float f, float sr)
{
    const float w = 2.f * kPi * f / sr, c = 2.f * std::cos (w);
    float s1 = 0.f, s2 = 0.f;
    for (size_t i = 0; i < n; ++i) { const float s = x[i] + c * s1 - s2; s2 = s1; s1 = s; }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (float) n;
}

int main()
{
    const float sr = 48000.f;

    // --- Pitch tracker --------------------------------------------------------------
    const float notes[] = { 82.4f, 110.f, 146.8f, 196.f, 246.9f, 329.6f, 440.f, 587.3f };
    for (float f : notes)
    {
        PitchTracker t;
        t.prepare (sr);
        auto sig = pluck (f, sr, 0.5f);
        size_t okSamples = 0, checked = 0;
        for (size_t i = 0; i < sig.size(); ++i)
        {
            t.process (sig[i]);
            if (i > (size_t) (0.15f * sr) && t.isStable()) { ++checked; if (std::fabs (t.frequency() - f) < 0.03f * f) ++okSamples; }
        }
        const float pct = checked ? 100.f * (float) okSamples / (float) checked : 0.f;
        std::printf ("tracker %6.1f Hz: %5.1f%% of stable samples within 3%% (%zu locked)\n", f, pct, checked);
        CHECK (checked > (size_t) (0.1f * sr), "tracker never locked on %.1f Hz", f);
        CHECK (pct > 90.f, "tracker accuracy %.1f%% on %.1f Hz", pct, f);
    }

    // --- Custard voice --------------------------------------------------------------
    for (float f : { 110.f, 196.f, 329.6f })
    {
        ChirpVoice v;
        v.prepare (sr);
        v.setAmount (0.7f);
        auto sig = pluck (f, sr, 0.6f);
        std::vector<float> out (sig.size());
        float peak = 0.f;
        for (size_t i = 0; i < sig.size(); ++i)
        {
            out[i] = v.process (sig[i]);
            CHECK (std::isfinite (out[i]), "non-finite chirp output");
            peak = std::max (peak, std::fabs (out[i]));
        }
        const size_t a = (size_t) (0.2f * sr), n = (size_t) (0.2f * sr);
        const float atF = goertzel (&out[a], n, f, sr), at2F = goertzel (&out[a], n, 2.f * f, sr);
        std::printf ("chirp  in %6.1f Hz: peak %.3f, energy@f %.4f, energy@2f %.4f\n", f, peak, atF, at2F);
        CHECK (peak > 0.02f && peak < 1.0f, "chirp peak %.3f out of range", peak);
        CHECK (at2F > 2.f * atF, "chirp not dominated by the octave above (%.4f vs %.4f)", at2F, atF);
    }

    // Custard at zero must be silent.
    {
        ChirpVoice v; v.prepare (sr); v.setAmount (0.f);
        auto sig = pluck (196.f, sr, 0.3f);
        float peak = 0.f;
        for (float s : sig) peak = std::max (peak, std::fabs (v.process (s)));
        CHECK (peak < 1.0e-6f, "custard at 0 not silent: %.6f", peak);
    }

    // --- Fuzz -----------------------------------------------------------------------
    const float osr = sr * 4.f;
    {
        FuzzStage fz; fz.prepare (osr); fz.setAmount (0.f);
        auto sig = pluck (196.f, osr, 0.2f);
        float maxDiff = 0.f;
        for (float s : sig) maxDiff = std::max (maxDiff, std::fabs (fz.process (s) - s));
        std::printf ("fuzz at 0: max |out-in| = %g\n", maxDiff);
        CHECK (maxDiff < 1.0e-6f, "fuzz at 0 is not transparent");
    }
    for (float amt : { 0.25f, 0.6f, 1.0f })
    {
        FuzzStage fz; fz.prepare (osr); fz.setAmount (amt);
        auto sig = pluck (196.f, osr, 1.0f, 0.4f);
        std::vector<float> out (sig.size());
        float peak = 0.f;
        for (size_t i = 0; i < sig.size(); ++i)
        {
            out[i] = fz.process (sig[i]);
            CHECK (std::isfinite (out[i]), "non-finite fuzz output");
            peak = std::max (peak, std::fabs (out[i]));
        }
        // Harmonic richness around the sustained part.
        const size_t a = (size_t) (0.05f * osr), n = (size_t) (0.2f * osr);
        const float f1 = goertzel (&out[a], n, 196.f, osr);
        float harm = 0.f;
        for (int h = 2; h <= 8; ++h) harm += std::pow (goertzel (&out[a], n, 196.f * (float) h, osr), 2.f);
        const float ratio = std::sqrt (harm) / std::max (f1, 1.0e-9f);
        std::printf ("fuzz amt %.2f: peak %.3f, harmonics/fundamental %.2f\n", amt, peak, ratio);
        CHECK (peak < 1.0f, "fuzz peak %.3f too hot", peak);
        CHECK (amt < 0.5f || ratio > 0.25f, "fuzz at %.2f not harmonically rich enough", amt);
    }

    // Starved-battery gating: with a quiet input the bias should push the stage towards
    // cut-off, so level drops much faster than the input does (expander-like sputter).
    // Below the threshold of the effect (low Roobarb) the fuzz should keep sustaining.
    {
        auto gainDb = [&] (float amt, float inAmp)
        {
            FuzzStage fz; fz.prepare (osr); fz.setAmount (amt);
            const size_t n = (size_t) (0.6f * osr);
            double acc = 0; size_t cnt = 0;
            for (size_t i = 0; i < n; ++i)
            {
                const float s = inAmp * std::sin (2.f * kPi * 196.f * (float) i / osr);
                const float o = fz.process (s);
                if (i > n / 2) { acc += (double) o * o; ++cnt; }
            }
            return 20.0 * std::log10 (std::sqrt (acc / (double) cnt) / (inAmp * 0.7071) + 1e-12);
        };
        const double hiMax = gainDb (1.0f, 0.2f), loMax = gainDb (1.0f, 0.002f);
        const double hiLow = gainDb (0.3f, 0.2f), loLow = gainDb (0.3f, 0.002f);
        std::printf ("starve @1.0: gain at 0.2 = %.1f dB, at 0.002 = %.1f dB (drop %.1f dB)\n", hiMax, loMax, hiMax - loMax);
        std::printf ("starve @0.3: gain at 0.2 = %.1f dB, at 0.002 = %.1f dB (drop %.1f dB)\n", hiLow, loLow, hiLow - loLow);
        CHECK ((hiMax - loMax) > (hiLow - loLow) + 12.0, "full Roobarb does not gate harder than low Roobarb");
    }

    std::printf (failures ? "\n%d FAILURE(S)\n" : "\nall DSP checks passed\n", failures);
    return failures ? 1 : 0;
}
