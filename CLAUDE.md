# Spectral Fault — instructions for Claude Code

## Project

Spectral Fault by Catastrophic Audio: hobby parametric EQ plugin (AU, VST3, Standalone) for macOS, loosely
inspired by commercial high-end EQs.
Learning project, built with JUCE and C++ in CLion. Public repo, licensed AGPLv3.

- Full plan, milestones and design options: `docs/PLAN.md`
- Current status, decisions made, session log: `docs/PROGRESS.md`
- Developer guide (build, test, architecture): `docs/DEVELOPMENT.md`; user guide web page: `docs/guide/index.html`
  (GitHub Pages from `/docs`), screenshots in `docs/images/` (regenerate with
  `EQ_SCREENSHOT_DIR=docs/images build/tests/SpectralFaultTests "[.screenshots]"`). Keep README, DEVELOPMENT.md and
  the user guide in step with user-visible changes; `python3 tools/check_docs.py` checks their links.

Read both at the start of every session.

## How to work in this repo

- Work only on the milestone named in my prompt. Its "done when" criteria are in `docs/PLAN.md`.
- Before editing, propose a plan: files to add or change, tests to write, commands you will run. Wait for my OK.
- Items under "Open decisions" below are mine to make. Do not pick one silently: lay out the options
  with their trade-offs and ask.
- Do not add third-party dependencies without asking.
- When a milestone step is done: build, run all tests, run pluginval, then update `docs/PROGRESS.md`
  (milestone status + a short session log entry) and summarise what changed.
- Small, focused commits. Message format: `M<n>: <what changed>` (e.g. `M1: add peaking band response test`).

## Build and test (CMake)

Project generation is CMake (`juce_add_plugin`); Projucer is not used.

```bash
# configure + build (Debug)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# run unit tests (Catch2, in tests/)
ctest --test-dir build --output-on-failure

# validate the plugin (pluginval is not on PATH; run it from the app bundle)
/Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 5 --validate "build/SpectralFault_artefacts/Debug/VST3/Spectral Fault.vst3"
/Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 5 --validate "$HOME/Library/Audio/Plug-Ins/Components/Spectral Fault.component"

# AU validation (the build copies the AU to ~/Library/Audio/Plug-Ins/Components)
auval -v aufx Peq1 Ctcd

# look at the editor without a host: renders PNGs of the editor into <dir>
EQ_SNAPSHOT_DIR=<dir> build/tests/SpectralFaultTests "[.snapshot]"

# Release CPU check: build only the tests, so the installed Debug plugin is not replaced
cmake -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --target SpectralFaultTests
build/release/tests/SpectralFaultTests "16 Brickwall bands*" -s
```

- Plugin identity: product `Spectral Fault`, company `Catastrophic Audio`, CMake target `SpectralFault`,
  manufacturer code `Ctcd`, plugin code `Peq1`, bundle ID `com.catastrophicaudio.spectralfault` (renamed in M7).
  Do not change the codes: hosts use them to recall saved sessions. The saved-state tag `ParametricEQ` and preset
  tag `ParametricEQPreset` also stay, so older sessions and presets keep loading.
- `COPY_PLUGIN_AFTER_BUILD` is on: every build installs the AU and VST3 into `~/Library/Audio/Plug-Ins/`.
- If a fresh AU build does not show up in `auval -a`, restart the AU registry: `killall -9 AudioComponentRegistrar`.

- JUCE 9.0.2 is a git submodule in `external/JUCE`, pinned to the release tag.
- Catch2 is a git submodule in `external/Catch2`, pinned to a release tag.
- Plugin formats: `AU VST3 Standalone`. Link `juce::juce_audio_utils` and `juce::juce_dsp`.
- You cannot listen to audio. Numerical tests are the only way to check DSP changes; I do listening checks myself.
- Primary test host is Logic (AU only). VST3 is checked with pluginval only.

## Design decisions

Decided; details and dates in `docs/PROGRESS.md`.

- Filter topology: matched second-order designs (Vicanek, *Matched Second Order Digital Filters*, 2016),
  run in a biquad structure. Coefficients are computed in our own code, citing the paper.
  Response tests check both the digital design and closeness to the analog prototype (see DSP testing rules).
- Band count: 16.
- Band states: free (not in use), in use and enabled, in use and disabled. A band processes audio only when in
  use and enabled. "In use" is the hidden state property band<n>_used (not a host parameter), mirrored in an
  atomic for the audio and Auto Gain threads; only Delete frees a band, On/Disable only toggles enabled.
- Filter types: Bell, Low Shelf, High Shelf, Low Cut, High Cut, Notch, Band Pass, Tilt Shelf, Flat Tilt, All Pass.
  Cut slopes 6-96 dB/oct in 6 dB steps, plus Brickwall (order-32 Butterworth, 192 dB/oct).
  Cuts are always Butterworth: Q has no effect on them. Odd orders add one first-order section.
  High Cut sections use Vicanek 2016 section 4.1 as published.
- Flat Tilt: 16 matched one-pole shelves an octave apart (2.5 Hz up); gain = total tilt over 20 Hz-20 kHz.
- Shelves: matched two-pole Butterworth (Vicanek, *Matched Two-Pole Digital Shelving Filters*, 2024/2025);
  Q has no effect on shelves. Tilt Shelf uses the matched one-pole shelf (Vicanek 2019, f_m = 0.9).
- Notch, All Pass and first-order cut sections have no published matched formula; they are our own
  derivation from Vicanek 2016's building blocks, marked as such in the code.
- Type, slope and enable changes crossfade old and new filter over ~20 ms.
- Auto Gain is static: an offset computed from the EQ curve, not measured from the audio.
  Formula: -10 log10 of the K-weighted (ITU-R BS.1770-5 Tables 1-2, as a weight) pink-noise power ratio
  over 256 log-spaced points 20 Hz-20 kHz, Low Cut and High Cut excluded, clamped to +-24 dB.
  Computed on a background thread (`AutoGainUpdater`, polls parameters every 20 ms), published via an atomic.
- Output stage: bands -> output gain x Auto Gain x polarity, one 20 ms linear ramp (polarity ramps through zero).
- Per-band channel mode (M6): Stereo / Left / Right / Mid / Side, band<n>_channel. Mid/Side is encoded and decoded
  around each M/S band (M = (L+R)/2, S = (L-R)/2). `StereoTransfer` models the chain as a 2x2 complex matrix per
  frequency; Auto Gain uses its power gain ||M||^2/2 (uncorrelated equal-level L/R) and the display shows one sum,
  L and R, or M and S.
- Analyzer and meter (M5): processBlock pushes stereo input (pre) and output (post) into two `AnalyzerFifo`s
  (SPSC, allocated once in the processor constructor) only while an editor is open. All FFT work, binning,
  smoothing and metering happen on the message thread (`SpectrumAnalyzer`, `LevelMeter`). Analyzer settings
  (mode, resolution, speed, range) are state properties; tilt is fixed at 4.5 dB/oct around 1 kHz.
- Output has one global phase-invert switch.
- Presets (M6b): `src/presets/`. A preset holds all 16 bands and the output section (not view settings). Eleven
  factory presets are compiled in; user presets are XML files in ~/Library/Audio/Presets/Catastrophic Audio/Spectral Fault/ (copied once from the
  old CatastrophicCoder/ParametricEQ folder).
  Presets are shown in the plugin's own browser only (not host programs). Preset code runs on the message thread;
  tests use a temporary folder (`PresetManager::setUserFolder`), never the real one. Preset sources are not cited
  in the repo (owner's decision).
- Dynamic EQ (M7): Bell and shelves; gain law Range or Ratio per band; detector filtered to the band's region
  (bell band pass, shelves lowpass/highpass) from its own input or the optional "Sidechain" bus (falls back to
  its own input); Peak or RMS followed by a linear-level attack/release follower, then dB (2026-09-29); per-channel for Stereo bands; coefficients every 16
  samples. Auto Gain uses static gains. Display shows live gain plus the range. Presets store dynamics (format 2).
- Linear phase (M8): hidden state properties linearPhase / linearPhaseLength (8192 / 16384 / 32768 taps, same at
  every rate). `LinearPhaseDesigner` (frequency sampling of the zero-phase 2x2 matrix, Blackman-Harris window),
  `LinearPhaseUpdater` (background thread), `LinearPhaseEngine` (own partitioned convolution; filter changes keep the
  input history and crossfade). Latency taps/2 + 512. Static bands form the FIR; dynamic bands run as IIR after it,
  detecting the delayed signal. Mode and length switches fade out, wait for a full history, fade in.
- A/B (M9a): `AbComparison` (src/presets) keeps the inactive slot (complete setting incl. phase mode and view);
  saved as an "ABComparison" element next to the parameter state (state version 5), not inside it.
- Undo/redo (M9b): `UndoHistory` records the user's own edits (parameter gestures plus `ScopedTransaction` around
  non-parameter edits) as sound-only `SettingSnapshot`s; preset loads, A/B switches and session loads run under
  `ScopedSuspend` and clear it. New UI edits that change several things must be wrapped in one transaction.
- EQ Sketch (M9d): Option-drag draws; `CurveFitter` (src/dsp) fits bells/shelves/cuts to a target with all
  available slots; bands inside the drawn range are replaced. EQ Match (9e) is meant to reuse the fitter.
- EQ Match (M9e): `MatchSession` learns long-term spectra (side-chain = reference in one pass, else two capture
  passes of the input), `MatchCurve` gives the smoothed, level-free difference, `CurveFitter` turns it into bands
  (replace all or keep existing, asked on Apply). Floating `MatchWindow` from the bottom bar's Match button.
  The session lives in the processor; learned spectra are saved as an "EQMatch" element (state version 6).
- Spectral dynamics (M9g): band<n>_spectral on a dynamic band moves its dynamic part into `SpectralDynamicsEngine`
  (short-time FFT, 2048/512, per-slice follower and gain law, region-weighted), run after the bands; latency
  +2048 while any band is spectral, reported by a 10 Hz timer (`refreshLatency()`); preset format 3.
- MIDI Learn is not planned: MIDI input would change the AU type (aufx -> aumf) and break saved sessions.
- Milestone order after 5: per-band stereo (6), dynamic EQ (7), linear phase (8).

## Hard rules: real-time audio thread

Inside `processBlock` and anything it calls:

- No memory allocation, no `new`/`delete`, no container growth (`push_back`, `resize`).
- No locks, no `std::mutex`, no waiting on other threads.
- No file I/O, logging, `DBG`, or console output.
- No FIR design or other FFT work that is not the audio path itself.

Other threading rules:

- All user-facing parameters are smoothed (`juce::SmoothedValue` or equivalent) or interpolated per sub-block.
- Audio-to-UI data (analyzer, meters) goes through lock-free single-producer/single-consumer FIFOs
  (`juce::AbstractFifo`). The UI never touches audio buffers.
- Heavy work (e.g. linear-phase FIR design) runs on a background thread and is swapped in atomically.

## DSP testing rules

Every DSP change comes with tests that check behaviour numerically:

- Magnitude response, two levels:
  - Implementation: the measured response (impulse through the processing code) matches the
    digital design's own H(e^jw) within 0.1 dB at the centre frequency, ±1 octave and shelf plateaus.
  - Analog closeness: at the centre frequency (and shelf plateaus) within 0.1 dB of the analog
    prototype; at ±1 octave within a bound stated per filter type in its test, with the worst
    measured case written next to it. Bell: 0.35 dB (worst on the grid 0.317 dB).
    Do not raise a bound without asking.
  - Grids: the strict grid above, plus an extended grid that adds f0 = 10 and 16 kHz at 44.1 kHz.
    Each shape asserts both: extended-grid bounds and tighter strict-grid bounds.
  - Test points at or above 0.8 of Nyquist have their own stated near-Nyquist bound per shape.
  - Cut slope points pass if digital and analog are both below -120 dB.
  - Shared grid and checker: `tests/DesignTestGrid.h`. Stated bounds and worst measured values
    are listed in `docs/PROGRESS.md` (Decisions) and next to each assertion.
- Run response tests at 44.1, 48 and 96 kHz (192 kHz for anything near Nyquist).
- Cut filters: attenuation one and two octaves past cutoff equals 6 dB/oct per filter order.
- Stability: a 20 Hz → 20 kHz frequency sweep over one second produces no NaN, no Inf,
  and no sample-to-sample jumps above a set threshold.
- Linear phase (when implemented): reported latency equals measured impulse delay.

Write the test first, then the implementation.

## Code conventions

- C++20 (JUCE requires 17+). JUCE naming style: `camelCase` functions and variables, `PascalCase` classes.
- One class per file. DSP in `src/dsp/`, UI in `src/ui/`, tests in `tests/`, plotting scripts in `tools/`.
- Parameter IDs include the band index: `band1_freq`, `band1_gain`, `band1_q`, `band1_type`, `band1_slope`, `band1_enabled`.
- Plugin state carries a version number from the first saved state onward.
- Prefer `juce::dsp` building blocks where they fit; explain in the plan when writing your own instead.

## Do not

- Edit anything under `external/`.
- Describe or advertise the AI-assisted development in the README, DEVELOPMENT.md, PLAN.md, PROGRESS.md or the
  user guide (owner's decision 2026-09-29), including "proposed by Claude"-style attributions; commit trailers and
  CLAUDE.md itself are fine.
- Copy code from AGPL/GPL projects (e.g. ZL Equalizer, FreeEQ8). Reading them for ideas is fine;
  implement from published papers and formulas and cite the source in a comment.
- Use other companies' product or brand names, logos or wordmarks anywhere (code, UI, docs, commits); copy their
  images, icons, fonts or other assets; or make individual components (knobs, panels, nodes, icons) identical to theirs.
- UI layout (decision 2026-09-28): the overall layout may follow the reference EQ: a full-window display with the dB
  scale and output meter on the right, a thin top bar, a thin bottom bar, and a band panel over the lower part of the
  display. Components and styling are our own.
- Colours may be similar in character to the reference (dark background, saturated band hues, a warm summed curve).
  The colour values are chosen by us, not sampled from the reference image.

## Open decisions (ask before assuming)

Record each decision in `docs/PROGRESS.md` once I make it, then move it out of this list.

- None at the moment. New ones get added here as they come up.
