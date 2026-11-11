#pragma once
// Small JUCE-free DSP building blocks shared by the fuzz and the synth voice.
#include <algorithm>
#include <cmath>

namespace rc
{
constexpr float kPi = 3.14159265358979f;

inline float clampf (float x, float lo, float hi) { return std::min (hi, std::max (lo, x)); }
inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep (float e0, float e1, float x)
{
    const float t = clampf ((x - e0) / (e1 - e0), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

struct OnePoleLP
{
    float a = 0.f, z = 0.f;
    void setCutoff (float fc, float sr) { a = 1.f - std::exp (-2.f * kPi * fc / sr); }
    float process (float x) { z += a * (x - z); return z; }
    void reset() { z = 0.f; }
};

struct OnePoleHP
{
    OnePoleLP lp;
    void setCutoff (float fc, float sr) { lp.setCutoff (fc, sr); }
    float process (float x) { return x - lp.process (x); }
    void reset() { lp.reset(); }
};

// Peak-ish envelope follower with separate attack / release times.
struct EnvFollower
{
    float atk = 0.f, rel = 0.f, v = 0.f;
    void set (float attackSec, float releaseSec, float sr)
    {
        atk = 1.f - std::exp (-1.f / (attackSec * sr));
        rel = 1.f - std::exp (-1.f / (releaseSec * sr));
    }
    float process (float x)
    {
        const float a = std::fabs (x);
        v += (a > v ? atk : rel) * (a - v);
        return v;
    }
    void reset() { v = 0.f; }
};
} // namespace rc
