# Chord Runner

**Chord Runner by ofsound** is a MIDI chord sequencer and arpeggiator. Type `C - G - Am - F`, press Apply, then Play. The native engine turns the shorthand into absolute MIDI notes and holds each chord for its duration. There is no audio instrument or DSP; route its MIDI to a synth.

Built with **JUCE 9.0.1**, C++23, CMake/Ninja, Catch2, and a **native JUCE** editor. The structure follows the sibling KSH and MIDI Phrases projects. Targets **Standalone, AU, and VST3** on macOS, with **AUv3** available through Xcode. The initial iOS port builds a universal **iPhone/iPad app with an embedded AUv3 MIDI extension** (iOS 15+). CLAP, inversions and advanced composition tools are outside this first pass.

## Build and run

Requires Xcode Command Line Tools, CMake 3.25+, and Ninja.

```sh
git submodule update --init --recursive
cmake -B Builds -G Ninja -DCMAKE_BUILD_TYPE=Debug
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
- Open **Chord glossary** in the header or **Chord suffixes & syntax** beside the progression. Search by suffix, name, alias or formula; filter by family; choose an example root; and add a chord to the draft without interrupting playback. Select a row to see its aliases, then Add to draft or double-click. The formulas show the exact generated voicing. The parser and glossary use the same `assets/chords.json` catalog.
- Examples: `Dm9 - G13b9 - Cmaj9`, `Fm9 - Bb13 - Ebmaj9 - Abmaj7#11`, or `Dm7b5 - G7(b9,#11) - Cm(maj9)`. Suffixes are case-sensitive; `m7` is minor and `M7` is major. Aliases include `Δ7`, `ø7`, `°7`, `+7`, and parenthesized alterations explicitly listed in the glossary.
- Qualities take precedence over numeric durations: `C6`, `C9`, `C11` and `C13` are chords lasting one bar. Use `@` to make a duration explicit: `C9@2` holds a ninth chord for two bars; `C@9` holds a major triad for nine bars. Existing unambiguous forms such as `Am2` and `C1/2` still work. Write `C6add9` or `C69` for a six-nine chord; `C6/9` remains a fractional bar duration.
- Extensions include every degree shown in the glossary, in root position; full 11ths and 13ths can sound dense. Added-tone chords omit the seventh. `7alt` uses the specific seven-note altered-dominant formula listed in the glossary.
- Durations support fractions and decimals, from 1/16 to 16 bars. Up to 64 chords and 4096 input characters.
- Notes use scientific pitch notation: **C4 = MIDI 60**. The octave control chooses each chord's root octave. Chords are in root position; slash chords and inversions are deliberately rejected for now.
- Invalid text reports an error and leaves the last valid pattern playing.

## Transport and routing

Internal mode is the default in every format. Play starts at the beginning, Stop releases held notes and resets the timeline. Playback loops. All notes off also disarms playback. The Sequencer and Arpeggiator switches are independent:

| Sequencer | Arpeggiator | MIDI output |
| --- | --- | --- |
| On | Off | Generated chord progression; incoming MIDI ignored |
| On | On | Generated progression through the arpeggiator; incoming MIDI ignored |
| Off | On | Arpeggiated incoming notes, preserving their channels and velocities |
| Off | Off | Incoming MIDI passed through with its timestamps |

The arpeggiator sits below the chord timeline. Its controls are grayed out while bypassed; the editor scrolls vertically in smaller windows. Play/Stop controls the sequencer. With the sequencer off and the internal clock selected, the arpeggiator responds immediately to MIDI input without pressing Play. Switching modules, host bypass and All notes off release held output notes and clear latched arpeggios. Host bypass passes incoming MIDI through.

**Standalone:** the custom JUCE holder creates **Chord Runner Output**, a virtual MIDI source on macOS. Select that source in a DAW or synth. JUCE's Options → Audio/MIDI Settings selects the audio device used to clock processing. Enable your MIDI keyboard in the same Audio/MIDI Settings dialog to feed the arpeggiator. Audio output is always silent. Quitting detaches the audio callback and flushes the MIDI output.

**Plugins:** MIDI goes to the host. The AU is a MIDI effect; the VST3 uses the same Fx/Instrument classification and silent compatibility bus as the siblings. For Ableton Live, use VST3 on a source MIDI track and route the plugin's MIDI to a second track containing the instrument. Host routing and transport capabilities vary; some hosts only process plugins while their own transport runs.

**Host sync:** select Host sync and Arm. Playback follows host tempo and quarter-note position, including seeks and host loops. Stop disarms the plugin. If host timing is unavailable or stopped, generated notes remain silent. This also applies to input arpeggiation when Host sync is selected; MIDI thru remains available. Fixed 4/4 shorthand durations are independent of the host's time signature. Restoring a project is stopped until explicitly armed or played.

Open/Save use `.chordrunner` JSON project files. Plugin sessions and standalone settings persist the progression, tempo, channel, velocity, octave, clock mode, module switches and arpeggiator settings, but never resume playback automatically.

## Arpeggiator

Controls follow [Ableton Live’s Arpeggiator reference](https://www.ableton.com/en/manual/live-midi-effect-reference/#arpeggiator), adapted to Chord Runner:

- **Style:** Up, Down, Up & Down, Down & Up, Converge, Diverge, Play Order, Chord Trigger, Random, Random Other and Random Once. Up & Down / Down & Up do not repeat end notes. Random Other draws without replacement each cycle; Random Once repeats its shuffled order until the input notes change.
- **Rate:** Sync uses quarter-note beat divisions (including dotted and triplet choices), or Free uses 10–2000 ms. Both use the native audio clock.
- **Gate:** 1–200% of the unswung rate; values above 100% overlap different pitches. Retriggering the same pitch releases its previous voice first.
- **Swing:** 0–50% alternately lengthens and shortens the step interval, preserving the pair’s duration.
- **Hold:** releasing keys keeps the pattern. Play a new chord after releasing all keys to replace it. While keys remain pressed, add notes or play an existing note again to remove it. Sustain pedal is consumed by the arp and retains released keys until pedal up.
- **Offset:** rotates the pattern’s starting position. **Steps** adds up to eight transposed passes; **Distance** sets each pass’s semitone shift (−36 to +36). Notes outside MIDI 0–127 are omitted.
- **Retrigger:** Off continues the order; Note restarts with a new note; Beat restarts at the selected quarter-note interval. **Repeats** limits full pattern cycles or runs indefinitely at ∞.
- **Velocity:** moves each source velocity toward Target over Decay milliseconds; Target 0 fades to silence. Retrigger velocity restarts that slope with pattern retriggers.

This implements semitone transposition and local swing; it does not integrate Live’s scale awareness, Groove Pool, or MPE. The timeline continues to show the source chords, not an estimated arpeggio. Settings use schema 3. Schema 1 and 2 projects preserve their previous chord notes and bar lengths by migrating numeric durations to explicit `@` notation; schema 1 also loads with Sequencer on and Arpeggiator off.

The engine stores up to 128 source notes across MIDI channels and 256 overlapping output voices. MIDI buffers are preallocated for the advertised block size, fastest supported rate, dense note retriggers and chord triggers. Input blocks exceeding 64 KiB are dropped as a whole and held output is released, preventing allocation or stuck notes on oversized input. SysEx and other MIDI pass through within that bound; sustain is handled internally when the arp is active.

## Native editor development

The JUCE editor lives in `source/ui/`. Controls call the same validated processor state API used by project/session restore; the engine remains the only parser and playback clock. Transport is polled on the message thread at 25 Hz, and the full state is fetched only after its revision changes.

Drag sliders, type in their value boxes, or double-click to reset. Arpeggiator slider gestures apply on release to avoid repeatedly restarting held notes. Sync/Free controls switch rate units; velocity-envelope and beat-retrigger controls enable their dependent values. The timeline draws the exact notes and beats returned by the processor.

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

For a physical device, choose your development team in Signing & Capabilities for **both** Standalone and AUv3, select the connected device, and Run. To keep signing configuration through regeneration, configure with `-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=YOUR_TEAM_ID -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Automatic`. Generated projects live in ignored build folders; CMake is the source of truth.

The initial phone layout uses scrollable sections for chords, timing, pitch and velocity, with Play/Stop and All Notes Off pinned below the scroll area. Both orientations are enabled. iPad uses the wider layout where space permits; narrow host windows adapt too. Standalone publishes **Chord Runner Output** through virtual MIDI on iOS, with background audio enabled; the AUv3 sends MIDI to its host. This is an initial development port: physical-device MIDI routing, AUv3 host integration, keyboard/orientation behavior and performance still need device testing.
