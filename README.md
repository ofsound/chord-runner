# Chord Runner

**Chord Runner by ofsound** is a MIDI chord sequencer and arpeggiator. Type `C - G - Am - F`, press Apply, then Play. The native engine turns the shorthand into absolute MIDI notes and holds each chord for its duration. There is no audio instrument or DSP; route its MIDI to a synth.

Built with **JUCE 9.0.1**, C++23, CMake/Ninja, Catch2, and a **native JUCE** editor. The structure follows the sibling KSH and MIDI Phrases projects. Targets **Standalone, AU, and VST3** on macOS, with **AUv3** available through Xcode. The iOS target builds a universal **iPhone/iPad app with an embedded AUv3 MIDI extension** (iOS 15+). CLAP, inversions and advanced composition tools are outside the current scope.

## Build and run

Requires Xcode Command Line Tools, CMake 3.25+, and Ninja.

```sh
git submodule update --init --recursive
cmake -B Builds -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCHORD_RUNNER_COPY_PLUGINS=ON
cmake --build Builds --parallel 4
ctest --test-dir Builds --output-on-failure
open "Builds/ChordRunner_artefacts/Debug/Standalone/Chord Runner.app"
```

The editor is compiled C++ using JUCE controls and drawing. No Node.js, npm, WebView, browser bridge, or web server is required. AU and VST3 are copied into the user's `~/Library/Audio/Plug-Ins/` directories by default. Use `-DCHORD_RUNNER_COPY_PLUGINS=OFF` to keep builds entirely inside this checkout.

CMake remembers this option in each build directory. If the standalone has newer features than the installed plugin, run `cmake -B Builds -DCHORD_RUNNER_COPY_PLUGINS=ON`, then build all formats again. VS Code's configure settings explicitly enable installation for the development build. Reload plugin instances in the DAW after rebuilding; an already loaded instance continues using its previous binary.

Builds default to **Debug**. For music-making or performance testing, use a separate Release directory:

```sh
cmake -B Builds-Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build Builds-Release --parallel 4
```

Local builds target the current architecture. Add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` for universal macOS binaries. Minimum macOS version is 11.0. CI builds and tests macOS only; distribution signing and notarization are not configured.

## Shorthand

- An unsuffixed chord lasts **one bar**, with a fixed **4/4** meter.
- `C1/2 G1/2 | Am2` holds C for two beats, G for two beats, and Am for eight beats: a three-bar loop.
- Spaces, dashes and `|` separate chords. The bar character is visual grouping and does not insert silence or pad the timeline.
- Roots: A–G, with `#` / `b` or ♯ / ♭ accidentals.
- **77 chord types**, including major/minor/diminished/augmented triads, suspensions, added tones, sixths and six-nine chords, all four seventh/ninth/eleventh/thirteenth families, half-diminished extensions, altered dominants, and major/minor color chords. See the [complete suffix and alias reference](docs/chords.md).
- Open **Chords** in the Sequence module. Search by suffix, name, alias or formula; filter by family; choose an example root; and add a chord to the draft without interrupting playback. Select a row to see its aliases, then Add to draft or double-click. The formulas show the exact generated voicing. The parser and glossary use the same `assets/chords.json` catalog.
- Examples: `Dm9 - G13b9 - Cmaj9`, `Fm9 - Bb13 - Ebmaj9 - Abmaj7#11`, or `Dm7b5 - G7(b9,#11) - Cm(maj9)`. Suffixes are case-sensitive; `m7` is minor and `M7` is major. Aliases include `Δ7`, `ø7`, `°7`, `+7`, and parenthesized alterations explicitly listed in the glossary.
- Qualities take precedence over numeric durations: `C6`, `C9`, `C11` and `C13` are chords lasting one bar. Use `@`, `x`, or `X` to make a duration explicit (`C7@2`, `C7x2`, and `C7X2` are equivalent): `C9@2` holds a ninth chord for two bars; `C@9` holds a major triad for nine bars. Existing unambiguous forms such as `Am2` and `C1/2` still work. Write `C6add9` or `C69` for a six-nine chord; `C6/9` remains a fractional bar duration.
- Extensions include every degree shown in the glossary, in root position; full 11ths and 13ths can sound dense. Added-tone chords omit the seventh. `7alt` uses the specific seven-note altered-dominant formula listed in the glossary.
- Durations support fractions and decimals, from 1/16 to 16 bars. Up to 64 chords and 4096 input characters.
- Notes use scientific pitch notation: **C4 = MIDI 60**. The octave control chooses each chord's root octave. Root position is the default; the Sequence module also offers Smooth, Ramp Up and Ramp Down automatic voicings. Slash-bass notation remains unsupported.
- Invalid text reports an error and leaves the last valid pattern playing.

## Transport and routing

Internal mode is the default in every format. Play starts at the beginning, Stop releases held notes and resets the timeline. Playback loops. All notes off also disarms playback. The Sequencer and Arpeggiator switches are independent:

| Sequencer | Arpeggiator | MIDI output |
| --- | --- | --- |
| On | Off | Generated chord progression; incoming MIDI ignored |
| On | On | Generated progression through the arpeggiator; incoming MIDI ignored |
| Off | On | Arpeggiated incoming notes, preserving their channels and velocities |
| Off | Off | Incoming MIDI passed through with its timestamps |

The arpeggiator sits below the chord timeline. Its controls are grayed out while bypassed. Module power controls sit inside the upper-left corner of their modules. Play/Stop controls the sequencer. With the sequencer off and the internal clock selected, the arpeggiator responds immediately to MIDI input without pressing Play. Switching modules, host bypass and All notes off release held output notes and clear latched arpeggios. Host bypass passes incoming MIDI through.

**Standalone:** the custom JUCE holder creates **Chord Runner Output**, a virtual MIDI source on macOS. Select that source in a DAW or synth. JUCE's Options → Audio/MIDI Settings selects the audio device used to clock processing. Enable your MIDI keyboard in the same Audio/MIDI Settings dialog to feed the arpeggiator. Audio output is always silent. Quitting detaches the audio callback and flushes the MIDI output.

**Plugins:** MIDI goes to the host. The AU is a MIDI effect; the VST3 uses the same Fx/Instrument classification and silent compatibility bus as the siblings. For Ableton Live, use VST3 on a source MIDI track and route the plugin's MIDI to a second track containing the instrument. Host routing and transport capabilities vary; some hosts only process plugins while their own transport runs.

**Host sync:** select Host sync and Arm. Playback follows host tempo and quarter-note position, including seeks and host loops. Stop disarms the plugin. If host timing is unavailable or stopped, generated notes remain silent. This also applies to input arpeggiation when Host sync is selected; MIDI thru remains available. Fixed 4/4 shorthand durations are independent of the host's time signature. Restoring a project is stopped until explicitly armed or played.

Open/Save use `.chordrunner` JSON project files. Plugin sessions and standalone settings persist the progression, tempo, channel, velocity, octave, clock mode, module switches and arpeggiator settings, but never resume playback automatically.

## Arpeggiator

Controls follow [Ableton Live’s Arpeggiator reference](https://www.ableton.com/en/manual/live-midi-effect-reference/#arpeggiator), adapted to Chord Runner:

- **Style:** Up, Down, Up & Down, Down & Up, Converge, Diverge, Play Order, Chord Trigger, Random, Random Other and Random Once. Up & Down / Down & Up do not repeat end notes. Random Other draws without replacement each cycle; Random Once repeats its shuffled order until the input notes change.
- **Rate:** Sync uses quarter-note beat divisions (including dotted and triplet choices), or Free uses 10–2000 ms. Both use the native audio clock.
- **Gate:** 1–200% of the unswung rate; single-note styles always release the preceding note before a new attack, including with long gates, swing and humanize. Values above 100% can extend a final note into an otherwise empty interval. Chord Trigger remains the explicit polyphonic style.
- **Swing:** KSH’s 0–100% amount with **1×, 2×, 4×** subdivision groups (default 2×). Alternate groups are delayed by `subdivision × 0.5 × amount/100` steps. Delayed hits can cross later steps without blocking their playback.
- **Velocity humanize:** 0–100%, uniformly varies each hit by up to that percentage of its velocity, clamped to 1–127. **Timing humanize:** 0–100%, uniformly shifts each hit early or late by up to 20% of one step, around its swung position. Both use KSH’s formulas. Fixed pending storage preserves negative offsets and delays across callback boundaries.
- **Hold:** releasing keys keeps the pattern. Play a new chord after releasing all keys to replace it. While keys remain pressed, add notes or play an existing note again to remove it. Sustain pedal is consumed by the arp and retains released keys until pedal up.
- **Start step:** chooses the first position in the current arp cycle, displayed from 1 with the available count. Its range follows the active note pool (including held/sustained notes), order style and extra passes; without input it uses the four-note demo. Older saved offsets wrap to their equivalent cycle position. **OCTAVES** contains Range (1–9) and Order (By phrase / By note). By phrase plays the whole note order at each octave; By note plays each note across the octave range before advancing. Range behaves as follows: 1 plays the held notes, and each additional octave repeats the pattern one octave higher. Semitone-distance selection has been removed; older projects now use octave spacing. Notes outside MIDI 0–127 are omitted.
- **Retrigger:** Off continues the order; Note restarts with a new note; Beat restarts at the selected quarter-note interval. **Repeats** limits full pattern cycles or runs indefinitely at ∞.
- **Velocity:** moves each source velocity toward Target over Decay milliseconds; Target 0 fades to silence. Retrigger velocity restarts that slope with pattern retriggers.

This implements semitone transposition and KSH groove; it does not integrate Live’s scale awareness, Groove Pool, or MPE. The timeline continues to show the source chords, not an estimated arpeggio. Projects use schema 4 and contain the entire 16-slot bank. Schema 3 projects migrate the old swing amount to twice its value on the 1× grid to preserve their groove. Schema 1 and 2 projects preserve their previous chord notes and bar lengths by migrating numeric durations to explicit `@` notation; schema 1 also loads with Sequencer on and Arpeggiator off.

The engine stores up to 128 source notes across MIDI channels and 256 overlapping output voices. MIDI buffers are preallocated for the advertised block size, fastest supported rate, dense note retriggers and chord triggers. Input blocks exceeding 64 KiB are dropped as a whole and held output is released, preventing allocation or stuck notes on oversized input. SysEx and other MIDI pass through within that bound; sustain is handled internally when the arp is active.


## Performance bank, undo and projects

A pattern is the sequence, voicing/output settings, module power states and every arp/groove setting together. The bank has **16 named slots** plus **Mute**. Click a pad to select it. Shift-click a source pad to arm copying; it pulses between 70% and full brightness. Click a destination to copy the whole pattern and select it. Escape or Mute cancels. Double-click the pattern name to rename; Delete/Backspace on a focused pad resets it. Copy, reset and rename are undoable. MIDI note-ons **0–15** select slots 1–16, and note **16** selects Mute, matching KSH. These selection notes and their note-offs are consumed on every MIDI channel. The bank labels this performance-key range C0–D#1 / E1; chord pitches continue to use scientific notation (C4 = 60).

**Launch** selects Immediate, Next 1/16, Next beat, Next bar (default), or Pattern end. The selected/edited slot, sounding slot and queued slot are distinct. Queued pads show amber; sounding pads show green. A launch restarts the new pattern at its beginning, releasing old notes before new notes at the exact sample. Changing a queued choice replaces it. Stopping commits the queued choice silently; host discontinuities rebase the pending launch to the new clock position. Tempo and clock are project-wide.

**Undo / Redo**, Cmd-Z / Cmd-Shift-Z, cover committed shorthand, controls, names, copy/reset and project details. History survives closing the editor, holds up to 128 transactions, and clears on New/Open/host restore. Invalid edits leave state and history intact. Text fields retain their own draft undo.

The **Project** menu provides New, Open, Save, Save As, Previous/Next and a folder library. Save reuses the current destination; Save As chooses another. Project details include name, author, description and created/modified dates. A dirty marker shows unsaved content; replacement offers Save, Discard and Cancel. Files are validated as a whole before replacing the bank and are saved through a temporary file. Older single-pattern projects import into slot 1. Standalone restores its last session, Save destination and unsaved status, with transport stopped. A plugin’s host state stores the bank and metadata, without a machine-specific Save path.

Desktop projects default to `Documents/ofsound/Chord Runner`. On iOS, KSH’s coordinated document access is adapted for `.chordrunner`: Files open-in-place, security-scoped bookmarks, conflict detection and async document I/O. Files sharing is enabled. An optional shared iCloud library uses `-DCHORD_RUNNER_IOS_ICLOUD_ENABLED=ON` and requires a provisioned `iCloud.com.ofsound.chordrunner` container and Apple development team; the default local/Files workflow needs no iCloud entitlement.

The design uses restrained gradients, clear module boundaries and visible launch states. Melatonin blur was considered; no new blur dependency or per-frame blur work is needed for these opaque native panels.

## Native editor development

The JUCE editor lives in `source/ui/`. Controls call the same validated processor state API used by project/session restore; the engine remains the only parser and playback clock. Transport is polled on the message thread at 25 Hz, and the full state is fetched only after its revision changes.

Drag sliders, type in their value boxes, or double-click to reset. Slider edits apply live. A continuous drag or scroll burst is one undo step; separate drags remain separate steps. Sync/Free controls switch rate units; velocity-envelope and beat-retrigger controls enable their dependent values. The timeline draws the source chord bands and overlays the arp output as amber MIDI notes, including gate lengths, transposition, swing and humanize. The cached visual audition runs the production C++ schedulers outside the audio callback at a 4 kHz preview rate, with a maximum of 16 bars / 8192 notes. Random and humanized phrases illustrate one possible take. Input-only arpeggiation has no offline phrase to preview.

After editing source or build files, run `cmake --build Builds --parallel 4` and `ctest --test-dir Builds --output-on-failure`. Build all formats and reload plugin instances in the DAW after rebuilding. Native editor tests exercise control-to-engine mappings, invalid drafts, transport, state restore, glossary filtering, and project round trips.

## Layout

- `source/engine/`: shorthand parser, bounded timeline, sample-based MIDI scheduler and snapshot exchange.
- `source/PluginProcessor.*`: transport, native state and host integration.
- `source/PluginEditor.*`: native editor hosting and project dialogs.
- `source/ui/`: JUCE controls, piano-roll rendering, chord glossary and message-thread controller.
- `standalone/`: application lifecycle, settings and virtual MIDI output.
- `tests/`: parser, scheduling, host transport, state and concurrent snapshot tests.
- `cmake/`: unchanged Pamplejuce helpers; `cmake_project/`: application-specific build integration.

See `AGENTS.md` / `CLAUDE.md` for contributor requirements.

## Xcode and iOS development

Generate and open the macOS project:

```sh
cmake --preset macos-xcode
open Builds-Xcode/ChordRunner.xcodeproj
```

Generate the universal iPhone/iPad project, following KSH's app + AUv3 structure:

```sh
cmake --preset ios-xcode
open Builds-iOS/ChordRunner.xcodeproj
```

Select **ChordRunner_Standalone** and an iPhone or iPad simulator, then Run. The app embeds **ChordRunner_AUv3** automatically. A command-line simulator build is also available:

```sh
cmake --build --preset ios-simulator
```

The `ios-xcode` preset uses the device SDK (`iphoneos`) and enables signing; the simulator build preset overrides the SDK and disables signing for that build only. If an older generated project lists only simulators, rerun `cmake --preset ios-xcode --fresh` (reapply any local signing flags) and reopen it. A fresh configure clears cached simulator library paths as well as changing the SDK.

For a physical device, choose your development team in Signing & Capabilities for **both** Standalone and AUv3, select the connected device, and Run. To keep signing configuration through regeneration, configure with `-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=YOUR_TEAM_ID -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Automatic`. The `Assets` and `ChordRunner` static-library targets do not need signing or a team; signing is disabled for these targets in CMake. Generated projects live in ignored build folders; CMake is the source of truth.

iPhone uses five native pages with no scrolling: **Perform**, **Sequence**, **Arp**, **Groove**, and **Rhythm**. Perform exposes all 16 pads, mute, tempo, clock and launch timing; Sequence contains shorthand, voicing and the fitted timeline; Arp contains order, rate and pitch controls; Groove contains swing, humanize and the velocity envelope; Rhythm contains the gate and tie lanes. Transport stays visible. Landscape moves page navigation into a single toolbar and lays out controls in columns. Page selection and drafts survive rotation. Desktop and iPad share a fixed 1400×1050 (4:3) canvas. iPad fits it uniformly inside the safe area in either orientation, with centered margins where necessary; its header and controls never reflow with screen or host-window size. iPhone retains its compact pages. Standalone publishes **Chord Runner Output** through virtual MIDI on iOS, with background audio enabled; the AUv3 sends MIDI to its host. The app and AUv3 are simulator-buildable; physical-device MIDI routing, host integration and software-keyboard behavior still need device testing.

## Gate and tie rhythm lanes

Gate and Tie sit side by side beneath the desktop arp controls; use **Rhythm** on iPhone. Each lane has its own power switch, 1–8-step cycle and cell mask, with the label beside the taller desktop/iPad row. Legacy patterns longer than eight retain access to their saved steps in a second row. New patterns start with length four in both lanes: Gate cells on, Tie cells off, and both power switches off. Drag the grip at the active range edge to resize (arrow keys also adjust it). Click beyond the range to extend it; click a step to toggle it, or drag a lit step to move the hit on release. Shortening a cycle preserves its hidden cells. Escape cancels a drag, and each completed gesture is one undo step. Small landscape phones offer Gate/Tie subpages without scrolling.

Each cell advances on the arp base rate, in Sync or Free mode. Gate-off cells create rests while pitch order continues. An enabled tie cell continues the preceding held note/chord without a new attack, overriding a short gate so the original note does not stop between steps. Rests break ties; a tie after a rest starts a fresh note. The first step starts a note even if its tie cell is enabled. Entirely tied cycles sustain until a release/stop/reset; finite repeat counts still release. Cycles continue across note-triggered chord changes, while source-note replacement releases the previous chord safely. Transport stop resets both cycles.

Both lanes are saved per pattern, copied and undone together with the arp, validated on project import, and included in the amber output preview. Older projects default to bypassed lanes.

## Voice leading

The Sequence header’s voicing menu offers Root position, **Smooth**, **Ramp Up** and **Ramp Down**. Smooth selects inversions across the whole progression to reduce motion between neighboring voices. The ramps balance that movement against a pitch-center target rising/falling by approximately one octave from the first chord to the last. Short local deviations may occur to keep chord transitions musical. The selected octave anchors the first chord. Smooth also considers the return to the first chord. The ramps deliberately omit that return transition from their optimization: each loop restarts at the first voicing, preserving the intended rise or fall.

The native parser resolves voicings before publishing the timeline. Chord types, pitch classes, note counts and durations remain unchanged; notes stay within MIDI 0–127. The chord bands, held MIDI and arp input all use those same resolved notes. Mode selection is stored per pattern and participates in copy/recall/undo. Older sessions default to Root position.

## Editing printed chord tones

After Apply, drag a source note in the piano roll to move its pitch and start time. Drag either edge to resize it. Shift-click or Command-click adds/removes notes from the selection; drag empty space for marquee selection, Command-A selects all, and clicking a chord heading selects all its tones. Moving or resizing one selected note edits the whole selection together. Horizontal edits snap to 1/16 notes (a quarter beat); hold Alt for unsnapped timing. The parsed chord bars stay fixed. Double-click the bar count at the right of the ruler to enter a new loop length in bars; the grid always covers the progression and all notes, up to 1024 bars. Manual starts, lengths and grid size save with each pattern and feed both sequencer MIDI and arp input. Applying shorthand again resets manual timing and grid size. Hover highlights the tone and shows its pitch; the grid scale stays fixed during a drag. Shift-drag slows the adjustment. Each drag is one undo transaction; Escape cancels it. Duplicate pitches within a chord and pitches outside MIDI 0–127 are rejected. Edited chord labels gain an asterisk.

Edits change held MIDI and the arp input/amber phrase preview. They survive arp/tempo edits, pattern copying, project recall and host sessions. Apply regenerates the printed chords from shorthand; changing the octave or voice-leading mode also generates fresh voicings. Undo restores manual edits after regeneration. The shorthand retains its original chord symbols.

Single-note arp output is monophonic across MIDI channels. If swing places several steps at the same sample, only the newest nominal step attacks. Beat retrigger owns its downbeat: timing humanize cannot pretrigger that same downbeat in the preceding interval and then generate it again at the reset. The amber preview uses the same scheduling and removes zero-duration artifacts.
