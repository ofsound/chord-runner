#include <PluginProcessor.h>
#include <ui/NativeEditor.h>
#include <catch2/catch_test_macros.hpp>

namespace
{
int revisionOf (PluginProcessor& p) { return static_cast<int> (p.getTimelineState()["revision"]); }
int firstTone (PluginProcessor& p) { return static_cast<int> (p.getTimelineState()["chords"][0]["notes"][0]); }
juce::MouseEvent pointerEvent (juce::Component& component, juce::Point<float> point, juce::Point<float> origin)
{
    return { juce::Desktop::getInstance().getMainMouseSource(), point,
        juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        &component, &component, juce::Time::getCurrentTime(), origin, juce::Time::getCurrentTime(), 1, true };
}
}
TEST_CASE ("Dragging a printed chord tone snaps vertically and forms one undo transaction", "[tones][native-ui]")
{
    PluginProcessor processor;
    chordrunner::ui::EditorController controller (processor);
    chordrunner::ui::TimelineView grid (controller);
    grid.setSize (846, 474);
    const auto image = grid.createComponentSnapshot (grid.getLocalBounds());
    REQUIRE (image.isValid());
    const juce::Point<float> origin { 100.0f, 424.0f };
    grid.mouseDown (pointerEvent (grid, origin, origin));
    grid.mouseDrag (pointerEvent (grid, { 100.0f, 384.0f }, origin));
    CHECK (firstTone (processor) == 62);
    grid.mouseDrag (pointerEvent (grid, { 100.0f, 364.0f }, origin));
    CHECK (firstTone (processor) == 63);
    grid.mouseDrag (pointerEvent (grid, { 100.0f, 344.0f }, origin)); // E is already present.
    CHECK (firstTone (processor) == 63);
    grid.mouseDrag (pointerEvent (grid, { 100.0f, 324.0f }, origin));
    CHECK (firstTone (processor) == 65);
    grid.mouseUp (pointerEvent (grid, { 100.0f, 324.0f }, origin));
    controller.undo();
    CHECK (firstTone (processor) == 60);
    CHECK_FALSE (processor.canUndo());
    controller.redo(); CHECK (firstTone (processor) == 65);
}
TEST_CASE ("Escape cancels a tone drag and external pattern changes stop a stale drag", "[tones][native-ui]")
{
    PluginProcessor processor;
    chordrunner::ui::EditorController controller (processor);
    chordrunner::ui::TimelineView grid (controller);
    grid.setSize (846, 474);
    const auto image = grid.createComponentSnapshot (grid.getLocalBounds());
    REQUIRE (image.isValid());
    const juce::Point<float> origin { 100.0f, 424.0f };
    grid.mouseDown (pointerEvent (grid, origin, origin));
    grid.mouseDrag (pointerEvent (grid, { 100.0f, 384.0f }, origin));
    REQUIRE (grid.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK (firstTone (processor) == 60);
    grid.mouseDown (pointerEvent (grid, origin, origin));
    processor.selectPattern (1);
    grid.mouseDrag (pointerEvent (grid, { 100.0f, 384.0f }, origin));
    CHECK (firstTone (processor) == 60);
    CHECK_FALSE (processor.editChordTone (0, 0, 62, revisionOf (processor) - 1).wasOk());
}
TEST_CASE ("Manual chord tones survive edits, copy and restore and feed actual MIDI", "[tones][state][midi]")
{
    PluginProcessor processor;
    REQUIRE (processor.editChordTone (0, 0, 61, revisionOf (processor)).wasOk());
    chordrunner::ui::EditorController controller (processor);
    REQUIRE (controller.set ("gate", 70, true));
    CHECK (firstTone (processor) == 61);
    REQUIRE (processor.copyPattern (0, 15).wasOk());
    juce::MemoryBlock data; processor.getStateInformation (data);
    PluginProcessor restored;
    restored.setStateInformation (data.getData(), static_cast<int> (data.getSize()));
    CHECK (static_cast<int> (restored.getProjectState()["patterns"][15]["toneOffsets"][0][0]) == 1);
    CHECK (firstTone (restored) == 61);
    restored.setRateAndBufferSizeDetails (48000, 512); restored.prepareToPlay (48000, 512);
    restored.setPlaying (true);
    juce::AudioBuffer<float> audio (2, 512); juce::MidiBuffer midi;
    restored.processBlock (audio, midi);
    REQUIRE (! midi.isEmpty());
    CHECK ((*midi.begin()).getMessage().getNoteNumber() == 61);
    REQUIRE (restored.editChordTone (0, 0, 62, revisionOf (restored)).wasOk());
    midi.clear(); restored.processBlock (audio, midi);
    const auto first = (*midi.begin()).getMessage();
    CHECK (first.isNoteOff()); CHECK (first.getNoteNumber() == 61);
    bool newPitch = false;
    for (const auto event : midi)
        newPitch = newPitch || (event.getMessage().isNoteOn() && event.getMessage().getNoteNumber() == 62);
    CHECK (newPitch);
    REQUIRE (controller.set ("enabled", true, true));
    const auto preview = processor.getOutputPreview();
    REQUIRE (! preview.empty()); CHECK (preview[0].note == 61);
    controller.draft = "invalid!";
    CHECK_FALSE (controller.applyDraft()); CHECK (firstTone (processor) == 61);
    controller.draft = "C - G - Am - F";
    REQUIRE (controller.applyDraft()); CHECK (firstTone (processor) == 60);
    controller.undo(); CHECK (firstTone (processor) == 61);
}
TEST_CASE ("Invalid manual-tone state is rejected atomically", "[tones][state]")
{
    PluginProcessor processor;
    REQUIRE (processor.editChordTone (0, 0, 61, revisionOf (processor)).wasOk());
    auto state = processor.getTimelineState();
    (*(*state["toneOffsets"].getArray())[0].getArray()).set (0, 127);
    CHECK (processor.setTimelineState (state).failed());
    CHECK (firstTone (processor) == 61);
    CHECK (processor.editChordTone (0, 0, 128, revisionOf (processor)).failed());
    CHECK (processor.editChordTone (0, 0, 64, revisionOf (processor)).failed());
    CHECK (firstTone (processor) == 61);
}

TEST_CASE ("Selected note timing is independent of chord bars and grid length is explicit", "[tones][state][midi]")
{
    PluginProcessor processor;
    const std::vector<chordrunner::NoteEdit> edits { { 0, 0, 60, 0.0, 2.0 }, { 0, 1, 64, 0.0, 2.0 }, { 0, 2, 67, 0.0, 2.0 }, { 1, 0, 67, 6.0, 2.0 }, { 1, 1, 71, 6.0, 2.0 }, { 1, 2, 74, 6.0, 2.0 } };
    REQUIRE (processor.editNotes (edits, revisionOf (processor)).wasOk());
    auto state = processor.getTimelineState();
    CHECK (static_cast<double> (state["chords"][1]["startBeat"]) == 4.0);
    CHECK (static_cast<double> (state["chords"][1]["noteStarts"][0]) == 6.0);
    CHECK (static_cast<double> (state["totalBeats"]) == 16.0);
    REQUIRE (processor.resizeLoop (24.0, revisionOf (processor)).wasOk());
    CHECK (static_cast<double> (processor.getTimelineState()["totalBeats"]) == 24.0);
    REQUIRE (processor.editNotes ({ { 3, 0, 65, 20.0, 4.0 } }, revisionOf (processor)).wasOk());
    CHECK_FALSE (processor.resizeLoop (16.0, revisionOf (processor)).wasOk());
    CHECK_FALSE (processor.editNotes ({ { 0, 0, 60, -1.0, 2.0 } }, revisionOf (processor)).wasOk());
    CHECK_FALSE (processor.editNotes ({ { 0, 0, 60, 0.0, std::numeric_limits<double>::infinity() } }, revisionOf (processor)).wasOk());
    CHECK_FALSE (processor.editNotes (edits, revisionOf (processor) - 1).wasOk());
    juce::MemoryBlock bytes;
    processor.getStateInformation (bytes);
    PluginProcessor restored;
    restored.setStateInformation (bytes.getData(), static_cast<int> (bytes.getSize()));
    CHECK (static_cast<double> (restored.getTimelineState()["totalBeats"]) == 24.0);
    CHECK (static_cast<double> (restored.getTimelineState()["chords"][3]["noteStarts"][0]) == 20.0);
    processor.undo();
    CHECK (static_cast<double> (processor.getTimelineState()["chords"][3]["noteStarts"][0]) == 12.0);
    processor.undo();
    CHECK (static_cast<double> (processor.getTimelineState()["totalBeats"]) == 16.0);
}

TEST_CASE ("Timed notes sustain across chord bars, chase seeks and release safely", "[tones][midi]")
{
    chordrunner::Timeline timeline;
    timeline.loopBeats = 20;
    timeline.chords[0].durations = { 6.0, 2.0, 2.0 };
    timeline.chords[1].startOffsets = { 2.0, 2.0, 2.0 };
    timeline.chords[1].durations = { 2.0, 2.0, 2.0 };
    chordrunner::TimelineEngine engine;
    juce::MidiBuffer midi;
    // One sample per hundredth beat: all scheduled edges have exact sample locations.
    engine.render (timeline, 0, 60, 100, 800, true, false, midi);
    bool longOff = false, delayedOn = false;
    for (const auto event : midi)
    {
        const auto message = event.getMessage();
        if (message.isNoteOff() && message.getNoteNumber() == 60)
        {
            CHECK (event.samplePosition == 600);
            longOff = true;
        }
        if (message.isNoteOn() && message.getNoteNumber() == 71)
        {
            CHECK (event.samplePosition == 600);
            delayedOn = true;
        }
    }
    CHECK (longOff);
    CHECK (delayedOn);
    midi.clear();
    engine.render (timeline, 5, 60, 100, 10, true, false, midi);
    bool chased = false;
    for (const auto event : midi)
        chased = chased || (event.getMessage().isNoteOn() && event.getMessage().getNoteNumber() == 60);
    CHECK (chased);
    midi.clear();
    engine.stop (midi);
    CHECK_FALSE (midi.isEmpty());
    midi.clear();
    engine.stop (midi);
    CHECK (midi.isEmpty());
}

TEST_CASE ("Chord heading selects tones for grouped resize and movement", "[tones][native-ui]")
{
    PluginProcessor processor;
    chordrunner::ui::EditorController controller (processor);
    chordrunner::ui::TimelineView grid (controller);
    grid.setSize (846, 474);
    REQUIRE (grid.createComponentSnapshot (grid.getLocalBounds()).isValid());
    auto click = [&] (juce::Point<float> point) {
        grid.mouseDown (pointerEvent (grid, point, point));
        grid.mouseUp (pointerEvent (grid, point, point));
    };
    click ({ 100, 35 }); // Select every tone in C.
    const float pixelsPerBeat = (static_cast<float> (grid.getWidth()) - 24.0f) / 16.0f;
    const juce::Point<float> edge { 24.0f + 4 * pixelsPerBeat - 4, 424 };
    const auto shortened = edge.translated (-2 * pixelsPerBeat, 0);
    grid.mouseDown (pointerEvent (grid, edge, edge));
    grid.mouseDrag (pointerEvent (grid, shortened, edge));
    grid.mouseUp (pointerEvent (grid, shortened, edge));
    for (int n = 0; n < 3; ++n)
        CHECK (static_cast<double> (processor.getTimelineState()["chords"][0]["noteDurations"][n]) == 2.0);
    controller.undo();
    click ({ 300, 35 }); // Select G, whose parsed start remains beat 4.
    const juce::Point<float> body { 300, 284 };
    grid.mouseDown (pointerEvent (grid, body, body));
    grid.mouseDrag (pointerEvent (grid, { 400, 284 }, body));
    grid.mouseUp (pointerEvent (grid, { 400, 284 }, body));
    for (int n = 0; n < 3; ++n)
        CHECK (static_cast<double> (processor.getTimelineState()["chords"][1]["noteStarts"][n]) == 6.0);

}

TEST_CASE ("Marquee selection moves every intersected source tone and escapes as one edit", "[tones][native-ui]")
{
    PluginProcessor processor;
    chordrunner::ui::EditorController controller (processor);
    chordrunner::ui::TimelineView grid (controller);
    grid.setSize (846, 474);
    REQUIRE (grid.createComponentSnapshot (grid.getLocalBounds()).isValid());
    const juce::Point<float> origin { 60, 250 }, end { 180, 435 };
    grid.mouseDown (pointerEvent (grid, origin, origin));
    grid.mouseDrag (pointerEvent (grid, end, origin));
    grid.mouseUp (pointerEvent (grid, end, origin));
    const juce::Point<float> note { 100, 424 }, moved { 150, 384 };
    grid.mouseDown (pointerEvent (grid, note, note));
    grid.mouseDrag (pointerEvent (grid, moved, note));
    const auto state = processor.getTimelineState();
    for (int n = 0; n < 3; ++n)
        CHECK (static_cast<double> (state["chords"][0]["noteStarts"][n]) == 1.0);
    CHECK (static_cast<int> (state["chords"][0]["notes"][0]) == 62);
    CHECK (static_cast<int> (state["chords"][0]["notes"][1]) == 66);
    CHECK (static_cast<int> (state["chords"][0]["notes"][2]) == 69);
    juce::FileOutputStream output (juce::File ("/tmp/chord-runner-note-editing.png"));
    output.setPosition (0);
    output.truncate();
    juce::PNGImageFormat().writeImageToStream (grid.createComponentSnapshot (grid.getLocalBounds()), output);
    REQUIRE (grid.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK (firstTone (processor) == 60);
    CHECK_FALSE (processor.canUndo());
}

TEST_CASE ("Edited timing is independent of buffer partitions including overlapping pitches", "[tones][midi]")
{
    chordrunner::Timeline timeline;
    timeline.chords[0].durations = { 6.25, 2.75, 7.0 };
    timeline.chords[1].startOffsets = { -1.5, 1.25, 0.5 };
    timeline.chords[1].durations = { 4.0, 2.0, 2.0 };
    timeline.loopBeats = 20.0;
    auto render = [&] (int block) {
        chordrunner::TimelineEngine engine;
        juce::MidiBuffer midi;
        std::vector<std::pair<int, int>> events;
        for (int sample = 0; sample < 4500; sample += block)
        {
            midi.clear();
            engine.render (timeline, sample / 100.0, 60, 100, juce::jmin (block, 4500 - sample), true, false, midi);
            for (const auto event : midi)
            {
                const auto message = event.getMessage();
                events.emplace_back (sample + event.samplePosition, message.getNoteNumber() + (message.isNoteOn() ? 128 : 0));
            }
        }
        return events;
    };
    const auto reference = render (1);
    CHECK (render (127) == reference);
    CHECK (render (512) == reference);
    CHECK (render (4500) == reference);
}
