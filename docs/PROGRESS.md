# Progress

Status of each milestone, decisions made, and a short log per working session.
Milestone definitions and "done when" criteria are in [PLAN.md](PLAN.md#milestones).

## Milestones

| # | Milestone | Status | Notes |
| --- | --- | --- | --- |
| 0 | Toolchain | Done | Command-line build, tests, pluginval and auval pass; owner confirmed CLion build and AU load in Logic (Standalone run not reported separately) |
| 1 | One bell band | Done | Knobs, smoothing, state save/load and measured response done; tests, pluginval and auval pass. Owner listening check in Logic: no clicks (session save/reopen not reported separately) |
| 2 | Full band set, tier 1 | In progress | Stage 1 of 5 done (shape designs and tests). Next: stage 2, EqBand with cascades and crossfaded switching |
| 3 | Response curve display | Not started | |
| 4 | Interactive display | Not started | |
| 5 | Spectrum analyzer | Not started | |
| 6 | Per-band stereo | Not started | |
| 7 | Dynamic EQ | Not started | |
| 8 | Linear phase mode | Not started | |
| 9 | Deferred features | Not started | |

Status values: Not started · In progress · Done · Skipped

## Decisions

Newest first. Move each item here from "Open decisions" in `CLAUDE.md` once it is made.

| Date | Decision | Options considered | Reason |
| --- | --- | --- | --- |
| 2026-09-28 | Response-test bounds vs analog, extended grid (fc / +-1 oct below 0.8 Nyq / +-1 oct at or above 0.8 Nyq), with worst measured: Bell 0.1 / 0.65 (0.614) / 2.7 (2.640); Band pass 0.1 / 0.7 (0.645) / 1.4 (1.378); Notch depth < -100 dB / 0.55 (0.538) / 1.8 (1.754); Shelves 0.15 (0.123) / 0.1 (0.080) / 0.25 (0.248); Tilt 0.4 (0.385) / 0.25 (0.222) / 0.05 (0.024); Low Cut 0.1 / 0.5 (0.454); High Cut 0.1 / 0.3 (0.277) / 12.25 (12.218); All pass flat within 1e-9 dB; Flat Tilt 0.25 (0.224) from the ideal line. Strict-grid bounds kept as well (Bell 0.35) | Accept and keep strict grid too; accept extended only; drop the new rows | Chosen by owner after tightening attempts |
| 2026-09-28 | Extended test grid: add f0 = 10 and 16 kHz at 44.1 kHz; separate near-Nyquist bounds for points at or above 0.8 Nyquist | Add rows or keep CLAUDE.md grid; near-Nyquist bounds or leave those points unasserted | Chosen by owner |
| 2026-09-28 | Cut slope test: points pass if digital and analog are both below -120 dB | Test below 0.8 Nyquist only; -120 dB floor; add a Nyquist zero to the design | Chosen by owner |
| 2026-09-28 | High Cut sections: Vicanek 2016 section 4.1 as published | 4.1; three-point section matched at 2 fc; three-point section matched at Nyquist | Chosen by owner; 4.1 is valid everywhere, 3-point at 2 fc was closer below 0.8 Nyq but had no solution in 6/208 cascades |
| 2026-09-28 | Tilt shelf matching point f_m = 0.9 (paper) | 0.9; 0.84 fitted to our grid | Chosen by owner |
| 2026-09-28 | Brickwall = order-32 Butterworth cascade (192 dB/oct, 16 sections) | Steep Butterworth; elliptic IIR; defer to M8 FIR | Chosen by owner |
| 2026-09-28 | Q has no effect on cuts (always Butterworth) | Ignored; resonance on last section; scale all sections | Chosen by owner |
| 2026-09-28 | Flat Tilt: 16 octave-spaced matched one-pole shelves | 16 one-pole shelves; defer to M8 FIR | Chosen by owner |
| 2026-09-28 | M2 shapes: all tier-1 shapes (Flat Tilt and Brickwall need research first) | All tier-1; all but Flat Tilt/Brickwall; core only | Chosen by owner |
| 2026-09-28 | Shelves: matched two-pole Butterworth (Vicanek 2024/25), no Q | Matched Butterworth; RBJ with Q; matched plus one-pole 6 dB option | Chosen by owner |
| 2026-09-28 | Cut slopes in 6 dB steps (6-96 dB/oct) plus Brickwall | 12-dB steps; 6-dB steps; 12-dB steps plus 6 | Chosen by owner |
| 2026-09-28 | Type/slope/enable changes crossfade over ~20 ms; smoothing rule unchanged | Crossfade; switch and reset state; switch and keep state | Chosen by owner (first picked switch and reset, then crossfade to keep the rule) |
| 2026-09-28 | Auto Gain static, from the summed EQ curve | Static from curve; dynamic measured; defer | Chosen by owner |
| 2026-09-28 | M2 editor: grid of all 16 bands | Band selector and strip; grid; generic editor | Chosen by owner |
| 2026-09-28 | Phase invert: one global switch at the output | Not in M2; output invert; per-band invert | Chosen by owner |
| 2026-09-27 | Response tests are two-level: measured vs digital design within 0.1 dB; vs analog within 0.1 dB at f0 and within a stated bound at ±1 octave (bell: 0.35 dB) | Two-level check, loosen ±1 octave only, keep 0.1 dB and change the design, trim the test grid | Chosen by owner after the matched bell measured up to 0.317 dB off analog at +1 octave (wide cuts near f0/fs = 0.1) |
| 2026-09-27 | Project generation: CMake (`juce_add_plugin`) | CMake, Projucer | Chosen by owner |
| 2026-09-27 | Filter topology: matched second-order designs (Vicanek 2016) in a biquad structure; tests compare against the analog prototype | RBJ biquad, TPT/SVF, matched, RBJ now and decide by M4 | Chosen by owner |
| 2026-09-27 | Unit tests: Catch2 as a git submodule in `external/Catch2`, pinned to a release tag | Catch2 or GoogleTest, each via FetchContent or submodule | Chosen by owner |
| 2026-09-27 | Primary test host: Logic (AU); VST3 covered by pluginval only | Logic, Reaper, GarageBand | Chosen by owner |
| 2026-09-27 | Public open-source repo, licence AGPLv3 | Private, public AGPLv3, public permissive | Chosen by owner |
| 2026-09-27 | Target band count: 16 | 8, 16, 24, decide at M2 | Chosen by owner |
| 2026-09-27 | Milestone order after 5: per-band stereo, dynamic EQ, linear phase | Any order of 6–8, or decide later | Chosen by owner |
| 2026-09-27 | `PLAN.md` and `PROGRESS.md` live in `docs/` | Move to `docs/`, keep at root | Chosen by owner |

## Session log

Newest first. One entry per session, a few lines each.

### 2026-09-28 — M2 stage 1

- Done: design classes for every shape in `src/dsp/`: matched lowpass/highpass/bandpass (Vicanek 2016),
  two-pole Butterworth shelves (Vicanek 2024/25), one-pole shelf and tilt (Vicanek 2019), notch, all pass
  and first-order cut sections (own derivations), `ButterworthCascade` (orders 1-32), `FlatTiltDesign`,
  `SectionCascade`, shared `MatchedDesignMath`. Bell moved onto the shared helpers.
- Prototyped in Python first (scratch only) to measure deviations before setting bounds. Two prototype bugs
  found and fixed before any C++: odd-order Butterworth pole angles, and matching the first-order section
  at Nyquist (now matched at fc, which makes every cascade exact at the cutoff).
- Band pass: eq. 40 as printed cancels catastrophically at 20 Hz, Q 18, 192 kHz (B1 < 0 in double);
  replaced by an exact rearrangement. The bell's eq. 45 has the same shape (R1 - R2 phi1 - B0);
  it passes all tests, not changed.
- Tests added / passing: 39/39. C++ worst cases equal the Python prototype to 3 decimals.
- Open issues: FilterType/CutSlope enums moved to stage 2 (first used there).
- Next step: stage 2, EqBand with cascades and crossfaded type/slope/enable switching.

### 2026-09-27 — M1

- Done: matched peaking design (Vicanek 2016, section 4.4) in `src/dsp/MatchedPeakingDesign`;
  `PeakingBand` (JUCE `IIR::Filter<double>` per channel, in-place coefficient updates, 20 ms smoothing,
  32-sample sub-blocks while ramping); `band1_freq/gain/q` in an APVTS; state as XML with `stateVersion=1`;
  rotary-knob editor. Repo now on GitHub (`CatastrophicCoder/eq`).
- Finding: the matched bell is exact at DC and f0 but departs from analog between f0 and Nyquist,
  worst 0.317 dB at +1 octave (-18 dB, Q 0.5, f0/fs ~ 0.11); RBJ is 1.539 dB off on the same case.
  Owner chose the two-level check (see Decisions).
- Tests added / passing: 19/19. Design (f0 vs analog, ±1 octave bound, DC, extremum at f0, stability,
  0 dB identity); band (measured impulse response vs design, sweep 20 Hz-20 kHz, ramp, reset);
  state (IDs/ranges/defaults, round trip, version, invalid data); processor (+12 dB applied end to end).
  Sweep largest step 0.139 / 0.128 / 0.064 at 44.1 / 48 / 96 kHz (limit 0.2).
  pluginval strictness 5 passes (VST3, AU); AU log warns "Current program is -1" (not a failure). auval passes.
- Open issues: the measured-response test takes ~4 s in Debug.
- 2026-09-28: owner listening check in Logic, frequency sweep with gain, no clicks. M1 marked done.
- Next step: M2 (full band set).

### 2026-09-27 — M0

- Done: open decisions settled and recorded; docs moved to `docs/`; repo-local git identity; branch renamed to `main`;
  `.gitignore`, AGPLv3 `LICENSE`; JUCE 9.0.2 and Catch2 v3.16.0 as submodules; CMake plugin skeleton
  (pass-through stereo processor, empty editor, AU/VST3/Standalone, auto-copy to `~/Library/Audio/Plug-Ins`).
- Toolchain: CMake 4.4.3, Ninja 1.13.2, Apple clang 17 with the Command Line Tools only (no Xcode); AU builds fine without it.
- Tests added / passing: `ProcessorSmokeTest` (stereo layout; bit-exact pass-through at 44.1/48/96 kHz), 2/2 pass.
  pluginval 1.0.4 strictness 5 passes for VST3 and AU; `auval -v aufx Peq1 Ctcd` passes.
- Owner checks: CLion build and AU load in Logic work. M0 marked done.
- Open issues: no git remote yet.
- Next step: M1 (matched peaking band).

### YYYY-MM-DD — M<n>

- Done:
- Tests added / passing:
- Open issues:
- Next step:
