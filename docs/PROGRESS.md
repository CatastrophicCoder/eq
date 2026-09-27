# Progress

Status of each milestone, decisions made, and a short log per working session.
Milestone definitions and "done when" criteria are in [PLAN.md](PLAN.md#milestones).

## Milestones

| # | Milestone | Status | Notes |
| --- | --- | --- | --- |
| 0 | Toolchain | Done | Command-line build, tests, pluginval and auval pass; owner confirmed CLion build and AU load in Logic (Standalone run not reported separately) |
| 1 | One bell band | Not started | |
| 2 | Full band set, tier 1 | Not started | |
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
