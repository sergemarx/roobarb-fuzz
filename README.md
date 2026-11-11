# Roobarb Fuzz (the "Scuzz-O-Tone")

A 70s-style fuzz-and-chirp guitar pedal as a VST3 / AU / standalone plugin, built with
[JUCE](https://juce.com). It's a love letter to the sound of a certain 1974 cartoon theme
(composed by Johnny Hawksworth for de Wolfe Music): a snarling fuzzed low line paired with a
chipper, high-pitched synth voice. No artwork or audio from the show is used.

Two dials, one switch:

| Control | What it does |
|---|---|
| **Roobarb** (fuzz) | 0 = clean. Turning up adds an asymmetric, mid-forward, tight-bottomed fuzz. Past ~1/3 it adds the "starved battery" behaviour: as notes die away the bias drifts and the fuzz gates and sputters (the "velcro ripping" sound pedal-heads describe). |
| **Custard** (jaunty) | 0 = off. Adds a pitch-tracked pulse-wave synth voice one octave above your playing (plus a quieter second octave). More Custard = narrower pulse, brighter filter, shorter and bouncier decay. |
| **Footswitch** | True bypass (latency-aligned), red LED when engaged. |

Mono in, dual-mono out. Stereo input is summed. Play single notes: Custard is monophonic.

## Where the sound comes from (research notes)

- Theme by Johnny Hawksworth (de Wolfe Music), first aired on BBC1 on 21 Oct 1974; ~21 seconds long.
- What made the fuzz line is debated. Forum listeners variously hear a clipped, slightly wah-like
  guitar or a **synth line through a fuzz** (one player reports a Moog into a fuzz pedal sounded
  "virtually identical" with some EQ). Roobarb is therefore a *tight, clipped, mid-heavy* fuzz that
  works on guitar or synth.
- Pedal-heads on the EHX forums describe the fuzz as the "velcro ripping" sound of a **starved /
  nearly dead battery** in a germanium Big Muff or Fuzz Face. That behaviour is modelled explicitly.
- The top voice is described as a chipper higher-pitched tone, so Custard is a bright, narrow-ish
  pulse wave with fast attack and staccato decay.

Honest caveat: these are listening-based descriptions, not a schematic or stem analysis. Use your
ears with the original open next to the plugin and nudge the constants (all in `Source/dsp/`).

## Build

### On GitHub (recommended)

1. Create a repo and push this folder:
   ```
   git init && git add . && git commit -m "Roobarb Fuzz"
   git branch -M main
   git remote add origin https://github.com/<you>/roobarb-fuzz.git
   git push -u origin main
   ```
2. Open the **Actions** tab. The `Build` workflow runs the DSP tests, then builds Windows, macOS
   (universal) and Linux VST3s. Download them from the run's **Artifacts** section.

### Locally

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
JUCE 8.0.6 is fetched automatically. The built plugin is in `build/RoobarbPedal_artefacts/Release/`.

DSP tests only (no JUCE, no GUI toolkit needed):
```
cmake -S . -B build-tests -DRC_BUILD_PLUGIN=OFF
cmake --build build-tests --target dsp_test && ./build-tests/dsp_test
```

## Layout

```
Source/dsp/Filters.h        tiny one-pole filters / envelope follower
Source/dsp/FuzzStage.h      Roobarb: asymmetric clipper + starved-battery bias + voicing
Source/dsp/PitchTracker.h   zero-crossing tracker used by Custard
Source/dsp/ChirpVoice.h     Custard: PolyBLEP pulse voice
Source/PluginProcessor.*    JUCE wrapper, 4x oversampling around the fuzz, latency alignment
Source/PluginEditor.*       the pedal: hammered enamel, chrome knobs, 3-frame "boiling" ink outline
tests/dsp_test.cpp          sanity checks on tracker accuracy, octave-up, fuzz behaviour
```

## Notes

- macOS builds are unsigned; Gatekeeper will complain until you sign/notarise them.
- The plugin name references a BBC cartoon. Fine for personal use; if you plan to release it
  commercially, pick your own name and check trademarks. The in-plugin title is already an
  original ("Scuzz-O-Tone"), and `PRODUCT_NAME` / `COMPANY_NAME` in `CMakeLists.txt` are yours to change.
