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
