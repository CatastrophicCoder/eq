# ParametricEQ — instructions for Claude Code

## Project

Hobby parametric EQ plugin (AU, VST3, Standalone) for macOS, loosely inspired by commercial high-end EQs.
Learning project, built with JUCE and C++ in CLion. Public repo, licensed AGPLv3.

- Full plan, milestones and design options: `docs/PLAN.md`
- Current status, decisions made, session log: `docs/PROGRESS.md`

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

# validate the plugin
pluginval --strictness-level 5 --validate build/ParametricEQ_artefacts/Debug/VST3/ParametricEQ.vst3

# AU validation (after the AU is copied to ~/Library/Audio/Plug-Ins/Components)
auval -a | grep -i parametriceq
```

- JUCE 9.0.2 is a git submodule in `external/JUCE`, pinned to the release tag.
- Catch2 is a git submodule in `external/Catch2`, pinned to a release tag.
- Plugin formats: `AU VST3 Standalone`. Link `juce::juce_audio_utils` and `juce::juce_dsp`.
- You cannot listen to audio. Numerical tests are the only way to check DSP changes; I do listening checks myself.
- Primary test host is Logic (AU only). VST3 is checked with pluginval only.

## Design decisions

Decided; details and dates in `docs/PROGRESS.md`.

- Filter topology: matched second-order designs (Vicanek, *Matched Second Order Digital Filters*, 2016),
  run in a biquad structure. Coefficients are computed in our own code, citing the paper.
  Response tests compare against the analog prototype curve.
- Band count: 16.
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

- Magnitude response: gain in dB at the centre frequency, at ±1 octave and at shelf plateaus
  matches the analytic formula within 0.1 dB.
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
- Copy code from AGPL/GPL projects (e.g. ZL Equalizer, FreeEQ8). Reading them for ideas is fine;
  implement from published papers and formulas and cite the source in a comment.
- Use other companies' product or brand names (including the commercial EQ that inspired this project),
  or imitate their graphics, in code, UI or docs.

## Open decisions (ask before assuming)

Record each decision in `docs/PROGRESS.md` once I make it, then move it out of this list.

- None at the moment. New ones get added here as they come up.
