#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <thread>

using namespace chordrunner;

namespace
{
struct Event
{
    int sample, note, channel;
    bool on;
    bool operator== (const Event&) const = default;
};
std::vector<Event> events (const juce::MidiBuffer& midi, int offset = 0)
{
    std::vector<Event> result;
    for (const auto event : midi)
    {
        const auto message = event.getMessage();
        if (message.isNoteOnOrOff())
            result.push_back ({ event.samplePosition + offset, message.getNoteNumber(), message.getChannel(), message.isNoteOn() });
    }
    return result;
}
std::vector<Event> renderPartitioned (int block)
{
    Timeline timeline;
    REQUIRE (parseProgression ("C1/16 G1/16 | Am1/8", timeline).wasOk());
    TimelineEngine engine;
    juce::MidiBuffer midi;
    midi.ensureSize (65536);
    std::vector<Event> result;
    double beat = 0.0;
    for (int start = 0; start < 96000; start += block)
    {
        const auto samples = std::min (block, 96000 - start);
        midi.clear();
        engine.render (timeline, beat, 137.0, 48000.0, samples, true, false, midi);
        auto batch = events (midi, start);
        result.insert (result.end(), batch.begin(), batch.end());
        beat += samples * 137.0 / (60.0 * 48000.0);
    }
    return result;
}
void prepare (PluginProcessor& processor)
{
    processor.setRateAndBufferSizeDetails (48000.0, 512);
    processor.prepareToPlay (48000.0, 512);
}
class TestPlayHead final : public juce::AudioPlayHead
{
public:
    juce::Optional<PositionInfo> getPosition() const override { return position; }
    juce::Optional<PositionInfo> position;
};
}

TEST_CASE ("Chord Runner identity and silent MIDI generator buses", "[processor]")
{
    PluginProcessor processor;
    CHECK (processor.getName() == "Chord Runner");
    CHECK (processor.getProgramName (0).isNotEmpty());
    CHECK (processor.acceptsMidi());
    CHECK (processor.producesMidi());
    CHECK (processor.isMidiEffect());
    CHECK (processor.getTotalNumInputChannels() == 0);
    prepare (processor);
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    for (int channel = 0; channel < 2; ++channel)
        juce::FloatVectorOperations::fill (audio.getWritePointer (channel), 1.0f, 512);
    midi.addEvent (juce::MidiMessage::noteOn (1, 40, static_cast<juce::uint8> (100)), 0);
    processor.processBlock (audio, midi);
    CHECK (audio.getMagnitude (0, 512) == 0.0f);
    CHECK (midi.isEmpty());
}

TEST_CASE ("Shorthand generates absolute root-position notes", "[parser]")
{
    Timeline timeline;
    REQUIRE (parseProgression ("C - G - Am - F", timeline).wasOk());
    REQUIRE (timeline.count == 4);
    CHECK (timeline.length() == 16.0);
    CHECK (timeline.chords[0].notes[0] == 60);
    CHECK (timeline.chords[0].notes[1] == 64);
    CHECK (timeline.chords[1].notes[2] == 74);
    CHECK (timeline.chords[2].notes[1] == 72);
    CHECK (timeline.chords[3].notes[2] == 72);
}

TEST_CASE ("Fractions and explicit suffixes measure bars", "[parser]")
{
    Timeline timeline;
    REQUIRE (parseProgression ("C1/2 G1/2 | Am2", timeline).wasOk());
    REQUIRE (timeline.count == 3);
    CHECK (timeline.chords[0].beats == 2.0);
    CHECK (timeline.chords[1].beats == 2.0);
    CHECK (timeline.chords[2].beats == 8.0);
    CHECK (timeline.length() == 12.0);
    REQUIRE (parseProgression ("C7@2 C@7 Cmaj7@1/2", timeline).wasOk());
    CHECK (timeline.chords[0].noteCount == 4);
    CHECK (timeline.chords[0].notes[3] == 70);
    CHECK (timeline.chords[0].beats == 8.0);
    CHECK (timeline.chords[1].beats == 28.0);
    CHECK (timeline.chords[2].beats == 2.0);
}

TEST_CASE ("Accidentals and common chord qualities", "[parser]")
{
    Timeline timeline;
    REQUIRE (parseProgression (juce::String::fromUTF8 ("F#m Bbmaj7 Cdim Caug Csus2 Csus4 Cm7 Cdim7 Cm7b5 C5 C♯m B♭"), timeline).wasOk());
    CHECK (timeline.chords[0].notes[0] == 66);
    CHECK (timeline.chords[0].notes[1] == 69);
    CHECK (timeline.chords[1].notes[3] == 81);
    CHECK (timeline.chords[2].notes[2] == 66);
    CHECK (timeline.chords[3].notes[2] == 68);
    CHECK (timeline.chords[4].notes[1] == 62);
    CHECK (timeline.chords[5].notes[1] == 65);
    CHECK (timeline.chords[6].notes[3] == 70);
    CHECK (timeline.chords[7].notes[3] == 69);
    CHECK (timeline.chords[8].notes[2] == 66);
    CHECK (timeline.chords[9].noteCount == 2);
    CHECK (timeline.chords[10].notes[0] == 61);
    CHECK (timeline.chords[11].notes[0] == 70);
}

TEST_CASE ("Malformed shorthand leaves the valid pattern intact", "[parser]")
{
    Timeline timeline;
    for (const auto* text : { "", "--- |", "H", "C1/0", "Am0", "C17", "C1/100", "Cfoo", "C/E", "C@NaN", "C#b", "C1/2junk" })
    {
        INFO (text);
        CHECK (parseProgression (text, timeline).failed());
        CHECK (timeline.count == 4);
        CHECK (timeline.chords[1].notes[0] == 67);
    }
    CHECK (parseProgression (juce::String::repeatedString ("C ", 65), timeline).failed());
    CHECK (parseProgression (juce::String::repeatedString ("C", 4097), timeline).failed());
    timeline.octave = 8;
    CHECK (parseProgression ("Bmaj7", timeline).failed());
}

TEST_CASE ("Held chords change on the exact sample with offs before ons", "[timing]")
{
    Timeline timeline;
    REQUIRE (parseProgression ("C1/2 G1/2 | Am2", timeline).wasOk());
    TimelineEngine engine;
    juce::MidiBuffer midi;
    midi.ensureSize (65536);
    engine.render (timeline, 0.0, 120.0, 48000.0, 48001, true, false, midi);
    auto result = events (midi);
    REQUIRE (result.size() == 9);
    CHECK (result[0] == Event { 0, 60, 1, true });
    CHECK (result[3] == Event { 48000, 60, 1, false });
    CHECK (result[6] == Event { 48000, 67, 1, true });
    midi.clear();
    engine.render (timeline, 48001.0 / 24000.0, 120.0, 48000.0, 512, true, false, midi);
    CHECK (midi.isEmpty());
    engine.stop (midi);
    REQUIRE (events (midi).size() == 3);
    CHECK_FALSE (events (midi)[0].on);
}

TEST_CASE ("Scheduling is independent of buffer partition", "[timing]")
{
    const auto expected = renderPartitioned (512);
    CHECK (renderPartitioned (1) == expected);
    CHECK (renderPartitioned (17) == expected);
    CHECK (renderPartitioned (4096) == expected);
}

TEST_CASE ("Loop retriggers a single chord and releases the old channel", "[timing]")
{
    Timeline timeline;
    REQUIRE (parseProgression ("C1/16", timeline).wasOk());
    TimelineEngine engine;
    juce::MidiBuffer midi;
    midi.ensureSize (65536);
    engine.render (timeline, 0.0, 120.0, 48000.0, 6001, true, false, midi);
    auto result = events (midi);
    REQUIRE (result.size() == 9);
    CHECK (result[3] == Event { 6000, 60, 1, false });
    CHECK (result[6] == Event { 6000, 60, 1, true });
    timeline.channel = 4;
    midi.clear();
    engine.render (timeline, 6001.0 / 24000.0, 120.0, 48000.0, 512, true, true, midi);
    REQUIRE (events (midi).size() == 6);
    CHECK (events (midi)[0].channel == 1);
    CHECK (events (midi)[3].channel == 4);
}

TEST_CASE ("Seek, preroll and stop do not leave held notes", "[timing]")
{
    Timeline timeline;
    TimelineEngine engine;
    juce::MidiBuffer midi;
    midi.ensureSize (65536);
    engine.render (timeline, -0.01, 120.0, 48000.0, 512, true, false, midi);
    REQUIRE (events (midi).size() == 3);
    CHECK (events (midi)[0].sample == 240);
    midi.clear();
    engine.render (timeline, 8.0, 120.0, 48000.0, 512, true, false, midi);
    REQUIRE (events (midi).size() == 6);
    CHECK (events (midi)[0] == Event { 0, 60, 1, false });
    CHECK (events (midi)[3] == Event { 0, 69, 1, true });
    midi.clear();
    engine.render (timeline, 8.1, 120.0, 48000.0, 512, false, false, midi);
    REQUIRE (events (midi).size() == 3);
    CHECK (engine.activeChord() == -1);
    midi.clear();
    engine.stop (midi);
    CHECK (midi.isEmpty());
}

TEST_CASE ("Internal transport plays, stops and restores a silent session", "[processor][state]")
{
    PluginProcessor processor;
    prepare (processor);
    auto state = processor.getTimelineState();
    state.getDynamicObject()->setProperty ("progression", "C1/2 G1/2 | Am2");
    state.getDynamicObject()->setProperty ("tempo", 137.0);
    state.getDynamicObject()->setProperty ("channel", 3);
    REQUIRE (processor.setTimelineState (state).wasOk());
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    processor.setPlaying (true);
    processor.processBlock (audio, midi);
    REQUIRE (events (midi).size() == 3);
    CHECK (events (midi)[0].channel == 1);
    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    processor.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    processor.processBlock (audio, midi);
    REQUIRE (events (midi).size() == 3);
    CHECK_FALSE (events (midi)[0].on);
    CHECK_FALSE (static_cast<bool> (processor.getTransportState()["armed"]));
    PluginProcessor restored;
    restored.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    CHECK (restored.getTimelineState()["progression"].toString() == "C1/2 G1/2 | Am2");
    CHECK (static_cast<double> (restored.getTimelineState()["tempo"]) == 137.0);
    CHECK (static_cast<int> (restored.getTimelineState()["channel"]) == 1);
    restored.setStateInformation ("invalid", 7);
    CHECK (restored.getTimelineState()["progression"].toString() == "C1/2 G1/2 | Am2");
}

TEST_CASE ("Invalid persisted settings are rejected atomically", "[state]")
{
    PluginProcessor processor;
    for (const auto* field : { "tempo", "channel", "velocity", "octave", "schema" })
    {
        auto state = processor.getTimelineState();
        state.getDynamicObject()->setProperty (field, -1);
        CHECK (processor.setTimelineState (state).failed());
    }
    auto state = processor.getTimelineState();
    state.getDynamicObject()->setProperty ("tempo", std::numeric_limits<double>::quiet_NaN());
    CHECK (processor.setTimelineState (state).failed());
    state.getDynamicObject()->setProperty ("tempo", std::numeric_limits<double>::infinity());
    CHECK (processor.setTimelineState (state).failed());
    CHECK (static_cast<double> (processor.getTimelineState()["tempo"]) == 120.0);
}

TEST_CASE ("Host transport follows PPQ, stops and handles missing timing", "[processor][host]")
{
    PluginProcessor processor;
    TestPlayHead head;
    processor.setPlayHead (&head);
    prepare (processor);
    auto state = processor.getTimelineState();
    state.getDynamicObject()->setProperty ("hostSync", true);
    REQUIRE (processor.setTimelineState (state).wasOk());
    processor.setPlaying (true);
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    processor.processBlock (audio, midi);
    CHECK (midi.isEmpty());
    juce::AudioPlayHead::PositionInfo position;
    position.setBpm (120.0);
    position.setPpqPosition (8.0);
    position.setIsPlaying (true);
    head.position = position;
    processor.processBlock (audio, midi);
    REQUIRE (events (midi).size() == 3);
    CHECK (events (midi)[0].note == 69);
    head.position = {};
    processor.processBlock (audio, midi);
    REQUIRE (events (midi).size() == 3);
    CHECK_FALSE (events (midi)[0].on);
    head.position = position;
    processor.processBlock (audio, midi);
    processor.panic();
    processor.processBlock (audio, midi);
    REQUIRE (events (midi).size() == 3);
    CHECK_FALSE (events (midi)[0].on);
    processor.processBlock (audio, midi);
    CHECK (midi.isEmpty());
    processor.setPlayHead (nullptr);
}

TEST_CASE ("Snapshot exchange always retains the latest edit", "[threading]")
{
    struct Snapshot { int generation = 0; std::array<int, 64> values {}; };
    SnapshotExchange<Snapshot> exchange;
    std::atomic<bool> finished { false };
    std::thread producer ([&]
    {
        for (int i = 1; i <= 10000; ++i)
        {
            Snapshot snapshot;
            snapshot.generation = i;
            snapshot.values.fill (i);
            exchange.publish (snapshot);
        }
        finished.store (true, std::memory_order_release);
    });
    bool coherent = true;
    int previous = 0;
    while (! finished.load (std::memory_order_acquire))
    {
        exchange.consume();
        const auto& current = exchange.current();
        coherent = coherent && current.generation >= previous;
        previous = current.generation;
        for (auto value : current.values)
            coherent = coherent && value == current.generation;
    }
    producer.join();
    exchange.consume();
    CHECK (coherent);
    CHECK (exchange.current().generation == 10000);
}

TEST_CASE ("Tempo edits preserve held notes and bypass releases them", "[processor][timing]")
{
    PluginProcessor processor;
    prepare (processor);
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    processor.setPlaying (true);
    processor.processBlock (audio, midi);
    REQUIRE (events (midi).size() == 3);
    auto state = processor.getTimelineState();
    state.getDynamicObject()->setProperty ("tempo", 150.0);
    REQUIRE (processor.setTimelineState (state).wasOk());
    processor.processBlock (audio, midi);
    CHECK (midi.isEmpty());
    processor.processBlockBypassed (audio, midi);
    REQUIRE (events (midi).size() == 3);
    CHECK_FALSE (events (midi)[0].on);
    midi.clear(); // The host supplies fresh input, not the previous callback's output.
    processor.processBlockBypassed (audio, midi);
    CHECK (midi.isEmpty());
    processor.processBlock (audio, midi);
    REQUIRE (events (midi).size() == 3);
    CHECK (events (midi)[0].on);
    processor.setPlaying (false);
    processor.processBlock (audio, midi);
    REQUIRE (events (midi).size() == 3);
    CHECK_FALSE (events (midi)[0].on);
}
