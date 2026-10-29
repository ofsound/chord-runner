#include <PluginProcessor.h>
#include <ui/NativeEditor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>

using namespace chordrunner;
namespace
{
double pitchCentre (const Chord& chord)
{
    double sum = 0.0;
    for (int i = 0; i < chord.noteCount; ++i) sum += chord.notes[static_cast<size_t> (i)];
    return sum / chord.noteCount;
}
double travel (const Timeline& timeline)
{
    double sum = 0.0;
    for (int c = 1; c < timeline.count; ++c)
        for (int n = 0; n < timeline.chords[static_cast<size_t> (c)].noteCount; ++n)
            sum += std::abs (timeline.chords[static_cast<size_t> (c)].notes[static_cast<size_t> (n)]
                - timeline.chords[static_cast<size_t> (c - 1)].notes[static_cast<size_t> (n)]);
    return sum;
}
std::vector<int> pitchClasses (const Chord& chord)
{
    std::vector<int> result;
    for (int i = 0; i < chord.noteCount; ++i) result.push_back (chord.notes[static_cast<size_t> (i)] % 12);
    std::sort (result.begin(), result.end());
    return result;
}
}
TEST_CASE ("Smooth inversions reduce movement while preserving chord tones", "[voicing]")
{
    Timeline root, smooth;
    REQUIRE (parseProgression ("C G Am F", root).wasOk());
    smooth.voiceLeading = 1;
    REQUIRE (parseProgression ("C G Am F", smooth).wasOk());
    CHECK (travel (smooth) < travel (root));
    CHECK (smooth.chords[0].notes == root.chords[0].notes);
    for (int i = 0; i < root.count; ++i)
    {
        CHECK (pitchClasses (smooth.chords[static_cast<size_t> (i)]) == pitchClasses (root.chords[static_cast<size_t> (i)]));
        CHECK (smooth.chords[static_cast<size_t> (i)].beats == Catch::Approx (root.chords[static_cast<size_t> (i)].beats));
    }
}
TEST_CASE ("Directional voicings follow rising and falling targets and restart from the first chord", "[voicing]")
{
    for (int mode : { 2, 3 })
    {
        Timeline timeline;
        timeline.voiceLeading = mode;
        REQUIRE (parseProgression ("C C C C", timeline).wasOk());
        const double first = pitchCentre (timeline.chords[0]);
        for (int i = 1; i < timeline.count; ++i)
            CHECK ((pitchCentre (timeline.chords[static_cast<size_t> (i)]) - pitchCentre (timeline.chords[static_cast<size_t> (i - 1)])) * (mode == 2 ? 1 : -1) > 0.0);
        CHECK ((pitchCentre (timeline.chords[3]) - first) * (mode == 2 ? 1 : -1) >= 8.0);
        TimelineEngine engine;
        juce::MidiBuffer midi;
        engine.render (timeline, timeline.length(), 120, 48000, 1, true, false, midi);
        std::vector<int> ons;
        for (const auto event : midi)
            if (event.getMessage().isNoteOn()) ons.push_back (event.getMessage().getNoteNumber());
        CHECK (ons == std::vector<int> { 60, 64, 67 });
        const auto previous = timeline.chords;
        REQUIRE (parseProgression ("C C C C", timeline).wasOk());
        for (int i = 0; i < timeline.count; ++i) CHECK (timeline.chords[static_cast<size_t> (i)].notes == previous[static_cast<size_t> (i)].notes);
    }
}
TEST_CASE ("Voicings preserve mixed chord sizes and MIDI bounds at extreme octaves", "[voicing]")
{
    for (int mode : { 1, 2, 3 })
        for (int octave : { 0, 4, 7 })
        {
            Timeline root, voiced;
            root.octave = voiced.octave = octave;
            voiced.voiceLeading = mode;
            REQUIRE (parseProgression ("C5 Fmaj9 Dm G13 C", root).wasOk());
            REQUIRE (parseProgression ("C5 Fmaj9 Dm G13 C", voiced).wasOk());
            for (int c = 0; c < voiced.count; ++c)
            {
                const auto& chord = voiced.chords[static_cast<size_t> (c)];
                CHECK (pitchClasses (chord) == pitchClasses (root.chords[static_cast<size_t> (c)]));
                for (int n = 0; n < chord.noteCount; ++n)
                {
                    CHECK (chord.notes[static_cast<size_t> (n)] >= 0);
                    CHECK (chord.notes[static_cast<size_t> (n)] <= 127);
                }
            }
        }
}
TEST_CASE ("Voice-leading modes save per pattern and undo without corrupting invalid edits", "[voicing][state]")
{
    PluginProcessor processor;
    chordrunner::ui::EditorController editor (processor);
    REQUIRE (editor.set ("voiceLeading", 2));
    REQUIRE (processor.copyPattern (0, 15).wasOk());
    juce::MemoryBlock data; processor.getStateInformation (data);
    PluginProcessor restored;
    restored.setStateInformation (data.getData(), static_cast<int> (data.getSize()));
    CHECK (static_cast<int> (restored.getProjectState()["patterns"][15]["voiceLeading"]) == 2);
    REQUIRE (editor.set ("voiceLeading", 3));
    CHECK_FALSE (editor.set ("voiceLeading", 99));
    editor.undo();
    CHECK (static_cast<int> (processor.getTimelineState()["voiceLeading"]) == 2);
    auto legacy = restored.getTimelineState();
    legacy.getDynamicObject()->removeProperty ("voiceLeading");
    REQUIRE (restored.setTimelineState (legacy).wasOk());
    CHECK (static_cast<int> (restored.getTimelineState()["voiceLeading"]) == 0);
}
