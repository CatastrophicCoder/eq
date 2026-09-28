# Spectral Fault

By Catastrophic Audio. A hobby parametric equalizer plugin for macOS (AU, VST3, Standalone), built with JUCE and C++.
Learning project; feature scope is loosely inspired by commercial high-end EQs.

- Plan and milestones: [docs/PLAN.md](docs/PLAN.md)
- Status and session log: [docs/PROGRESS.md](docs/PROGRESS.md)
- Rules for Claude Code: [CLAUDE.md](CLAUDE.md)

## Requirements

- macOS with Xcode and command-line tools (`xcode-select --install`)
- CMake 3.22+ and Ninja (`brew install cmake ninja`)
- CLion (toolchain: Xcode clang, generator: Ninja)
- [pluginval](https://github.com/Tracktion/pluginval) for plugin validation

## Build

```bash
git clone --recurse-submodules <repo-url>
cd eq
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Built plugins are in `build/SpectralFault_artefacts/Debug/`.

## Repository layout

```
CLAUDE.md             Instructions for Claude Code
docs/                 PLAN.md, PROGRESS.md
external/JUCE/        JUCE submodule (pinned release tag)
src/                  PluginProcessor, PluginEditor
src/dsp/              Band, FilterDesign, Analyzer, LinearPhase, Dynamics
src/ui/               ResponseCurve, BandNode, AnalyzerView, LookAndFeel
tests/                Catch2 unit tests (coefficients, magnitude response, stability)
tools/                Python scripts for plotting measured responses
```

## Working with Claude Code

Start a session in the repo root with `claude`. Suggested first prompts, one session each:

1. *Read docs/PLAN.md and CLAUDE.md. Do milestone 0: add JUCE 9.0.2 as a git submodule in
   external/JUCE, write CMakeLists.txt with juce_add_plugin for AU, VST3 and Standalone,
   add a Catch2 test target, and make it build. Propose the file list before writing anything.*
2. *Milestone 1: one peaking band with frequency, gain and Q in an AudioProcessorValueTreeState,
   with parameter smoothing. Write the magnitude-response test first, then the filter, and iterate
   until tests and pluginval pass.*

After each session: run the Standalone target from CLion, listen, then commit.

## Licence

GNU Affero General Public License v3.0 (AGPLv3). JUCE is used under its AGPLv3 option. A `LICENSE` file is added in milestone 0.
