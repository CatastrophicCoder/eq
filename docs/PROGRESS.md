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
| 6b | Presets | Done | Built and validated (186 tests, pluginval, auval); owner tested in Logic |
| 7 | Dynamic EQ | Done | Built and validated (223 tests, pluginval, auval); owner tested in Logic except the side-chain (covered by unit tests only). Peak detector reading still an open decision |
| 8 | Linear phase mode | Done | Built and validated (245 tests, pluginval, auval); owner tested in Logic |
| 9 | Deferred features | In progress | Split into 9a-9g (small to large). 9a A/B done. 9b undo/redo done. 9c peak pick and 9d EQ Sketch done. 9e EQ Match built and validated, awaiting owner's Logic test |

Status values: Not started · In progress · Done · Skipped

## Decisions

Newest first. Move each item here from "Open decisions" in `CLAUDE.md` once it is made.

| Date | Decision | Options considered | Reason |
| --- | --- | --- | --- |
| 2026-09-28 | Look and feel: the overall layout follows the reference EQ (full-window display, dB scale and meter on the right, thin top and bottom bars, band panel over the lower display); components and styling our own; no names, logos or copied assets | Own look with functional conventions; close imitation kept private; close imitation in the public repo; decide later with neutral styling | Chosen by owner (option 3, limited to the overall layout). Trade-dress risk noted at decision time |
| 2026-09-28 | Colours similar in character to the reference; values chosen by us, not sampled from the reference image | Own palette; close to the reference | Chosen by owner |
| 2026-09-28 | M3 band controls: reference-style band panel at the bottom of the display, with 16 band tabs until M4's click-to-select | Panel with tabs; display with the M2 grid below | Chosen by owner |
| 2026-09-28 | Double-click on empty display space adds a Bell (first free band, at the clicked frequency and gain); with all 16 in use nothing is added and a message is shown | Always Bell; type by position | Chosen by owner |
| 2026-09-28 | Dynamic gain law selectable per band: Range (full range 12 dB above threshold, smoothstep knee) or Ratio ((level - threshold)(1 - 1/ratio), 6 dB soft knee, capped at the range, sign of the range) | Threshold + range; ratio + range cap; selectable | Chosen by owner (law details proposed by Claude) |
| 2026-09-28 | Dynamic types: Bell, Low Shelf, High Shelf | Bell + shelves; all gain types; bell only | Chosen by owner |
| 2026-09-28 | Detector listens to the band's region (bell: band pass at f/Q; low shelf: lowpass; high shelf: highpass) or, per band, the external side-chain filtered the same way; a band set to side-chain with no side-chain connected falls back to its own region | Region + side-chain; region only | Chosen by owner (fallback proposed by Claude) |
| 2026-09-28 | Detector Peak or RMS (10 ms), selectable per band; attack/release in the dB domain | Peak; RMS; selectable | Chosen by owner |
| 2026-09-28 | Dynamic bands recompute their filters every 16 samples | 16; 32; every sample | Chosen by owner |
| 2026-09-28 | Auto Gain uses static gains only (dynamic movement ignored) | Static only; follow live gain | Chosen by owner |
| 2026-09-28 | Stereo dynamic bands detect and act per channel (a Stereo band can then give L and R different gains) | Linked; per channel | Chosen by owner |
| 2026-09-28 | Display shows live dynamic gain plus the range | Live gain + range; range only | Chosen by owner |
| 2026-09-28 | Dynamic parameters per band (hint 4): dyn, dynmode, thresh (-60..0 dB), range (-24..+24 dB), ratio (1-20), attack (0.1-200 ms), release (5-2000 ms), detector, sidechain; 259 parameters in total | - | Proposed by Claude, accepted by owner |
| 2026-09-28 | Factory presets (11): Lead Vocal, Male Vocal, Female Vocal, Acoustic Guitar, Electric Clean, Electric Rhythm, Bass DI, Kick, Snare, Overheads, Mix Bus Polish. Frequencies and cut/boost directions from public mixing guides (at least two per preset); most dB and Q values are conservative choices, not from sources | All 13 proposed; drop the two weakest (Drum Bus, Piano); edit the list | Chosen by owner: drop Drum Bus and Piano |
| 2026-09-28 | Preset sources are not cited in the repo (the brand-name rule would otherwise need an exception) | Rule exception for citations; titles only; no citations in the repo | Chosen by owner |
| 2026-09-28 | Presets become milestone M6b, built before M7 (moved from the M9 list) | Now before M7; after M8; in M9 | Chosen by owner |
| 2026-09-28 | A preset stores all 16 bands (in use, enabled, settings, channel) plus output gain, Auto Gain and invert; not view settings | Bands and output; bands only; everything | Chosen by owner |
| 2026-09-28 | User presets are XML files in ~/Library/Audio/Presets/CatastrophicCoder/ParametricEQ/ | Preset files; host presets only | Chosen by owner |
| 2026-09-29 | M8 phase mode (Zero latency / Linear phase) is a hidden state property, not a host parameter | Host parameter; hidden state property | Chosen by owner |
| 2026-09-29 | M8 filter length is a tap count, the same at every sample rate (latency in ms shrinks at higher rates) | Same ms at every rate; same tap count | Chosen by owner |
| 2026-09-29 | M8 latency menu: 8192 / 16384 / 32768 taps (85 / 171 / 341 ms at 48 kHz) | 4 steps 4k-32k; 5 steps 2k-32k; 3 steps 4k/16k/64k; 3 steps 8k-32k | Chosen by owner |
| 2026-09-29 | M8: dynamic bands run as normal (IIR) filters after the linear-phase FIR, as in Zero latency mode | IIR after FIR; static part in FIR plus delta filter; unavailable in Linear phase | Chosen by owner |
| 2026-09-29 | M8: dynamic bands detect on the delayed signal (where they act) | Delayed signal; undelayed input (lookahead) | Chosen by owner |
| 2026-09-29 | Detector (Peak and RMS): classic linear-domain attack/release follower, converted to dB afterwards; replaces the dB-domain smoothing of 2026-09-28 | Keep; peak hold + dB smoothing; classic follower; decide later (RMS: both or Peak only) | Chosen by owner (both Peak and RMS) |
| 2026-09-29 | M8 FIR design: frequency sampling of the zero-phase 2x2 matrix on a grid 4x finer than the filter, inverse FFT, centred, 4-term Blackman-Harris window (Harris 1978); tap 0 zero, exactly symmetric, latency N/2 | - | Planned by Claude |
| 2026-09-29 | M8 accuracy bounds: a band is resolved when its frequency and (bell, notch, band pass) bandwidth f0/Q are at least 32 FFT bins (32 fs/N); for resolved bands above 32 fs/N: smooth shapes within 0.1 dB (worst measured 0.048 dB, 1 kHz Q8 notch), cuts within 0.5 dB outside their transition band (max(1/4, 24/slope) octaves) with a -40 dB floor (worst measured 0.000 dB); smooth-shape floor -60 dB. 93 of 126 grid cases resolved | - | Proposed by Claude (new bounds) |
| 2026-09-29 | M8 convolution: own uniformly partitioned overlap-save engine (512-sample partitions) instead of juce::dsp::Convolution, whose new filters start with an empty history (a dropout of up to half the filter length on every change); the input spectra are kept across filter changes and old/new outputs crossfade over 1024 samples. Adds 512 samples: latency = taps/2 + 512 | juce::dsp::Convolution; own engine | Chosen by Claude after a failing test (reported to owner) |
| 2026-09-29 | M8 mode and length switches: fade out (20 ms), switch, stay silent until the filter of the new length is in use and has a full input history (taps samples), fade in (20 ms). prepareToPlay and reset() count as a fresh stream (no wait) | - | Planned by Claude |
| 2026-09-29 | MIDI Learn dropped: MIDI input would change the AU type from aufx to aumf (breaking saved Logic sessions), and VST3 would need hidden controller parameters; hosts map controllers to parameters themselves | Drop; VST3 and Standalone only; change the AU type; postpone | Chosen by owner |
| 2026-09-29 | M9 split into 9a A/B, 9b undo/redo, 9c Spectrum Grab, 9d EQ Sketch, 9e EQ Match, 9f natural-phase-style mode, 9g spectral dynamics, built in that order | Small to large; workflow first; DSP first | Chosen by owner |
| 2026-09-29 | A/B slots hold everything: all parameters, bands in use, phase mode and length, view settings, current preset | Same as a preset; preset plus phase mode; everything | Chosen by owner |
| 2026-09-29 | Both A/B slots and the active one are saved with the session (state version 5); older sessions start with both slots equal | Saved with session; only while open | Chosen by owner |
| 2026-09-29 | Undo/redo covers sound edits only (bands, output, add/delete/enable, phase mode); not preset loads, A/B switches, view settings or host automation | Sound only; plus presets and A/B; everything | Chosen by owner |
| 2026-09-29 | A preset load or A/B switch clears the undo history | Clear; one history per slot; keep one history | Chosen by owner |
| 2026-09-29 | Undo history lives only while the plugin is open (not saved with the session) | Only while open; saved | Chosen by owner |
| 2026-09-29 | Undo/redo through two buttons in the top bar only; no keyboard shortcuts (Cmd-Z stays with Logic) | Buttons and shortcuts; buttons only; shortcuts only | Chosen by owner |
| 2026-09-29 | Peak pick (9c): a marker on the nearest spectrum peak appears while hovering empty display space; dragging it creates a band | Hover markers; modifier + drag; pick button | Chosen by owner |
| 2026-09-29 | Peak pick band: Bell at the peak frequency, Q from the peak's -3 dB width (0.5-18), gain set by the drag from 0 dB | Q from width; fixed Q 6; fixed Q 1 | Chosen by owner |
| 2026-09-29 | Peak pick follows the shown spectrum: pre when Pre or Pre+Post is shown, post when only Post; none with the analyzer off | Pre; post; whichever is shown | Chosen by owner |
| 2026-09-29 | Peak pick ring holds its peak and place while the pointer stays within the half-octave window it was chosen from (first implemented as 30 px, which failed while approaching); a press within 30 px picks it (owner feedback: the ring jumped with the live peak) | Hold while near; slow peak memory; both | Chosen by owner |
| 2026-09-29 | EQ Sketch (9d): Option + drag on empty display space draws a curve; release turns it into bands | Sketch button; Option + drag; right-click menu | Chosen by owner |
| 2026-09-29 | EQ Sketch replaces the bands whose frequency lies inside the drawn range; bands outside stay | Keep, use free slots; replace all; replace in drawn range | Chosen by owner |
| 2026-09-29 | EQ Sketch fitter may use bells, shelves and cuts | Bells only; bells + shelves; bells, shelves, cuts | Chosen by owner |
| 2026-09-29 | EQ Sketch uses all available slots (free plus replaced) for the closest fit | Fewest for 1 dB; fewest for 0.5 dB; all available slots | Chosen by owner |
| 2026-09-29 | Curve fitter bounds: single band within 0.01 dB (worst 4e-11), combinations of the fitter's own shapes within 0.1 dB above -40 dB (worst 0.038, bells + 24 dB/oct high cut), outside a partial range within 0.5 dB (worst 0.357); a drawn hump within 1 dB at its top | - | Proposed by Claude (new bounds) |
| 2026-09-29 | EQ Sketch keeps using all available slots (revisited after a full-width sketch filled all 16 slots) | Keep all slots; fewest for 1 dB; fewest for 0.5 dB | Chosen by owner |
| 2026-09-29 | Peak pick rings stay visible with all 16 bands in use; pressing one shows "All 16 bands in use" | Hidden; shown, press explains | Chosen by owner |
| 2026-09-29 | Peak pick shows rings on the 5 most prominent peaks of the shown spectrum while the pointer is over the display; nodes do not hide them; a ring within half an octave of the pointer holds still (replaces the single ring near the pointer) | One near the pointer; top peaks while hovering; near pointer, also over nodes | Chosen by owner |
| 2026-09-29 | EQ Match reference: the side-chain when connected (reference and current learned in one pass), otherwise two capture passes through the plugin input | Side-chain; capture; audio file; side-chain or capture | Chosen by owner |
| 2026-09-29 | EQ Match learns long-term average spectra between Learn and Stop; the match is computed once from the two averages | Average while learning; live match | Chosen by owner |
| 2026-09-29 | EQ Match asks on Apply whether to replace all bands or keep them and use the free slots | Replace all; keep, use free slots; ask each time | Chosen by owner |
| 2026-09-29 | EQ Match controls: Amount (0-100 %) and Smoothing (1/12 to 1 octave) | Amount + smoothing; amount only; none | Chosen by owner |
| 2026-09-29 | EQ Match controls in a separate floating window opened by a Match button; the target curve is previewed on the display | Panel over display; separate window; bottom bar strip | Chosen by owner |
| 2026-09-29 | EQ Match bound: match curve and applied bands within 0.25 dB of a known EQ's shape, 100 Hz - 10 kHz, for 6-8 s of pink noise at 1/6-octave smoothing (worst measured 0.057 dB) | - | Proposed by Claude (new bound) |
| 2026-09-28 | Plugin name "Spectral Fault", brand (company) "Catastrophic Audio" | Name lists proposed by Claude | Chosen by owner |
| 2026-09-28 | Rename details: bundle ID com.catastrophicaudio.spectralfault; CMake target SpectralFault (tests SpectralFaultTests); plugin codes, saved-state tag and preset tag unchanged; rename committed under M7 | Keep or change bundle ID; keep or rename target; M7 or separate prefix | Chosen by owner |
| 2026-09-28 | User preset folder moves to ~/Library/Audio/Presets/Catastrophic Audio/Spectral Fault/; the old folder's presets are copied once (only if the new folder has none); old files stay | Keep old path; move without migration; move and migrate | Chosen by owner |
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

### 2026-09-29 — 9e (EQ Match)

- Done: `SpectrumAverager` (src/dsp; 8192-point Hann frames, 50 % overlap, power averaged per bin, read with
  +-1/24-octave band averaging), `MatchCurve` (smoothing, reference - current, overall level removed, amount,
  +-24 dB), `MatchSession` (src/ui; learning passes, side-chain one-pass mode, apply via CurveFitter: replace all
  or keep existing with the free slots, one undo step), `MatchPanel` + `MatchWindow` (floating, always on top),
  bottom bar Match button, dashed preview on the display, side-chain tap FIFO fed only while learning.
  `BandParameterWriter::writeFittedBand` now shared by EQ Sketch and EQ Match.
- Found while testing: single-bin readings were noisy at low frequencies (a 1.2 dB outlier at 100 Hz after 8 s of
  white noise); levels now average the bins within +-1/24 octave, so steady tones read below their peak level
  (noise reads its density; matching compares like with like).
- Tests added / passing: 286/286. Averager: white flat, pink -3 dB/oct by regression, a tone stands out 40 dB;
  smoothing widths; match curve recovers a known bell + shelf within 0.05 dB, amount scales, level ignored;
  side-chain tap only while learning; capture/replace, keep-existing and side-chain flows within 0.06 dB, one undo
  step; window toggles from the bottom bar, preview appears and follows Amount; no allocation with the tap on.
  pluginval strictness 5 (VST3, AU) and auval pass. Snapshot of the panel (m9e_match_panel.png).
- Next step: owner's check in Logic, then 9f natural-phase-style mode.

### 2026-09-29 — 9c/9d owner feedback (third round)

- Owner, after restarting Logic: Option-drag works (a full-width sketch added nodes nicely); but afterwards no
  rings appeared until bands were deleted, and then only when the pointer met a peak. Causes, both by design: the
  sketch filled all 16 slots (rings were hidden with no free slot), and only one ring near the pointer, never over
  nodes, was shown. Owner decisions (Decisions table): keep all slots; show rings with full slots and explain on
  press; rings on the 5 most prominent peaks while hovering, nodes not hiding them, each held while the pointer
  is within half an octave.
- Tests rewritten for the rings (top 5 of the shown spectrum, kept over nodes, none with the analyzer off or the
  pointer outside; drag creates the bell; full slots: message, no band, no undo step; held ring while approaching;
  rings away from the pointer follow the spectrum). 278/278 tests, pluginval strictness 5 (VST3, AU), auval pass.
- A clang crash (segmentation fault) on one build was not reproducible; the rebuild succeeded.
- Owner retested in Logic; 9c and 9d done.
- Next step: 9e EQ Match.

### 2026-09-29 — 9c/9d owner feedback (second round)

- Owner: Option-drag did nothing; the ring still jumped while hovering. Logic had been started at 12:33, before the
  ring fix (12:39) and EQ Sketch (12:49) were installed, so it was still running the 9c build; Logic keeps a
  plugin's code until it quits.
- Separately, the ring fix itself was flawed: it held only within 30 px, but a ring can appear up to half an octave
  from the pointer, so approaching it re-picked peaks on every move. Now it holds while the pointer stays over empty
  space within the half-octave window it was chosen from; a press within 30 px picks it. New test: approaching the
  ring in steps while the spectrum changes leaves it in place (278/278 tests, pluginval and auval pass).
- Next step: owner retests both in Logic after restarting it.

### 2026-09-29 — 9d (EQ Sketch)

- Done: `CurveFitter` (src/dsp): end analysis (level end -> shelf, steep drop -> cut with slope from the drop),
  bells added at the largest remaining error until every slot is used, Levenberg-Marquardt refinement on the
  plugin's own designs; fitted with and without the end bands, closer fit kept (a broad bell's tail had been taken
  for a shelf plateau). `ResponseDisplay`: Option-drag on empty space draws (one height per pixel column, line
  shown while drawing); release replaces bands inside the drawn range, keeps the rest, fits the sketch minus the
  kept bands with outside points weighted 0.5 towards "add nothing"; one undo step. Sketches under 1/3 octave ignored.
- Tests added / passing: 277/277. Slot count, stereo/enabled/no dynamics; single bells exact; three combinations
  with shelf/cut at the ends; partial range spill; 16-band fit Debug 324 ms, Release 42 ms; sketch replaces in range,
  keeps outside, curve within 1 dB of the hump, complete host edits, one undo step; plain drag selects; narrow
  sketch ignored. pluginval strictness 5 (VST3, AU) and auval pass. Snapshot of a sketch in progress (m9d_sketch.png).
- Observed: with "all available slots", a simple hump sketched with 15 slots gave 11 bands within 0.5 dB of 0 dB.
- Next step: owner's check in Logic, then 9e EQ Match.

### 2026-09-29 — 9c (peak pick, the plan's "Spectrum Grab")

- Done: `PeakFinder` (src/ui): local maxima at least 3 dB prominent within an octave, Q from the -3 dB width;
  `ResponseDisplay`: marker (with frequency label) on the most prominent peak within half an octave of the pointer
  over empty space, from the shown spectrum; pressing it creates a Bell (peak frequency, estimated Q clamped
  0.5-18, 0 dB) and the drag sets the gain only; creation and drag are one undo step. No marker over nodes, with
  the analyzer off or with all 16 bands in use. The UI uses the neutral name "peak pick" (brand rule).
- Tests added / passing: 269/269. Finder frequency and Q (within 10 % for Q 1, 4, 12) and prominence; no peaks for
  flat spectra or a 2 dB bump; most prominent near the pointer; marker from pre or post by analyzer mode, none over
  nodes or with the analyzer off, on a real 1 kHz resonance fed through the plugin; drag creates the bell with gain
  following, frequency fixed, complete host edits, one undo step; all bands in use: no marker. pluginval
  strictness 5 (VST3, AU) and auval pass. Snapshot shows the marker.
- Owner feedback from Logic: the ring jumped with the live peak and could only be caught with the analyzer frozen.
  Fixed: the ring holds while the pointer is within 30 px, and a press in that radius picks it (270/270 tests,
  pluginval and auval pass). Awaiting the owner's check of the fix.
- Next step: 9d EQ Sketch.

### 2026-09-29 — 9b (undo/redo)

- Done: `UndoHistory` (src/presets): steps from the user's parameter gestures and explicit transactions (band in use,
  phase mode, display add/delete/enable/menu edits); sound-only snapshots via the new shared `SettingSnapshot`
  (A/B now uses it too); undo/redo apply them as host edits; wheel/pinch Q steps on the same bands within 1 s merge;
  100 steps; preset loads, A/B switches and session loads clear it. Top bar undo/redo buttons left of A/B.
- Tests added / passing: 263/263. Knob/menu edit = one step; multi-node drag = one step; add, delete, disable and
  menu type change each one step; phase mode and length undoable with latency; Q wheel merge; automation, view
  settings and empty gestures not recorded; preset/A-B/session load clear; redo cleared by a new edit; 100-step
  cap; undo sends complete host edits for changed parameters only and records nothing; buttons enable, act and
  fit at three sizes. pluginval strictness 5 (VST3, AU) and auval pass.
- Owner tested undo/redo in Logic; 9b done.
- Next step: 9c Spectrum Grab.

### 2026-09-29 — M9 planning and 9a (A/B comparison)

- M9 assessed feature by feature: only MIDI Learn needed a change to existing work (AU type aufx -> aumf); dropped
  by owner. Order and A/B decisions recorded.
- Done: `AbComparison` (src/presets): capture/apply of the complete setting, switch (differing parameters as host
  edits), copy, saved state; top bar A / B / copy buttons left of the preset browser, active slot in amber.
- Tests added / passing: 253/253. Start state; each slot keeps its edits (bands, in-use, dynamics, output, phase
  mode, view); copy; host edits only for differing parameters, all complete; saved and restored with the active
  slot, exactly one A/B element in the saved state; older sessions give equal slots; loading a preset changes only
  the active slot; buttons switch, copy and show the active slot, laid out at three sizes. pluginval strictness 5
  (VST3, AU) and auval pass.
- Owner tested A/B in Logic; 9a done.
- Next step: 9b undo/redo.

### 2026-09-29 — M8 done

- Owner tested Linear phase mode in Logic and marked M8 done.

### 2026-09-29 — M8 step 3 (phase mode menus)

- Done: `PhaseModeControls` (src/ui): Zero latency / Linear phase and a length menu showing each length's latency in
  ms at the current sample rate (96 / 181 / 352 ms at 48 kHz); the length menu is greyed in Zero latency mode.
  Placed on the right of the top bar instead of the planned bottom bar: at the minimum width (960 px) the bottom bar
  has no room for two more menus.
- Tests added / passing: 245/245. Menus drive the processor and follow it (session load, sample rate change);
  layout and readability at three sizes, clear of the preset browser. Snapshots checked. pluginval strictness 5
  (VST3, AU) and auval pass.
- Next step: owner's listening check in Logic (mode switch, the three lengths, parameter changes, latency
  compensation against another track).

### 2026-09-29 — M8 step 2 (linear-phase audio path)

- Done: `LinearPhaseEngine` (own partitioned convolution, 2 or 4 paths), `LinearPhaseUpdater` (background thread,
  20 ms poll, designs on change and hands filters over through a preallocated slot), processor wiring: hidden state
  properties linearPhase / linearPhaseLength (state version 4), latency taps/2 + 512 reported, static bands run as
  the FIR and dynamic bands as IIR after it, mode/length switches with fade and history wait.
- Found while testing: juce::dsp::Convolution restarts each new filter with an empty history, so every parameter
  change in Linear phase mode silenced up to half the filter length; replaced by our own engine (Decisions). A
  length change also glitched because the fade-in began on a block still made by the old filter; readiness is now
  checked before processing. In Mid/Side chains the cross terms depend on phase, so they are compared with the
  zero-phase model rather than with Zero latency mode.
- Tests added / passing: 243/243 (three parallel runs). State and latency reporting; measured delay = reported at
  44.1/48/96/192 kHz for all three lengths; symmetric impulse response; flat EQ = exact delay; null against Zero
  latency (Stereo/Left/Right) within 0.1 dB; Mid/Side chain matches the zero-phase model within 0.1 dB; parameter
  change reaches the audio in about 165 ms of wall-clock time without clicks; mode and length switches clean;
  dynamic bands act after the FIR; toggling dynamics clean; no allocation in processBlock while filters swap (the
  allocation counter now counts only the audio thread); CPU at 32768 taps with Mid/Side at 96 kHz: Debug 102 ms per
  second of audio, Release 9.5 ms. pluginval strictness 5 (VST3, AU) and auval pass.
- Known transient: toggling a band's dynamics in Linear phase mode briefly (until the next filter, about 70-170 ms)
  applies that band twice or not at all.
- Next step: M8 step 3, phase mode and length menus in the bottom bar.

### 2026-09-29 — M8 step 1 (linear-phase FIR design)

- Done: `LinearPhaseDesigner` (src/dsp): active, non-dynamic bands -> zero-phase 2x2 matrix sampled on a 4x oversampled
  FFT grid -> symmetric Blackman-Harris-windowed FIRs of 8192/16384/32768 taps (cross terms only with Mid/Side bands).
  Release: about 50 ms per 32768-tap stereo design (background thread in step 2).
- Found while testing: a zero of a response (band pass or low cut at DC) could round to a slightly negative power and
  give NaN taps; clamped at zero. Accuracy depends on bandwidth, not only frequency: unresolved examples are a 100 Hz
  Q4 bell (0.57 dB off at 16384 taps, 48 kHz) and a 1 kHz Q8 notch (-22 dB deep instead of -59 at 8192 taps).
- Tests added / passing: 232/232. Tap counts and latency; flat EQ = pure N/2 delay; exact symmetry; accuracy for
  resolved bands at 44.1/48/96 kHz (bounds in Decisions); cut slope one and two octaves out; stereo matrix terms for
  all five channel modes; dynamic bands left out. Hidden diagnostic "[.diag]" prints the full accuracy table.
  pluginval strictness 5 (VST3, AU) and auval pass (audio path unchanged).
- Next step: M8 step 2, audio path (convolution, latency, background updates, mode switch).

### 2026-09-29 — M8 planning and detector change (step 0)

- Decisions recorded (phase mode as state property; same tap count at every rate; 8k/16k/32k taps; dynamic bands as
  IIR after the FIR, detecting the delayed signal; classic follower for Peak and RMS).
- Done: `LevelDetector` now runs attack/release on the linear level and converts to dB afterwards.
- Tests added / passing: 225/225. Attack/release time constants on the linear level; release falls 8.69 dB per
  release time (RMS: analytic value with its 10 ms mean square, -6.78 dB after the first 100 ms); at default
  10/100 ms a steady sine reads within 1.3 dB of its peak (worst measured -1.16 dB), RMS reads the RMS. Stage 2
  processing tests pass unchanged. pluginval strictness 5 (VST3, AU) and auval pass.
- Next step: M8 step 1, linear-phase FIR design.

### 2026-09-28 — M7 done

- Owner tested dynamics in Logic and marked M7 done. The side-chain was not tried in Logic (routing not found); it is
  covered by unit tests only. The Peak detector reading stays an open decision in CLAUDE.md.

### 2026-09-28 — M7 stage 3

- Done: band panel dynamics section (Dynamic switch, Range/Ratio, Peak/RMS, Side-chain, Thresh, Range, Ratio, Attack,
  Release); the switch is active for Bell and shelves, its settings only while it is on, Ratio only in Ratio mode.
  Panel widened to 780-1240 px. Display: a dynamic band's curve follows its live gain (of L and R, the channel moving
  further), with a thin static curve and a faint area to static + range edged by a dashed line; the sum follows the
  live curves; curves recompute when a live change moves 0.05 dB or more. Preset format 2 stores dynamics
  (<Dynamics> per band); format 1 files load with dynamics off; factory presets unchanged (dynamics off).
- Tests added / passing: 223/223. Preset format 2 round trip, format 1 compatibility, clamping, capture/apply and
  modified state; live, static and range curves, 0.05 dB recompute step, non-dynamic bands ignore live gain; panel
  attachments both ways, greying rules, layout and menu readability at three sizes; display curve at the processor's
  live gain after real audio. pluginval strictness 5 (VST3, AU) and auval pass. Snapshots checked at three sizes.
- Simplification: a Stereo band whose L and R move differently is drawn with the larger change, not as split curves.
- Next step: owner's listening check in Logic; Peak detector decision (open).

### 2026-09-28 — Rename to Spectral Fault (under M7)

- Done: product "Spectral Fault", company "Catastrophic Audio", bundle ID com.catastrophicaudio.spectralfault, CMake
  target SpectralFault and test binary SpectralFaultTests; the top bar shows the new name (JucePlugin_Name). Codes
  Ctcd/Peq1 and the state and preset XML tags unchanged. User presets move to the new folder, with a one-time copy
  from the old one.
- Tests added / passing: 211/211 (new and old folder paths; copy once, skip when the new folder has presets, nothing
  without an old folder; temporary folders never migrate). pluginval strictness 5 passes on both new bundles.
- Removed the old installed ParametricEQ.component/.vst3 (owner's OK) and restarted the AU registry; `auval -a` lists
  "Catastrophic Audio: Spectral Fault" and auval passes.

### 2026-09-28 — M7 stage 2

- Done: dynamic parameters reach the bands; optional "Sidechain" input bus (disabled by default; disabled, mono or
  stereo); `CascadeProcessor` per-channel coefficients; `EqBand` dynamic path: detector filter (bell band pass,
  shelf lowpass/highpass at Q 0.71) and a `LevelDetector` per detection channel, own input or side-chain (falls back
  to own input when none is connected), gain redesigned every 16 samples (per channel for Stereo bands, only when
  the change moves), threshold/range/ratio smoothed over 20 ms; live gain change per band and channel for the UI.
- Tests added / passing: 209/209. Steady-state cut and boost (Range, Ratio), bit-exact below threshold, attack and
  release timing, frequency selectivity, shelf detector regions, per-channel and Side detection, side-chain bus
  layouts, side-chain triggering and fallback, bursts and sweep (finite, step < 0.2), reset(), no allocation with 16
  dynamic bands plus side-chain. CPU, 16 dynamic stereo bells at 96 kHz: Debug 137 ms per second of audio,
  Release 18.7 ms. pluginval strictness 5 (VST3, AU) and auval pass; auval lists the side-chain bus.
- Found: the Peak detector smooths |x| in the dB domain, so a steady sine reads below its peak: at -10 dBFS, 1 kHz,
  -10.9 dB with 1/50 ms and -12.4 dB with the default 10/100 ms (RMS reads -13.0). Range-mode tests saturate and
  do not show it; the Ratio test now checks the law against the detector's actual reading. Raised with the owner.
- Next step: stage 3, panel dynamic controls, live gain and range on the display, preset format v2.

### 2026-09-28 — M7 stage 1

- Done: processor `reset()` (band filters, crossfades, output ramp); `LevelDetector` (Peak/RMS, attack/release in dB);
  `DynamicGainLaw` (Range and Ratio); nine dynamic parameters per band (hint 4), 259 parameters in total, not yet
  used by the audio path.
- Tests added / passing: 194/194. Detector time constants and readings; both gain laws (thresholds, knees, slope,
  cap, sign); parameters and defaults; pre-M7 sessions load with dynamics off; reset() stops a ringing tail.
  pluginval strictness 5 (VST3 in 4 s, AU) and auval pass.
- Resolved: the missing reset() noted in M6.
- Next step: stage 2, dynamic gain in EqBand (per channel, every 16 samples), detector filters, side-chain bus.

### 2026-09-28 — M6b

- Research: an agent collected EQ starting points for common sources from public mixing guides (at least two per
  instrument, with quotes). Frequencies and directions were well sourced; most dB and Q values were not, and were
  chosen conservatively. Owner kept 11 of 13 proposals and chose not to cite the sources in the repo.
- Done: `Preset`, `FactoryPresets`, `PresetManager` (src/presets); preset state in the session; top-bar browser
  (previous/next, name with "*", categories, User, Save As, Delete).
- Tests added / passing: 186/186. XML round trip and rejection; the 11 factory presets band by band; validity;
  loading sets exactly the preset and frees other bands, with matched gestures and no clicks; user presets in a
  temporary folder (save, overwrite, sort, load, delete, file names); next/previous; session restore of the name
  and modified state; top-bar wiring. pluginval strictness 5 (VST3, AU) and auval pass.
- Checked: the real preset folder stays empty (it was created by Logic on 2026-09-27, before M6b).
- 2026-09-28: owner tested in Logic. M6b marked done.
- Next step: M7 (dynamic EQ).

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
