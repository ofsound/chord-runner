# Chord Runner contributor guide

Chord Runner by ofsound is a MIDI-only chord-pattern generator built with JUCE 9, C++23, CMake, Catch2 and a native JUCE editor. `AGENTS.md` is a symlink to this file.

## Configuration

- Internal target: `ChordRunner`; display name: `Chord Runner`.
- Company: `ofsound`; bundle: `com.ofsound.chordrunner`.
- Manufacturer: `Osnd`; unique plugin code: `Crn1`.
- Formats: Standalone, AU and VST3. MIDI effect with a silent output bus for host compatibility.
- JUCE 9 is explicitly requested by the user. Do not downgrade to the siblings' JUCE 8 version.
- Default Debug / Ninja builds use `Builds/`, matching the siblings' VS Code / Cursor setup. Use Release for music-making performance tests.
- macOS development CI only, no IPP, no distribution signing or notarization.

## Commands

```sh
git submodule update --init --recursive
cmake -B Builds -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build Builds --parallel 4
ctest --test-dir Builds --output-on-failure
```

After source, UI, or build changes, build **all formats**. Reconfigure when CMake changes. Fix all warnings in owned code before finishing; CMake enforces warnings as errors for source, standalone and tests. Do not edit vendored dependencies to silence warnings.

The editor is native JUCE C++ in `source/ui/`; there is no WebView, JavaScript bridge, npm build or dev server. Disable automatic user-folder plugin installation with `CHORD_RUNNER_COPY_PLUGINS=OFF` when needed.

## Product and syntax

Shorthand is the primary input. `C - G - Am - F` is four bars. `C1/2 G1/2 | Am2` is three bars. Durations are **bars**, one by default, fixed 4/4. C4 = MIDI 60. C++ is the single parser and scheduling authority; the UI renders the exact notes/start beats returned by C++.

Use `@` to disambiguate durations from quality numbers (`C7@2`). Slash chords/inversions are future work. Do not add separate JavaScript parsing or playback clocks. Invalid edits must preserve the last valid state.

## Architecture and realtime safety

`SharedCode` links the processor/editor into all plugin targets and Tests. `source/engine/` owns the bounded 64-chord / 8-note model and audio-thread scheduler. State and native calls serialize producers with `stateLock`, which is never acquired by the audio callback. `SnapshotExchange` uses three fixed slots with acquire/release index exchanges; never mutate an audio-visible slot from the UI.

No allocations, locks, file operations, UI callbacks or dynamic container growth in the audio hot path. Two MIDI buffers are preallocated in prepareToPlay and seeded into the JUCE wrappers' recycled buffers via constant-time swaps. Size for the advertised maximum block, fastest supported host tempo and shortest chord; never replace the recycled wrapper buffer with an unallocated buffer mid-stream. Use fixed storage and preallocate outside rendering. UI status is atomic and polled; full state is fetched only when its revision changes. Parse text, serialize JSON, and handle files only outside processing. Preserve finite-number validation (do not enable fast-math).

Always release held notes on stop, loop transitions, state replacement, seeks and channel changes. Standalone shutdown must detach the audio processor before flushing its physical MIDI endpoint; never call processBlock from the message thread while the audio callback is attached.

Sessions use versioned, validated JSON. Restore stopped. Keep single default program named for VST3 validation. Dummy audio is cleared every callback.

## Style

C++: Allman braces, four spaces, repository `.clang-format`. Use native JUCE controls and keep processor mutations in the message-thread editor controller. Keep implementation details out of the visible product UI. Reuse sibling project conventions where useful, without modifying those projects.
