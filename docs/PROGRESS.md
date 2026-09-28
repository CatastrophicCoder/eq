# Progress

Status of each milestone, decisions made, and a short log per working session.
Milestone definitions and "done when" criteria are in [PLAN.md](PLAN.md#milestones).

## Milestones

| # | Milestone | Status | Notes |
| --- | --- | --- | --- |
| 0 | Toolchain | Done | Command-line build, tests, pluginval and auval pass; owner confirmed CLion build and AU load in Logic (Standalone run not reported separately) |
| 1 | One bell band | Done | Knobs, smoothing, state save/load and measured response done; tests, pluginval and auval pass. Owner listening check in Logic: no clicks (session save/reopen not reported separately) |
| 2 | Full band set, tier 1 | Done | All 5 stages built and validated (tests Debug + Release, pluginval, auval). Owner listening check in Logic: no clicks (other checklist items not reported separately) |
| 3 | Response curve display | Done | Built and validated (108 tests, pluginval, auval); owner reviewed the layout and colours |
| 4 | Interactive display | Done | Built and validated (135 tests, pluginval, auval). Owner re-check in Logic: everything tested, looks good |
| 5 | Spectrum analyzer | Done | Built and validated (157 tests, pluginval, auval); owner tested in Logic |
| 6 | Per-band stereo | Done | Built and validated (172 tests, pluginval, auval); owner tested in Logic |
| 6b | Presets | In progress | Planning; factory content being researched from public sources |
| 7 | Dynamic EQ | Not started | |
| 8 | Linear phase mode | Not started | |
| 9 | Deferred features | Not started | |

Status values: Not started · In progress · Done · Skipped

## Decisions

Newest first. Move each item here from "Open decisions" in `CLAUDE.md` once it is made.

| Date | Decision | Options considered | Reason |
| --- | --- | --- | --- |
| 2026-09-28 | Look and feel: the overall layout follows the reference EQ (full-window display, dB scale and meter on the right, thin top and bottom bars, band panel over the lower display); components and styling our own; no names, logos or copied assets | Own look with functional conventions; close imitation kept private; close imitation in the public repo; decide later with neutral styling | Chosen by owner (option 3, limited to the overall layout). Trade-dress risk noted at decision time |
| 2026-09-28 | Colours similar in character to the reference; values chosen by us, not sampled from the reference image | Own palette; close to the reference | Chosen by owner |
| 2026-09-28 | M3 band controls: reference-style band panel at the bottom of the display, with 16 band tabs until M4's click-to-select | Panel with tabs; display with the M2 grid below | Chosen by owner |
| 2026-09-28 | Double-click on empty display space adds a Bell (first free band, at the clicked frequency and gain); with all 16 in use nothing is added and a message is shown | Always Bell; type by position | Chosen by owner |
| 2026-09-28 | Factory presets (11): Lead Vocal, Male Vocal, Female Vocal, Acoustic Guitar, Electric Clean, Electric Rhythm, Bass DI, Kick, Snare, Overheads, Mix Bus Polish. Frequencies and cut/boost directions from public mixing guides (at least two per preset); most dB and Q values are conservative choices, not from sources | All 13 proposed; drop the two weakest (Drum Bus, Piano); edit the list | Chosen by owner: drop Drum Bus and Piano |
| 2026-09-28 | Preset sources are not cited in the repo (the brand-name rule would otherwise need an exception) | Rule exception for citations; titles only; no citations in the repo | Chosen by owner |
| 2026-09-28 | Presets become milestone M6b, built before M7 (moved from the M9 list) | Now before M7; after M8; in M9 | Chosen by owner |
| 2026-09-28 | A preset stores all 16 bands (in use, enabled, settings, channel) plus output gain, Auto Gain and invert; not view settings | Bands and output; bands only; everything | Chosen by owner |
| 2026-09-28 | User presets are XML files in ~/Library/Audio/Presets/CatastrophicCoder/ParametricEQ/ | Preset files; host presets only | Chosen by owner |
| 2026-09-28 | Presets are listed in the plugin's own browser only (not exposed as host programs) | Plugin browser only; also as host programs | Chosen by owner |
| 2026-09-28 | Per-band channel mode Stereo / Left / Right / Mid / Side (band<n>_channel, hint 3, default Stereo); Mid/Side encoded and decoded around each M/S band | - | Planned by Claude: per-band transform so L/R and M/S bands can be mixed in one chain |
| 2026-09-28 | Auto Gain with channel modes: exact 2x2 transfer-matrix model (power gain ||M||^2/2, K-weighted), assuming uncorrelated equal-level L and R | Exact 2x2 model; half weight; ignore one-channel bands | Chosen by owner |
| 2026-09-28 | Summed curve with channel modes: one curve for Stereo only; L and R curves with L/R bands; M and S curves with M/S bands only; mixed: L and R from the matrix diagonal (approximate) | Two curves as needed; one curve with all bands; one curve with Stereo bands only | Chosen by owner |
| 2026-09-28 | Nodes show their channel mode as a letter badge (L/R/M/S, none for Stereo) | Letter badge; panel and menu only | Chosen by owner |
| 2026-09-28 | Analyzer modes Off / Pre / Post / Pre+Post (default Pre+Post) | Selectable four modes; post only; pre+post always | Chosen by owner |
| 2026-09-28 | Analyzer slope compensation 4.5 dB/oct, pivot 1 kHz | 4.5; 3; none; selectable | Chosen by owner |
| 2026-09-28 | Analyzer dB range selectable 60 / 90 / 120 dB (default 90) | 0 to -90 fixed; 0 to -120 fixed; selectable | Chosen by owner |
| 2026-09-28 | Output meter added in M5: stereo peak and RMS (300 ms), peak hold 1 s then 20 dB/s, -60 to 0 dBFS, clip light | With M5; later | Chosen by owner; meter details proposed by Claude |
| 2026-09-28 | Analyzer settings (mode, resolution, speed, range) saved with the session as state properties; freeze not saved | - | Follows the display-range decision |
| 2026-09-28 | Bands have three states: free, in use and enabled, in use and disabled. Delete (key or menu) frees a band; On/Disable/double-click only toggles enabled. Disabled bands are grey, bypassed, not editable until re-enabled. Replaces the M4 behaviour where every "off" removed the band | Found by the owner in the M4 check: disable and delete were the same | Owner's specification |
| 2026-09-28 | "In use" stored as hidden state properties band<n>_used (not host parameters), with an atomic copy for the audio and Auto Gain threads; state version 3, older sessions: enabled bands count as in use | Hidden parameter; state property | Chosen by owner |
| 2026-09-28 | Double-click on a node toggles enable/disable (replaces "disables") | Toggle; nothing | Chosen by owner |
| 2026-09-28 | Disabled bands: grey node and grey curve outline, no fill, not in the sum | Grey node and curve; grey node only | Chosen by owner |
| 2026-09-28 | Double-click on a node disables that band (superseded: now toggles) | Disable; reset gain; nothing | Chosen by owner |
| 2026-09-28 | Band tabs removed in M4: bands are selected on the display; the band panel shows the primary selection and hides when nothing is selected | Remove tabs; keep tabs too | Chosen by owner |
| 2026-09-28 | Nodes of types without gain (cuts, notch, band pass, all pass) sit on the 0 dB line | 0 dB line; on the band's curve | Chosen by owner |
| 2026-09-28 | Band colours follow the visible spectrum: band 1 dark violet (390 nm) to band 16 red (645 nm), evenly spaced in wavelength (Bruton 1996); tab numbers use a lightened tint (WCAG contrast >= 4.5) | End at 645 nm; keep 390-700 nm (bands 14-16 identical red); even in hue. Tabs: lighter tint; exact band colour | Chosen by owner; replaces the golden-ratio hues |
| 2026-09-28 | Text scaling with the window: not needed for now | Scale fonts and bar heights; leave | Owner: not an issue for now |
| 2026-09-28 | Display range (3/6/12/30 dB) saved with the session as a non-automatable state property | Save in session; not saved | Chosen by owner |
| 2026-09-28 | M2 editor: one resizable row (1000x360 to 2600x800, default 1480x440) | One row of 16; two rows of 8; one resizable row | Chosen by owner |
| 2026-09-28 | Controls a type does not use are greyed out | Grey out; hide | Chosen by owner |
| 2026-09-28 | Release CPU measured as part of M2 validation and logged | Measure and log; skip | Chosen by owner |
| 2026-09-28 | Auto Gain formula: K-weighted (BS.1770-5) pink-noise power average | Unweighted power average; K-weighted power; mean dB with clamp (mean dB breaks with deep cuts) | Chosen by owner |
| 2026-09-28 | Auto Gain excludes Low Cut and High Cut | All enabled bands; exclude cuts | Chosen by owner |
| 2026-09-28 | Auto Gain limit +-24 dB | +-12; +-24; +-30 dB | Chosen by owner |
| 2026-09-28 | Auto Gain computed on a background thread | Background thread; message-thread timer; coarse on the audio thread | Chosen by owner. Implementation polls parameters every 20 ms instead of using listeners (listeners can fire on the audio thread and depend on the host's notification path) |
| 2026-09-28 | Band defaults: per-type presets, all disabled (1 Low Cut 30 Hz, 2 Low Shelf 80 Hz, 3-14 Bells 120 Hz-12 kHz log-spaced, 15 High Shelf 10 kHz, 16 High Cut 18 kHz; 0 dB; bells Q 1, others Q 0.71; cuts 24 dB/oct) | Off and spread; off at 1 kHz; off with per-type presets | Chosen by owner |
| 2026-09-28 | Band 1 is disabled in a new instance (a new instance is a bit-exact pass-through); M1 sessions migrate to band 1 = enabled bell | Enabled bell at 0 dB; disabled like the rest | Chosen by owner |
| 2026-09-28 | CPU check is an always-on test: 16 Brickwall bands at 96 kHz stereo must run faster than real time in any build | Hidden Release-only benchmark; always-on; skip | Chosen by owner |
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

### 2026-09-28 — M6

- Done: `ChannelMode`, band<n>_channel parameters; frame-wise routing in `EqBand` (Left/Right one channel,
  Mid/Side encode-filter-decode), crossfaded mode changes; complex `response()` on sections; `StereoTransfer`
  (2x2 chain matrices) for Auto Gain and the split sum curves; L/R/M/S node badges; channel menu in the panel
  and a Channel submenu in the node menu.
- Tests added / passing: 172/172. Left/Right leave the other channel bit-exact; Mid/Side leave the other part
  bit-exact; neutral M/S returns the input; crossfades; matrices, chain order and power gain; Auto Gain exact
  per mode; K-weighted loudness of uncorrelated pink noise with a mix of modes and Auto Gain on: 0.018 dB
  change (limit 0.3); sum curves exact for L/R-only and M/S-only chains; measured L and R responses within
  0.1 dB of the displayed sums. pluginval strictness 5 (VST3, AU) and auval pass.
- Test fixes: the M/S test relied on AudioProcessor::reset() clearing the filters, which the processor does not
  override; a 3 dB threshold was only the large-gain limit, replaced by the exact formula.
- Open issues: the processor does not override reset() (hosts call it on transport jumps; filter tails then
  continue). Candidate for a later small fix.
- 2026-09-28: owner tested in Logic. M6 marked done. Owner feedback: pre and post analyzer curves are both grey
  and hard to tell apart (to fix before M7).
- Analyzer colours fixed (owner's choice: different colours): pre muted blue, post warm light grey; a test keeps
  them apart. Where a band's fill overlaps the analyzer the tints mix with the band colour (bands are drawn on top).
- Next step: M7 (dynamic EQ).

### 2026-09-28 — M5

- Done: `AnalyzerFifo` (src/dsp), `SpectrumAnalyzer`, `LevelMeter`, `AnalyzerSettings` (src/ui); processor taps
  (pre/post, stereo, only while an editor is open); analyzer drawn under the curves (pre faint, post brighter) with
  its own dB scale; stereo output meter at the right edge (click resets the clip lights); bottom-bar controls for
  mode, resolution (shown as FFT size), speed, range and freeze, with tooltips; settings saved with the session.
- Tests added / passing: 157/157. FIFO order, overflow and a two-thread stress test; 0 dBFS calibration at all
  four FFT sizes; peak location; equal levels across frequency; release rate; freeze; tilt; meter peak/RMS, hold,
  fall, clip latch; taps equal input/output exactly; no allocation in push or processBlock with the taps on;
  settings persistence. pluginval strictness 5 (VST3, AU) and auval pass.
- Deviation from the plan: no white-noise flatness test. With max-per-point binning noise cannot read flat (wide
  high-frequency points pick higher maxima); replaced by equal-amplitude sines across the range.
- Found: the FIFO stress test hung against the stub (now bounded by a deadline); peak hold ran a step long from
  float rounding (1 - 10 x 0.1); resolution and speed menus both read "Medium" (resolution now shows FFT size).
- Open issues: low frequencies look jagged at small FFT sizes (few bins per point there; higher resolution helps).
- Validation timing: pluginval VST3 3 s, AU 4 s, auval < 1 s. An earlier 10+ minute run most likely came from
  restarting AudioComponentRegistrar before auval (full AU registry rescan; not proven). Restart it only when a
  fresh AU build is missing from `auval -a`, as CLAUDE.md says.
- 2026-09-28: owner tested in Logic. M5 marked done.
- Next step: M6 (per-band stereo: Stereo / Left / Right / Mid / Side).

### 2026-09-28 — M4

- Done: `NodeLayout`, `SelectionModel`, `NodeDragController`, `BandParameterWriter` in `src/ui/`; `ResponseDisplay`
  nodes, selection (click, Cmd-click, area with Shift to add), drags (multi-band, Shift fine), Q by wheel/pinch,
  double-click add (Bell) / disable, right-click type/slope/disable menu, Delete/Backspace, all-bands-in-use
  notice, readout. Band tabs removed; the panel follows the primary selection and hides without one.
  `EDITOR_WANTS_KEYBOARD_FOCUS` on; unhandled keys return to the host.
- Tests added / passing: 124/124. Interaction is tested through handlePress/Drag/Release/Wheel/Magnify/Key
  (no synthetic mouse events); a parameter listener checks every edit is inside matched host gestures.
  pluginval strictness 5 (VST3, AU) and auval pass.
- Modifier choice: Cmd-click toggles selection; Shift is fine-drag on a node and add-to-selection on an area
  (Shift could not also toggle on click without clashing with fine-drag).
- Open issues: a node under the band panel cannot be clicked there (reachable by area selection); host key
  handling (Delete reaching the plugin, space bar still reaching Logic) needs the owner's check.
- Owner check 1: disable and delete were the same (one flag meant both "exists" and "on"). Fixed with three band
  states (free / enabled / disabled), hidden band<n>_used properties, state version 3 with migration. Disabled
  bands are grey and not editable; double-click on a node toggles; only Delete (key or menu) frees a band.
  135/135 tests, pluginval and auval pass.
- 2026-09-28: owner re-check in Logic, everything tested (band states, keyboard, dragging, automation). M4 marked done.
- Next step: M5 (spectrum analyzer).

### 2026-09-28 — M3

- Done: look-and-feel decision recorded and CLAUDE.md rule updated (overall layout may follow the reference;
  components, styling and colour values our own). `FrequencyAxis`, `ResponseCurves` (512 log points, recompute
  only on change), `ResponseDisplay` (grid, labels, dB scale, filled band curves, amber sum, clipping, range
  button 3/6/12/30 dB saved as the `displayRangeDb` state property), `BandPanel` (16 coloured tabs, one band's
  controls, re-attached per band), `TopBar`, `BottomBar`. Editor 1280x770 default, 960x580-2400x1440.
  M2's `BandStrip`/`OutputStrip` removed; menu-label logic moved to `MenuLabels.h`.
- Tests added / passing: 104/104. Axis mapping and grid; band curves equal BandDesign; displayed sum vs the
  measured plugin response within 0.1 dB (44.1/48/96 kHz); no recompute without a change; range persistence and
  fallback; layout at three sizes; tab re-attachment (and no writes to the previous band); menus readable;
  greying; curve clipping. pluginval strictness 5 (VST3, AU) and auval pass.
- Checked by eye via snapshots: clipped "+12" scale label found and fixed.
- Owner review: layout works well. Band colours changed to a violet-to-red spectrum (390-645 nm; 700 nm made
  bands 14-16 identical, found in the snapshot, now covered by a distinctness test); tab numbers tinted for
  contrast. Text scaling left as is (owner's call). 108/108 tests, pluginval passes.
- 2026-09-28: owner reviewed the layout and spectrum colours. M3 marked done.
- Next step: M4 (interactive display).

### 2026-09-28 — M2 stage 5

- Done: `BandStrip`, `OutputStrip`, `CompactLookAndFeel` in `src/ui/`; resizable one-row editor with a 15 Hz
  timer for greying and the Auto Gain readout; short menu labels when full ones do not fit; gain text never
  shows -0.00. Hidden `[.snapshot]` test renders the editor to PNG (used to check the layout by eye).
- Tests added / passing: 89/89 (Debug), 99346/99346 assertions (Release). Editor structure, size limits,
  layout at min/default/max, all 99 controls attached both ways, menu choices, greying per type, readout,
  menu text shown and fitting at all sizes, repeated open/close. pluginval strictness 5 (VST3, AU, including
  its editor tests) and auval pass.
- CPU, 16 Brickwall bands at 96 kHz stereo, 1 s of audio: Debug 232 ms (4.3x real time),
  Release ~37 ms (~27x real time).
- Found (from the rendered snapshot, not the first tests): gain showing -0.00; menus truncated to "..." by
  the V4 combo-box layout; then blank menus, because `ComboBox::getSelectedId()` returns 0 after
  `changeItemText`. The first fit test passed on blank menus; it now requires the shown text to equal the
  selected item. In Release, the allocation counter's self-test was optimised away (elided new/delete);
  it now calls `::operator new` directly.
- 2026-09-28: owner listening check in Logic, no clicks. M2 marked done.
- Next step: M3 (response curve display).

### 2026-09-28 — M2 stage 4

- Done: `output_gain` (-30..+30 dB, continuous), `auto_gain`, `output_invert` (hint 2, no state bump).
  `KWeighting` (BS.1770-5 Tables 1-2, read from the standard's PDF), `AutoGain`, `AutoGainUpdater` thread,
  output stage with one smoothed gain. `Parameters::toBandSettings` shared by processor and thread.
- Tests added / passing: 79/79. K-weighting +0.691 dB at 997 Hz (the standard's note 1); Auto Gain vs an
  independent 8192-point integration within 0.05 dB; measured K-weighted loudness of the whole plugin vs the
  model: 4.5e-7 dB (tone bands only) and 9e-8 dB (with an excluded cut); bit-exact invert and bypass;
  click-free gain and polarity changes; offset published with no processBlock calls; zero allocations with
  Auto Gain on. pluginval strictness 5 (VST3, AU) and auval pass.
- Found: a 0.01 dB snapping interval turns 0 dB into -6.7e-7 dB in float; output_gain made continuous.
  Band gains keep the M1 interval (a "0 dB" bell is -6.7e-7 dB; inaudible, not changed).
  Two test mistakes fixed: waiting for any non-zero offset raced the thread; the expectation with an excluded
  cut assumed K-weighted power is separable, which it is not where tone bands and the cut overlap.
- Next step: stage 5, grid editor for 16 bands plus output controls, then M2 validation.

### 2026-09-28 — M2 stage 3

- Done: 96 band parameters (`band<n>_{freq,gain,q,type,slope,enabled}`, n = 1-16) via `Parameters::id()`,
  per-type preset defaults (all disabled), version hints 1 for M1's band1 freq/gain/q and 2 for the rest.
  Processor runs 16 `EqBand`s in series. State version 2 with migration of M1 sessions (band 1 = enabled bell).
  Idle bypassed bands skip their sample loop.
- Tests added / passing: 64/64. Parameters (IDs, ranges, choices, defaults, hints), round trip of all 96,
  v1 migration from an M1-format blob, newer/invalid state ignored, bands in series equal the sum of their designs
  (within 0.1 dB at 44.1/48/96 kHz), bit-exact new instance, zero allocations with 16 active bands.
  CPU (Debug): 16 Brickwall bands at 96 kHz stereo, 1 s of audio in 232 ms (4.3x real time).
  pluginval strictness 5 (VST3, AU) and auval pass.
- Found: the first allocation test counted 90 allocations that came from the test's own `Parameters::id()`
  strings, not from `processBlock` (0). Fixed the test to look parameters up first, as a host does.
- Open issues: the editor still shows only band 1 freq/gain/q, and band 1 now starts disabled, so a listening
  check needs the host's generic parameter view (Logic: Controls) until the stage-5 grid editor.
  Planned commits "16-band parameters" and "run 16 bands in the processor" landed as one.
- Next step: stage 4, output gain, static Auto Gain, output phase invert.

### 2026-09-28 — M2 stage 2

- Done: `FilterType` (order fixed, saved in sessions) and `CutSlope` (index 0-15 = orders 1-16, 16 = Brickwall);
  `BandSettings`; `BandDesign` (settings -> sections, disabled = none, frequency capped at 0.49 fs);
  `CascadeProcessor` (own TDF-II cascade in double, fixed arrays; matches `juce::dsp::IIR::Filter<double>`
  within 1e-12 on random cascades); `EqBand` (two slots, 20 ms smoothing, 20 ms linear crossfade for
  type/slope/enable, later discrete requests wait and the latest wins). Processor runs band 1 through `EqBand`
  as a bell. `PeakingBand` removed.
- Tests added / passing: 57/57. Measured response vs design within 0.1 dB for every type and cut slope
  (2148 checks); sweeps for every type incl. Brickwall, largest step 0.142 (clean +12 dB sine: 0.1425);
  crossfades 0.142 / 0.130 (enable); stress test settles on the last request; zero allocations in
  `EqBand::process` and `processBlock` (global allocation counter in `RealtimeAllocationTest`).
  pluginval strictness 5 (VST3, AU) and auval pass.
- Open issues: the measured-response test takes ~9 s in Debug. Pushing happens from the owner's IDE
  (this shell has no GitHub credentials); stage 1 is on GitHub.
- Next step: stage 3, 16 bands, parameters, state version 2 with v1 migration.

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
