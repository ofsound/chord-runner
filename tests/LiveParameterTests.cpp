#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE ("Live parameter edits preserve held notes in every MIDI routing mode", "[processor][live-edits]")
{
    struct Edit { const char* key; juce::var value; bool arp; };
    const std::array edits {
        Edit { "tempo", 121, false }, Edit { "velocity", 90, false },
        Edit { "gate", 70, true }, Edit { "swing", 20, true },
        Edit { "swingSubdivision", 2, true }, Edit { "velocityHumanize", 10, true },
        Edit { "timingHumanize", 10, true }, Edit { "strength", 50, true },
        Edit { "target", 40, true }, Edit { "decay", 2000, true },
        Edit { "freeMs", 200, true }, Edit { "interval", 8, true },
        Edit { "style", 1, true }, Edit { "offset", 1, true },
        Edit { "steps", 1, true }, Edit { "repeats", 2, true },
        Edit { "gateMask", 7, true }, Edit { "tieMask", 1, true },
        Edit { "rate", 0.5, true }, Edit { "sync", false, true },
        Edit { "hold", true, true }, Edit { "retrigger", 2, true },
        Edit { "octaveMode", 1, true }, Edit { "distance", 7, true },
        Edit { "gateSequenceEnabled", true, true }, Edit { "tieSequenceEnabled", true, true },
        Edit { "gateLength", 8, true }, Edit { "tieLength", 8, true },
        Edit { "velocityEnabled", true, true }, Edit { "velocityRetrigger", false, true }
    };
    for (int mode = 0; mode < 3; ++mode)
    {
        for (const auto& edit : edits)
        {
            juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
            PluginProcessor processor;
            juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);
            auto state = processor.getTimelineState();
            state.getDynamicObject()->setProperty ("sequencerEnabled", mode != 2);
            state["arp"].getDynamicObject()->setProperty ("enabled", mode != 0);
            REQUIRE (processor.setTimelineState (state).wasOk());
            processor.setRateAndBufferSizeDetails (48000, 512);
            processor.prepareToPlay (48000, 512);
            processor.setPlaying (true);
            juce::AudioBuffer<float> audio (2, 512);
            juce::MidiBuffer midi;
            if (mode == 2)
                for (int pitch : { 60, 64, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, pitch, static_cast<juce::uint8> (100)), 0);
            processor.processBlock (audio, midi);
            midi.clear();
            state = processor.getTimelineState();
            auto target = edit.arp ? state["arp"] : state;
            target.getDynamicObject()->setProperty (edit.key, edit.value);
            REQUIRE (processor.setTimelineState (state).wasOk());
            processor.processBlock (audio, midi);
            INFO ("mode " << mode << ", parameter " << edit.key);
            int ons = 0, offs = 0;
            for (const auto event : midi)
            {
                if (event.getMessage().isNoteOn()) ++ons;
                if (event.getMessage().isNoteOff()) ++offs;
            }
            CHECK (ons == 0);
            CHECK (offs == 0);
        }
    }
}

namespace
{
void prepareLiveTest (PluginProcessor& processor, bool sequence, bool arp)
{
    auto state = processor.getTimelineState();
    state.getDynamicObject()->setProperty ("sequencerEnabled", sequence);
    state["arp"].getDynamicObject()->setProperty ("enabled", arp);
    REQUIRE (processor.setTimelineState (state).wasOk());
    processor.setRateAndBufferSizeDetails (48000, 512);
    processor.prepareToPlay (48000, 512);
    processor.setPlaying (true);
}
}

TEST_CASE ("Phrase edits keep step position and velocity edits reach the next arp attack", "[processor][live-edits]")
{
    for (const bool sequence : { false, true })
    {
        PluginProcessor processor;
        prepareLiveTest (processor, sequence, true);
        juce::AudioBuffer<float> audio (2, 512);
        juce::MidiBuffer midi;
        if (! sequence)
            for (int pitch : { 60, 64, 67 })
                midi.addEvent (juce::MidiMessage::noteOn (1, pitch, static_cast<juce::uint8> (100)), 0);
        processor.processBlock (audio, midi);
        auto state = processor.getTimelineState();
        state["arp"].getDynamicObject()->setProperty ("offset", 2);
        state.getDynamicObject()->setProperty ("velocity", 41);
        REQUIRE (processor.setTimelineState (state).wasOk());
        int attacks = 0;
        for (int start = 512; start < 6656; start += 512)
        {
            midi.clear();
            processor.processBlock (audio, midi);
            for (const auto event : midi)
                if (event.getMessage().isNoteOn())
                {
                    ++attacks;
                    CHECK (start + event.samplePosition == 6000);
                    CHECK (event.getMessage().getNoteNumber() == 60); // Step 1 + offset 2, not a new step 0.
                    CHECK (event.getMessage().getVelocity() == (sequence ? 41 : 100));
                }
        }
        CHECK (attacks == 1);
    }
}

TEST_CASE ("Parameter undo and redo preserve notes while note replacements release them", "[processor][live-edits]")
{
    PluginProcessor processor;
    prepareLiveTest (processor, true, false);
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    processor.processBlock (audio, midi);
    auto state = processor.getTimelineState();
    state["arp"].getDynamicObject()->setProperty ("swing", 25);
    REQUIRE (processor.editTimeline (state, "arp.swing").wasOk());
    midi.clear();
    processor.processBlock (audio, midi);
    CHECK (midi.isEmpty());
    processor.undo();
    processor.processBlock (audio, midi);
    CHECK (midi.isEmpty());
    processor.redo();
    processor.processBlock (audio, midi);
    CHECK (midi.isEmpty());
    state = processor.getTimelineState();
    state.getDynamicObject()->setProperty ("progression", "Dm");
    state.getDynamicObject()->removeProperty ("toneOffsets");
    state.getDynamicObject()->removeProperty ("noteTiming");
    REQUIRE (processor.setTimelineState (state).wasOk());
    processor.processBlock (audio, midi);
    int offs = 0, ons = 0;
    for (const auto event : midi)
    {
        if (event.getMessage().isNoteOff()) { ++offs; CHECK (ons == 0); }
        if (event.getMessage().isNoteOn())
        {
            ++ons;
            const int pitch = event.getMessage().getNoteNumber();
            CHECK ((pitch == 62 || pitch == 65 || pitch == 69));
        }
    }
    CHECK (offs == 3);
    CHECK (ons == 3);
}

TEST_CASE ("Changing restart controls waits for the next beat boundary", "[arp][live-edits]")
{
    chordrunner::Arpeggiator arp;
    chordrunner::ArpeggiatorSettings settings;
    settings.rate = 0.25;
    juce::MidiBuffer input, output;
    input.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (100)), 0);
    arp.render (settings, input, output, 0, 120, 48000, 512, true, false);
    input.clear(); output.clear();
    settings.retrigger = 2;
    settings.interval = 0.5;
    arp.render (settings, input, output, 512.0 / 24000.0, 120, 48000, 512, true, false);
    CHECK (output.isEmpty());
    settings.interval = 0.25;
    arp.render (settings, input, output, 1024.0 / 24000.0, 120, 48000, 512, true, false);
    CHECK (output.isEmpty());
    CHECK (arp.triggerStep() == 0);
    for (int start = 1536; start < 6144; start += 512)
    {
        output.clear();
        arp.render (settings, input, output, start / 24000.0, 120, 48000, 512, true, false);
        for (const auto event : output)
            if (event.getMessage().isNoteOn()) CHECK (start + event.samplePosition == 6000);
    }
    CHECK (arp.triggerSerial() == 2);
}

TEST_CASE ("Turning latch off releases only released source keys", "[arp][live-edits]")
{
    chordrunner::Arpeggiator arp;
    chordrunner::ArpeggiatorSettings settings;
    settings.hold = true;
    juce::MidiBuffer input, output;
    for (int pitch : { 60, 64 })
        input.addEvent (juce::MidiMessage::noteOn (1, pitch, static_cast<juce::uint8> (100)), 0);
    arp.render (settings, input, output, 0, 120, 48000, 512, true, false);
    input.clear(); output.clear();
    input.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    arp.render (settings, input, output, 512.0 / 24000.0, 120, 48000, 512, true, false);
    CHECK (output.isEmpty());
    input.clear();
    settings.hold = false;
    arp.render (settings, input, output, 1024.0 / 24000.0, 120, 48000, 512, true, false);
    REQUIRE (output.getNumEvents() == 1);
    CHECK ((*output.begin()).getMessage().isNoteOff());
    CHECK ((*output.begin()).getMessage().getNoteNumber() == 60);
    CHECK (arp.inputNoteCount() == 1);
    CHECK (arp.triggerSerial() == 1);
}

TEST_CASE ("Editing an endless tie releases its old tail at the next step", "[arp][live-edits]")
{
    chordrunner::Arpeggiator arp;
    chordrunner::ArpeggiatorSettings settings;
    settings.tieSequenceEnabled = true;
    settings.tieLength = 1;
    settings.tieMask = 1;
    juce::MidiBuffer input, output;
    input.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (100)), 0);
    arp.render (settings, input, output, 0, 120, 48000, 512, true, false);
    input.clear(); output.clear();
    settings.tieSequenceEnabled = false;
    arp.render (settings, input, output, 512.0 / 24000.0, 120, 48000, 512, true, false);
    CHECK (output.isEmpty());
    int offs = 0, ons = 0;
    for (int start = 1024; start < 6144; start += 512)
    {
        output.clear();
        arp.render (settings, input, output, start / 24000.0, 120, 48000, 512, true, false);
        for (const auto event : output)
        {
            if (event.getMessage().isNoteOff()) { ++offs; CHECK (start + event.samplePosition == 6000); }
            if (event.getMessage().isNoteOn()) { ++ons; CHECK (start + event.samplePosition == 6000); }
        }
    }
    CHECK (offs == 1);
    CHECK (ons == 1);
}
