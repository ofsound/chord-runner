#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <set>

using namespace chordrunner;

TEST_CASE ("Every glossary spelling produces its advertised chord formula", "[parser][vocabulary]")
{
    const auto* entries = chordVocabulary().getArray();
    REQUIRE (entries != nullptr);
    REQUIRE (entries->size() >= 70);
    std::set<juce::String> spellings;
    // Independent diatonic-degree interpretation checks the catalog's MIDI intervals.
    constexpr std::array<int, 14> degreeSemitones { 0, 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21 };
    for (const auto& entry : *entries)
    {
        INFO (entry["name"].toString().toStdString());
        REQUIRE (entry["name"].toString().isNotEmpty());
        REQUIRE (entry["family"].toString().isNotEmpty());
        const auto* degrees = entry["degrees"].getArray();
        const auto* intervals = entry["intervals"].getArray();
        REQUIRE (degrees != nullptr);
        REQUIRE (intervals != nullptr);
        REQUIRE (degrees->size() == intervals->size());
        REQUIRE (intervals->size() >= 2);
        REQUIRE (intervals->size() <= maxNotes);
        std::vector<int> expected;
        for (const auto& degree : *degrees)
        {
            auto name = degree.toString();
            int alteration = 0;
            while (name.startsWithChar ('b') || name.startsWithChar ('#'))
            {
                alteration += name[0] == 'b' ? -1 : 1;
                name = name.substring (1);
            }
            const int number = name.getIntValue();
            REQUIRE (number >= 1);
            REQUIRE (number <= 13);
            expected.push_back (60 + degreeSemitones[static_cast<size_t> (number)] + alteration);
        }
        juce::StringArray suffixes { entry["suffix"].toString() };
        REQUIRE (entry["aliases"].isArray());
        for (const auto& alias : *entry["aliases"].getArray())
            suffixes.add (alias.toString());
        for (const auto& suffix : suffixes)
        {
            INFO (suffix.toStdString());
            REQUIRE (spellings.insert (suffix).second);
            for (const auto& duration : { "", "@1/2", "@2" })
            {
                INFO (duration);
                Timeline timeline;
                REQUIRE (parseProgression ("C" + suffix + duration, timeline).wasOk());
                REQUIRE (timeline.count == 1);
                const auto& chord = timeline.chords[0];
                REQUIRE (chord.noteCount == static_cast<int> (expected.size()));
                for (int i = 0; i < chord.noteCount; ++i)
                {
                    CHECK (chord.notes[static_cast<size_t> (i)] == expected[static_cast<size_t> (i)]);
                    CHECK (static_cast<int> ((*intervals)[i]) == expected[static_cast<size_t> (i)] - 60);
                }
                CHECK (chord.beats == Catch::Approx (juce::String (duration).isEmpty() ? 4.0 : juce::String (duration) == "@2" ? 8.0 : 2.0));
                CHECK (juce::String::fromUTF8 (chord.label.data()) == "C" + suffix + duration);
            }
        }
    }
}

TEST_CASE ("Jazz progressions use exact root-position notes and explicit bar lengths", "[parser][vocabulary]")
{
    Timeline timeline;
    REQUIRE (parseProgression (juce::String::fromUTF8 ("Dm9@1/2 G13b9@1/2 | Cmaj9@2 Ebm(maj9) F♯7(b9,#11) Cø7 CΔ7"), timeline).wasOk());
    const std::vector<std::vector<int>> expected {
        { 62, 65, 69, 72, 76 }, { 67, 71, 74, 77, 80, 88 }, { 60, 64, 67, 71, 74 },
        { 63, 66, 70, 74, 77 }, { 66, 70, 73, 76, 79, 84 }, { 60, 63, 66, 70 }, { 60, 64, 67, 71 }
    };
    REQUIRE (timeline.count == static_cast<int> (expected.size()));
    for (int i = 0; i < timeline.count; ++i)
    {
        const auto& chord = timeline.chords[static_cast<size_t> (i)];
        REQUIRE (chord.noteCount == static_cast<int> (expected[static_cast<size_t> (i)].size()));
        for (int n = 0; n < chord.noteCount; ++n)
            CHECK (chord.notes[static_cast<size_t> (n)] == expected[static_cast<size_t> (i)][static_cast<size_t> (n)]);
    }
    CHECK (timeline.chords[0].beats == 2.0);
    CHECK (timeline.chords[1].beats == 2.0);
    CHECK (timeline.chords[2].beats == 8.0);
    REQUIRE (parseProgression ("C6 C9 C11 C13 C@6 C@9 C@11 C@13 C6add9 C6/9 Cadd2 Cadd9", timeline).wasOk());
    CHECK (timeline.chords[0].noteCount == 4);
    CHECK (timeline.chords[1].noteCount == 5);
    CHECK (timeline.chords[2].noteCount == 6);
    CHECK (timeline.chords[3].noteCount == 7);
    for (int i = 0; i < 4; ++i)
    {
        CHECK (timeline.chords[static_cast<size_t> (i)].beats == 4.0);
        CHECK (timeline.chords[static_cast<size_t> (i + 4)].noteCount == 3);
    }
    CHECK (timeline.chords[8].notes[4] == 74);
    CHECK (timeline.chords[9].noteCount == 3);
    CHECK (timeline.chords[9].beats == Catch::Approx (8.0 / 3.0));
    CHECK (timeline.chords[10].notes[1] == 62);
    CHECK (timeline.chords[11].notes[3] == 74);
}

TEST_CASE ("Legacy session migration preserves numeric durations and saved arpeggiator settings", "[state][vocabulary]")
{
    for (const int schema : { 1, 2 })
    {
        PluginProcessor processor;
        auto legacy = processor.getTimelineState();
        legacy.getDynamicObject()->setProperty ("schema", schema);
        legacy.getDynamicObject()->setProperty ("progression", "C6 - Dm9 - G13 - C7@2 - Cmaj72 - C6/9");
        legacy["arp"].getDynamicObject()->setProperty ("enabled", true);
        REQUIRE (processor.setTimelineState (legacy).wasOk());
        const auto migrated = processor.getTimelineState();
        CHECK (static_cast<int> (migrated["schema"]) == 4);
        CHECK (migrated["progression"].toString() == "C@6 - Dm@9 - G@13 - C7@2 - Cmaj7@2 - C@6/9");
        CHECK (static_cast<bool> (migrated["arp"]["enabled"]) == (schema == 2));
        const auto& chords = *migrated["chords"].getArray();
        CHECK (chords[0]["notes"].getArray()->size() == 3);
        CHECK (static_cast<double> (chords[0]["beats"]) == 24.0);
        CHECK (static_cast<double> (chords[1]["beats"]) == 36.0);
        CHECK (static_cast<double> (chords[2]["beats"]) == 52.0);
        CHECK (chords[4]["notes"].getArray()->size() == 4);
        juce::MemoryBlock saved;
        processor.getStateInformation (saved);
        PluginProcessor restored;
        restored.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
        CHECK (restored.getTimelineState()["progression"] == migrated["progression"]);
        CHECK (juce::JSON::toString (restored.getTimelineState()["chords"]) == juce::JSON::toString (migrated["chords"]));
        CHECK_FALSE (static_cast<bool> (restored.getTransportState()["armed"]));
    }
}

TEST_CASE ("Invalid jazz suffixes and out-of-range extensions preserve the last valid state", "[parser][vocabulary]")
{
    Timeline timeline;
    REQUIRE (parseProgression ("Dm9 G13 Cmaj9", timeline).wasOk());
    const auto original = timeline;
    for (const auto* invalid : { "C14@1", "Cmaj15@1", "C7(b9", "C7b9)", "C7(b9,,#11)", "C7b9#12", "Cmaj9/E", "C9@", "C9@@2", "C9@0", "C9@1/0", "C9@17", "C9@NaN", "C9@1/100", "C9@2foo" })
    {
        INFO (invalid);
        CHECK (parseProgression (invalid, timeline).failed());
        CHECK (timeline.count == original.count);
        CHECK (timeline.chords[1].notes == original.chords[1].notes);
        CHECK (timeline.chords[2].beats == Catch::Approx (original.chords[2].beats));
    }
    timeline.octave = 8;
    CHECK (parseProgression ("C13", timeline).failed());
    CHECK (timeline.chords[1].notes == original.chords[1].notes);
    timeline.octave = 4;
    CHECK (parseProgression (juce::String::repeatedString ("C13 ", 64), timeline).wasOk());
    CHECK (parseProgression (juce::String::repeatedString ("C13 ", 65), timeline).failed());
}
