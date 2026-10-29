#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <set>

using namespace chordrunner;

namespace
{
struct ArpEvent
{
    int sample, note, channel, velocity;
    bool on;
    bool operator== (const ArpEvent&) const = default;
};
std::vector<ArpEvent> readEvents (const juce::MidiBuffer& midi, int offset = 0)
{
    std::vector<ArpEvent> result;
    for (const auto event : midi)
    {
        const auto message = event.getMessage();
        if (message.isNoteOnOrOff())
            result.push_back ({ event.samplePosition + offset, message.getNoteNumber(), message.getChannel(), message.getVelocity(), message.isNoteOn() });
    }
    return result;
}
std::vector<int> noteOns (const juce::MidiBuffer& midi)
{
    std::vector<int> result;
    for (const auto event : midi)
        if (event.getMessage().isNoteOn())
            result.push_back (event.getMessage().getNoteNumber());
    return result;
}
void addChord (juce::MidiBuffer& midi, int sample = 0, int channel = 3)
{
    for (const auto note : { 60, 64, 67 })
        midi.addEvent (juce::MidiMessage::noteOn (channel, note, static_cast<juce::uint8> (100)), sample);
}
void configure (PluginProcessor& processor, bool sequencer, bool arp)
{
    auto state = processor.getTimelineState();
    state.getDynamicObject()->setProperty ("sequencerEnabled", sequencer);
    state["arp"].getDynamicObject()->setProperty ("enabled", arp);
    REQUIRE (processor.setTimelineState (state).wasOk());
}
void prepareArp (PluginProcessor& processor, int block = 512)
{
    processor.setRateAndBufferSizeDetails (48000.0, block);
    processor.prepareToPlay (48000.0, block);
}
std::vector<ArpEvent> partitionedArp (int block, ArpeggiatorSettings settings)
{
    Arpeggiator engine;
    juce::MidiBuffer input, output;
    input.ensureSize (65536);
    output.ensureSize (65536);
    std::vector<ArpEvent> result;
    for (int start = 0; start < 50000; start += block)
    {
        const int count = std::min (block, 50000 - start);
        input.clear();
        output.clear();
        if (start == 0)
            addChord (input, 0);
        engine.render (settings, input, output, start * 137.0 / (60.0 * 48000.0), 137.0, 48000.0, count, true, false);
        const auto batch = readEvents (output, start);
        result.insert (result.end(), batch.begin(), batch.end());
    }
    return result;
}
}

TEST_CASE ("Four module combinations select the correct MIDI source", "[arp][processor]")
{
    for (bool sequencer : { false, true })
        for (bool arp : { false, true })
        {
            PluginProcessor processor;
            configure (processor, sequencer, arp);
            prepareArp (processor);
            processor.setPlaying (true);
            juce::AudioBuffer<float> audio (2, 512);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (3, 40, static_cast<juce::uint8> (80)), 12);
            processor.processBlock (audio, midi);
            const auto notes = noteOns (midi);
            if (sequencer)
                CHECK (notes == (arp ? std::vector<int> { 60 } : std::vector<int> { 60, 64, 67 }));
            else
            {
                CHECK (notes == std::vector<int> { 40 });
                REQUIRE (! readEvents (midi).empty());
                CHECK (readEvents (midi)[0].sample == 12);
                CHECK (readEvents (midi)[0].channel == 3);
            }
            CHECK (audio.getMagnitude (0, 512) == 0.0f);
        }
}

TEST_CASE ("Input arpeggiation starts without sequencer Play and ends on key release", "[arp][processor]")
{
    PluginProcessor processor;
    configure (processor, false, true);
    prepareArp (processor);
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (5, 72, static_cast<juce::uint8> (91)), 27);
    midi.addEvent (juce::MidiMessage::noteOff (5, 72), 311);
    processor.processBlock (audio, midi);
    const auto events = readEvents (midi);
    REQUIRE (events.size() == 2);
    CHECK (events[0] == ArpEvent { 27, 72, 5, 91, true });
    CHECK (events[1] == ArpEvent { 311, 72, 5, 0, false });
}

TEST_CASE ("Arpeggiator styles, offset and transposition have defined note orders", "[arp]")
{
    const std::array<std::vector<int>, 8> expected {{
        {60,64,67,60,64,67}, {67,64,60,67,64,60}, {60,64,67,64,60,64},
        {67,64,60,64,67,64}, {60,67,64,60,67,64}, {64,67,60,64,67,60},
        {60,64,67,60,64,67}, {60,64,67,60,64,67,60,64,67,60,64,67,60,64,67,60,64,67}
    }};
    for (int style = 0; style < 8; ++style)
    {
        Arpeggiator engine;
        ArpeggiatorSettings settings;
        settings.style = style;
        juce::MidiBuffer input, output;
        addChord (input);
        engine.render (settings, input, output, 0, 120, 48000, 30001, true, false);
        CHECK (noteOns (output) == expected[static_cast<size_t> (style)]);
    }
    Arpeggiator engine;
    ArpeggiatorSettings settings;
    settings.steps = 2;
    settings.offset = 1;
    juce::MidiBuffer input, output;
    addChord (input);
    engine.render (settings, input, output, 0, 120, 48000, 48001, true, false);
    CHECK (noteOns (output) == std::vector<int> {64,67,72,76,79,84,88,91,60});
}

TEST_CASE ("Random Once repeats and Random Other exhausts its pool", "[arp]")
{
    for (int style : { ArpeggiatorSettings::randomOnce, ArpeggiatorSettings::randomOther })
    {
        Arpeggiator engine;
        ArpeggiatorSettings settings;
        settings.style = style;
        juce::MidiBuffer input, output;
        addChord (input);
        engine.render (settings, input, output, 0, 120, 48000, 30001, true, false);
        const auto notes = noteOns (output);
        REQUIRE (notes.size() == 6);
        CHECK (std::set<int> (notes.begin(), notes.begin() + 3) == std::set<int> {60,64,67});
        CHECK (std::set<int> (notes.begin() + 3, notes.end()) == std::set<int> {60,64,67});
        if (style == ArpeggiatorSettings::randomOnce)
            CHECK (std::equal (notes.begin(), notes.begin() + 3, notes.begin() + 3));
    }
}

TEST_CASE ("Gate, free rate and swing schedule exact samples", "[arp][timing]")
{
    Arpeggiator engine;
    ArpeggiatorSettings settings;
    settings.sync = false;
    settings.freeMs = 10;
    settings.gate = 50;
    settings.swing = 100;
    settings.swingSubdivision = 0;
    juce::MidiBuffer input, output;
    addChord (input, 11);
    engine.render (settings, input, output, 0, 137, 48000, 1200, true, false);
    CHECK (readEvents (output) == std::vector<ArpEvent> {
        {11,60,3,100,true}, {251,60,3,0,false}, {731,64,3,100,true},
        {971,64,3,0,false}, {971,67,3,100,true}
    });
}

TEST_CASE ("Arpeggiator timing is invariant across buffer sizes", "[arp][timing]")
{
    for (bool sync : { false, true })
    {
        ArpeggiatorSettings settings;
        settings.sync = sync;
        settings.freeMs = 17.3;
        settings.rate = 1.0 / 6.0;
        settings.swing = 37;
        settings.gate = 175;
        settings.retrigger = 2;
        settings.interval = 0.5;
        const auto expected = partitionedArp (512, settings);
        CHECK (partitionedArp (1, settings) == expected);
        CHECK (partitionedArp (17, settings) == expected);
        CHECK (partitionedArp (4096, settings) == expected);
    }
}

TEST_CASE ("Hold latches released keys, replaces a chord and panic clears it", "[arp]")
{
    Arpeggiator engine;
    ArpeggiatorSettings settings;
    settings.hold = true;
    juce::MidiBuffer input, output;
    addChord (input);
    for (int note : {60,64,67})
        input.addEvent (juce::MidiMessage::noteOff (3, note), 100);
    input.addEvent (juce::MidiMessage::noteOn (3, 72, static_cast<juce::uint8> (100)), 12001);
    input.addEvent (juce::MidiMessage::noteOff (3, 72), 12100);
    engine.render (settings, input, output, 0, 120, 48000, 18002, true, false);
    CHECK (noteOns (output) == std::vector<int> {60,64,67,72,72});
    output.clear();
    engine.stop (output);
    CHECK (noteOns (output).empty());
    REQUIRE (! readEvents (output).empty());
    input.clear();
    output.clear();
    engine.render (settings, input, output, 1, 120, 48000, 24000, true, false);
    CHECK (output.isEmpty());
}

TEST_CASE ("Turning Hold off drops released keys and retains pressed keys", "[arp]")
{
    Arpeggiator engine;
    ArpeggiatorSettings settings;
    settings.hold = true;
    juce::MidiBuffer input, output;
    addChord (input);
    input.addEvent (juce::MidiMessage::noteOff (3, 60), 100);
    input.addEvent (juce::MidiMessage::noteOff (3, 67), 100);
    engine.render (settings, input, output, 0, 120, 48000, 512, true, false);
    input.clear(); output.clear();
    settings.hold = false;
    engine.render (settings, input, output, 512.0 / 24000.0, 120, 48000, 512, true, true);
    CHECK (noteOns (output) == std::vector<int> {64});
}

TEST_CASE ("Sustain is consumed and releases arpeggiated tails on pedal up", "[arp]")
{
    Arpeggiator engine;
    ArpeggiatorSettings settings;
    juce::MidiBuffer input, output;
    addChord (input);
    input.addEvent (juce::MidiMessage::controllerEvent (3, 64, 127), 1);
    for (int note : {60,64,67})
        input.addEvent (juce::MidiMessage::noteOff (3, note), 100);
    input.addEvent (juce::MidiMessage::controllerEvent (3, 64, 0), 7000);
    engine.render (settings, input, output, 0, 120, 48000, 13000, true, false);
    CHECK (noteOns (output) == std::vector<int> {60,64});
    REQUIRE (! readEvents (output).empty());
    CHECK (readEvents (output).back() == ArpEvent {7000,64,3,0,false});
    for (const auto event : output)
        CHECK_FALSE (event.getMessage().isController());
}

TEST_CASE ("Finite repeats and beat retrigger leave rests", "[arp]")
{
    Arpeggiator engine;
    ArpeggiatorSettings settings;
    settings.repeats = 1;
    settings.retrigger = 2;
    settings.interval = 1;
    juce::MidiBuffer input, output;
    addChord (input);
    engine.render (settings, input, output, 0, 120, 48000, 24001, true, false);
    CHECK (noteOns (output) == std::vector<int> {60,64,67,60});
    CHECK (readEvents (output).back().sample == 24000);
}

TEST_CASE ("Velocity decay reaches target and optional note retrigger preserves its slope", "[arp]")
{
    for (bool retrigger : { false, true })
    {
        Arpeggiator engine;
        ArpeggiatorSettings settings;
        settings.velocityEnabled = true;
        settings.velocityRetrigger = retrigger;
        settings.target = 20;
        settings.decay = 250;
        juce::MidiBuffer input, output;
        addChord (input);
        input.addEvent (juce::MidiMessage::noteOn (3, 72, static_cast<juce::uint8> (100)), 12000);
        engine.render (settings, input, output, 0, 120, 48000, 12001, true, false);
        const auto result = readEvents (output);
        REQUIRE (! result.empty());
        CHECK (result.back().velocity == (retrigger ? 100 : 20));
    }
}

TEST_CASE ("Routing changes release passthrough and generated notes", "[arp][processor]")
{
    PluginProcessor processor;
    configure (processor, false, false);
    prepareArp (processor);
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    addChord (midi);
    midi.addEvent (juce::MidiMessage::controllerEvent (3, 64, 127), 1);
    processor.processBlock (audio, midi);
    configure (processor, true, true);
    processor.setPlaying (true);
    midi.clear();
    processor.processBlock (audio, midi);
    const auto result = readEvents (midi);
    REQUIRE (result.size() == 4);
    CHECK_FALSE (result[0].on);
    CHECK_FALSE (result[1].on);
    CHECK_FALSE (result[2].on);
    CHECK (result.back().note == 60);
    CHECK (result.back().channel == 1);
    configure (processor, false, false);
    midi.clear();
    processor.processBlock (audio, midi);
    REQUIRE (readEvents (midi).size() == 1);
    CHECK_FALSE (readEvents (midi)[0].on);
}

TEST_CASE ("Arpeggiator settings round trip, old sessions migrate and invalid edits are atomic", "[arp][state]")
{
    PluginProcessor processor;
    auto state = processor.getTimelineState();
    state.getDynamicObject()->setProperty ("sequencerEnabled", false);
    state["arp"].getDynamicObject()->setProperty ("enabled", true);
    state["arp"].getDynamicObject()->setProperty ("style", 9);
    state["arp"].getDynamicObject()->setProperty ("swing", 33.5);
    REQUIRE (processor.setTimelineState (state).wasOk());
    juce::MemoryBlock data;
    processor.getStateInformation (data);
    PluginProcessor restored;
    restored.setStateInformation (data.getData(), static_cast<int> (data.getSize()));
    CHECK_FALSE (static_cast<bool> (restored.getTimelineState()["sequencerEnabled"]));
    CHECK (static_cast<int> (restored.getTimelineState()["arp"]["style"]) == 9);
    CHECK (static_cast<double> (restored.getTimelineState()["arp"]["swing"]) == Catch::Approx (33.5));
    CHECK_FALSE (static_cast<bool> (restored.getTransportState()["armed"]));
    for (const auto* field : {"rate", "freeMs", "gate", "interval", "decay", "style", "steps", "offset", "repeats"})
    {
        auto invalid = processor.getTimelineState();
        invalid["arp"].getDynamicObject()->setProperty (field, -1);
        CHECK (processor.setTimelineState (invalid).failed());
        invalid["arp"].getDynamicObject()->setProperty (field, std::numeric_limits<double>::quiet_NaN());
        CHECK (processor.setTimelineState (invalid).failed());
    }
    state.getDynamicObject()->setProperty ("schema", 1);
    state.getDynamicObject()->removeProperty ("arp");
    state.getDynamicObject()->removeProperty ("sequencerEnabled");
    REQUIRE (processor.setTimelineState (state).wasOk());
    CHECK (static_cast<bool> (processor.getTimelineState()["sequencerEnabled"]));
    CHECK_FALSE (static_cast<bool> (processor.getTimelineState()["arp"]["enabled"]));
}

TEST_CASE ("MIDI thru retains controller, SysEx and note timestamps; host bypass releases the arp", "[arp][processor]")
{
    PluginProcessor processor;
    configure (processor, false, false);
    prepareArp (processor);
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    const std::array<juce::uint8, 4> bytes {1,2,3,4};
    midi.addEvent (juce::MidiMessage::createSysExMessage (bytes.data(), 4), 21);
    midi.addEvent (juce::MidiMessage::pitchWheel (2, 10000), 45);
    processor.processBlock (audio, midi);
    REQUIRE (midi.getNumEvents() == 2);
    CHECK ((*midi.begin()).samplePosition == 21);
    CHECK ((*midi.begin()).getMessage().isSysEx());
    configure (processor, false, true);
    midi.clear(); addChord (midi);
    processor.processBlock (audio, midi);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (4, 70, static_cast<juce::uint8> (90)), 17);
    processor.processBlockBypassed (audio, midi);
    CHECK (readEvents (midi) == std::vector<ArpEvent> {{0,60,3,0,false}, {17,70,4,90,true}});
}

TEST_CASE ("Preroll starts an evenly spaced arpeggio without a catch-up burst", "[arp][timing]")
{
    Arpeggiator engine;
    ArpeggiatorSettings settings;
    juce::MidiBuffer input, output;
    addChord (input);
    engine.render (settings, input, output, -0.01, 120, 48000, 6241, true, false);
    std::vector<int> starts;
    for (const auto event : output)
        if (event.getMessage().isNoteOn())
            starts.push_back (event.samplePosition);
    CHECK (starts == std::vector<int> {240,6240});
}

TEST_CASE ("Host seeks, loops, stops and channel edits release sequenced arpeggios", "[arp][host]")
{
    class Head final : public juce::AudioPlayHead
    {
    public:
        PositionInfo position;
        juce::Optional<PositionInfo> getPosition() const override { return position; }
    } head;
    PluginProcessor processor;
    processor.setPlayHead (&head);
    configure (processor, true, true);
    auto state = processor.getTimelineState();
    state.getDynamicObject()->setProperty ("hostSync", true);
    REQUIRE (processor.setTimelineState (state).wasOk());
    prepareArp (processor);
    processor.setPlaying (true);
    head.position.setBpm (120);
    head.position.setPpqPosition (0);
    head.position.setIsPlaying (true);
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    processor.processBlock (audio, midi);
    CHECK (noteOns (midi) == std::vector<int> {60});
    for (const auto beat : {8.0, 0.0})
    {
        midi.clear();
        head.position.setPpqPosition (beat);
        processor.processBlock (audio, midi);
        REQUIRE (readEvents (midi).size() == 2);
        CHECK_FALSE (readEvents (midi)[0].on);
        CHECK (readEvents (midi)[1].on);
    }
    state = processor.getTimelineState();
    state.getDynamicObject()->setProperty ("channel", 7);
    REQUIRE (processor.setTimelineState (state).wasOk());
    midi.clear();
    head.position.setPpqPosition (512.0 / 24000.0);
    processor.processBlock (audio, midi);
    REQUIRE (readEvents (midi).size() == 2);
    CHECK (readEvents (midi)[0].channel == 1);
    CHECK (readEvents (midi)[1].channel == 7);
    midi.clear();
    head.position.setIsPlaying (false);
    processor.processBlock (audio, midi);
    REQUIRE (readEvents (midi).size() == 1);
    CHECK_FALSE (readEvents (midi)[0].on);
    processor.setPlayHead (nullptr);
}

TEST_CASE ("Dense chord retriggers retain preallocated recycled MIDI storage", "[arp][realtime]")
{
    PluginProcessor processor;
    configure (processor, false, true);
    auto state = processor.getTimelineState();
    state["arp"].getDynamicObject()->setProperty ("style", ArpeggiatorSettings::chordTrigger);
    state["arp"].getDynamicObject()->setProperty ("gate", 200);
    REQUIRE (processor.setTimelineState (state).wasOk());
    prepareArp (processor);
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    std::set<const void*> storage;
    for (int i = 0; i < 2; ++i)
    {
        midi.clear();
        processor.processBlock (audio, midi);
        storage.insert (midi.data.getRawDataPointer());
    }
    for (int block = 0; block < 4; ++block)
    {
        midi.clear();
        for (int note = 17; note < 128; ++note)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, static_cast<juce::uint8> (100)), 0);
        for (int sample = 1; sample < 512; ++sample)
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), sample);
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (100)), sample);
        }
        processor.processBlock (audio, midi);
        CHECK (storage.contains (midi.data.getRawDataPointer()));
        CHECK (noteOns (midi).size() == 111 * 512);
    }
}

TEST_CASE ("Oversized MIDI input fails silent and clears latched output", "[arp][realtime]")
{
    PluginProcessor processor;
    configure (processor, false, true);
    prepareArp (processor);
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    addChord (midi);
    processor.processBlock (audio, midi);
    midi.clear();
    std::vector<juce::uint8> payload (40000, 1);
    const auto sysex = juce::MidiMessage::createSysExMessage (payload.data(), static_cast<int> (payload.size()));
    REQUIRE (midi.addEvent (sysex, 1));
    REQUIRE (midi.addEvent (sysex, 2));
    processor.processBlock (audio, midi);
    REQUIRE (readEvents (midi).size() == 1);
    CHECK_FALSE (readEvents (midi)[0].on);
    midi.clear();
    processor.processBlock (audio, midi);
    CHECK (midi.isEmpty());
}

TEST_CASE ("Held input count tracks physical keys without counting generated MIDI or selection notes", "[arp][processor]")
{
    PluginProcessor processor;
    configure (processor, false, true);
    prepareArp (processor);
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    auto count = [&] { return static_cast<int> (processor.getTransportState()["heldInputNotes"]); };
    addChord (midi);
    midi.addEvent (juce::MidiMessage::noteOn (3, 60, static_cast<juce::uint8> (100)), 1);
    processor.processBlock (audio, midi);
    CHECK (count() == 3);
    midi.clear();
    processor.processBlock (audio, midi);
    CHECK (count() == 3);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (4, 60, static_cast<juce::uint8> (100)), 0);
    midi.addEvent (juce::MidiMessage::noteOff (3, 60), 1);
    processor.processBlock (audio, midi);
    CHECK (count() == 3);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (3, 67, static_cast<juce::uint8> (0)), 0);
    processor.processBlock (audio, midi);
    CHECK (count() == 2);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 16, static_cast<juce::uint8> (100)), 0);
    processor.processBlock (audio, midi);
    CHECK (count() == 2);
    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (3, 123, 0), 0);
    processor.processBlock (audio, midi);
    CHECK (count() == 1);
    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (4, 120, 0), 0);
    processor.processBlock (audio, midi);
    CHECK (count() == 0);
    midi.clear();
    addChord (midi);
    processor.processBlock (audio, midi);
    REQUIRE (count() == 3);
    processor.panic();
    CHECK (count() == 0);
    midi.clear();
    processor.processBlock (audio, midi);
    CHECK (count() == 0);
}

TEST_CASE ("Output pulse telemetry advances only on emitted arp attacks", "[arp][processor]")
{
    PluginProcessor processor;
    configure (processor, false, true);
    prepareArp (processor);
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (91)), 100);
    processor.processBlock (audio, midi);
    const auto first = processor.getTransportState();
    CHECK (static_cast<juce::int64> (first["arpPulseSerial"]) == 1);
    CHECK (static_cast<juce::int64> (first["arpPulseStep"]) == 0);
    CHECK (static_cast<int> (first["arpPulseVelocity"]) == 91);
    CHECK (static_cast<double> (first["arpPulseSampleAgeMs"]) == Catch::Approx (412.0 / 48.0));
    midi.clear();
    processor.processBlock (audio, midi);
    CHECK (static_cast<juce::int64> (processor.getTransportState()["arpPulseSerial"]) == 1);
    auto state = processor.getTimelineState();
    state["arp"].getDynamicObject()->setProperty ("gateSequenceEnabled", true);
    state["arp"].getDynamicObject()->setProperty ("gateMask", 0);
    REQUIRE (processor.setTimelineState (state).wasOk());
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 64, static_cast<juce::uint8> (80)), 0);
    processor.processBlock (audio, midi);
    CHECK (static_cast<juce::int64> (processor.getTransportState()["arpPulseSerial"]) == 1);
}
