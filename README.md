# Chord Runner

**Chord Runner by ofsound** is a MIDI chord-pattern generator. Type `C - G - Am - F`, press Apply, then Play. The native engine turns the shorthand into absolute MIDI notes and holds each chord for its duration. There is no audio instrument or DSP; route its MIDI to a synth.

Built with **JUCE 9.0.1**, C++23, CMake/Ninja, Catch2, and an embedded **Svelte 5 / Vite / Tailwind** editor. The structure follows the sibling KSH and MIDI Phrases projects. This scaffold targets **Standalone, AU, and VST3** on macOS. AUv3, CLAP, mobile, inversions and advanced composition tools are not part of this first pass.

## Build and run

Requires Xcode Command Line Tools, CMake 3.25+, Ninja, and Node.js 22+ with npm.

```sh
git submodule update --init --recursive
cmake -B Builds -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build Builds --parallel 4
ctest --test-dir Builds --output-on-failure
open "Builds/ChordRunner_artefacts/Debug/Standalone/Chord Runner.app"
```

CMake installs locked npm dependencies, checks the Svelte UI, packages it into `assets/webview/ui.zip`, and embeds it in every format. No running web server or network connection is required by the finished app. AU and VST3 are copied into the user's `~/Library/Audio/Plug-Ins/` directories by default. Use `-DCHORD_RUNNER_COPY_PLUGINS=OFF` to keep builds entirely inside this checkout.

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
- Qualities: major (plain or `maj`), `m` / `min`, `7`, `maj7`, `m7` / `min7`, `dim`, `dim7`, `m7b5`, `aug`, `sus2`, `sus4`, and `5`.
- Qualities take precedence over numeric durations: `C7` is C dominant seventh. Use `@` to make a duration explicit: `C7@2` holds that seventh chord for two bars; `C@7` holds a major triad for seven bars.
- Durations support fractions and decimals, from 1/16 to 16 bars. Up to 64 chords and 4096 input characters.
- Notes use scientific pitch notation: **C4 = MIDI 60**. The octave control chooses each chord's root octave. Chords are in root position; slash chords and inversions are deliberately rejected for now.
- Invalid text reports an error and leaves the last valid pattern playing.

## Transport and routing

Internal mode is the default in every format. Play starts at the beginning, Stop releases held notes and resets the timeline. Playback loops. All notes off also disarms playback. Incoming MIDI is not passed through.

**Standalone:** the custom JUCE holder creates **Chord Runner Output**, a virtual MIDI source on macOS. Select that source in a DAW or synth. JUCE's Options → Audio/MIDI Settings selects the audio device used to clock processing. Audio output is always silent. Quitting detaches the audio callback and flushes the MIDI output.

**Plugins:** MIDI goes to the host. The AU is a MIDI effect; the VST3 uses the same Fx/Instrument classification and silent compatibility bus as the siblings. For Ableton Live, use VST3 on a source MIDI track and route the plugin's MIDI to a second track containing the instrument. Host routing and transport capabilities vary; some hosts only process plugins while their own transport runs.

**Host sync:** select Host sync and Arm. Playback follows host tempo and quarter-note position, including seeks and host loops. Stop disarms the plugin. If host timing is unavailable, the plugin remains silent. Fixed 4/4 shorthand durations are independent of the host's time signature. Restoring a project is stopped until explicitly armed or played.

Open/Save use `.chordrunner` JSON project files. Plugin sessions and standalone settings persist the progression, tempo, channel, velocity, octave and clock mode, but never resume playback automatically.

## UI development

```sh
npm ci --prefix ui
npm run dev --prefix ui        # dedicated port 5175
npm run check --prefix ui
```

Opt into live development with `-DCHORD_RUNNER_UI_DEV_SERVER=ON` in a Debug configure. Otherwise Debug and Release both load the embedded UI. The development-server option requires Vite to be running. A regular browser shows a preview with native MIDI actions disabled.

After editing `ui/`, `source/`, or build files, run the full `cmake --build Builds` so AU, VST3 and standalone contain the same editor. Reload plugin instances in the DAW after rebuilding.

## Layout

- `source/engine/`: shorthand parser, bounded timeline, sample-based MIDI scheduler and snapshot exchange.
- `source/PluginProcessor.*`: transport, native state and host integration.
- `source/PluginEditor.*`, `WebViewResourceProvider.*`: embedded browser, bridge and project dialogs.
- `standalone/`: application lifecycle, settings and virtual MIDI output.
- `ui/`: Svelte frontend; JUCE 9's checked-in WebView interop module is aliased from its submodule.
- `tests/`: parser, scheduling, host transport, state and concurrent snapshot tests.
- `cmake/`: unchanged Pamplejuce helpers; `cmake_project/`: application-specific build integration.

See `AGENTS.md` / `CLAUDE.md` for contributor requirements.
