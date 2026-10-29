#include <PluginProcessor.h>
#include <ui/NativeEditor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <map>

namespace
{
struct Hit
{
    int sample = 0, note = 0, velocity = 0;
    bool on = false;
    bool operator== (const Hit&) const = default;
};
std::vector<Hit> hits (const juce::MidiBuffer& midi, int offset = 0)
{
    std::vector<Hit> result;
    for (const auto event : midi)
    {
        const auto message = event.getMessage();
        if (message.isNoteOnOrOff())
            result.push_back ({ event.samplePosition + offset, message.getNoteNumber(), static_cast<int> (message.getVelocity()), message.isNoteOn() });
    }
    return result;
}
void preparePerformance (PluginProcessor& processor, int block = 512)
{
    processor.setRateAndBufferSizeDetails (48000, block);
    processor.prepareToPlay (48000, block);
}
void pattern (PluginProcessor& processor, int slot, const juce::String& text, bool arp = false)
{
    processor.selectPattern (slot);
    auto state = processor.getTimelineState();
    state.getDynamicObject()->setProperty ("progression", text);
    state["arp"].getDynamicObject()->setProperty ("enabled", arp);
    REQUIRE (processor.editTimeline (state).wasOk());
}
std::vector<Hit> switchedPerformance (int block, int mode)
{
    PluginProcessor processor;
    pattern (processor, 0, "C");
    pattern (processor, 1, "Dm", true);
    processor.selectPattern (0);
    processor.setLaunchMode (mode);
    preparePerformance (processor, 4096);
    processor.setPlaying (true);
    juce::MidiBuffer midi;
    midi.ensureSize (65536);
    std::vector<Hit> result;
    constexpr int total = 120000, trigger = 17301;
    for (int start = 0; start < total; start += block)
    {
        const int count = juce::jmin (block, total - start);
        juce::AudioBuffer<float> audio (2, count);
        midi.clear();
        if (trigger >= start && trigger < start + count)
            midi.addEvent (juce::MidiMessage::noteOn (1, 1, static_cast<juce::uint8> (100)), trigger - start);
        processor.processBlock (audio, midi);
        auto events = hits (midi, start);
        result.insert (result.end(), events.begin(), events.end());
    }
    CHECK (processor.selectedPattern() == 1);
    CHECK (static_cast<int> (processor.getTransportState()["activePattern"]) == 1);
    return result;
}
}

TEST_CASE ("Sixteen complete patterns and project details restore atomically and stopped", "[performance][state]")
{
    PluginProcessor processor;
    for (int i = 0; i < 16; ++i)
    {
        pattern (processor, i, i % 2 == 0 ? "Cmaj9@2" : "Dm7 G7", i % 2 != 0);
        processor.renamePattern (i, "Scene " + juce::String (i + 1));
        auto state = processor.getTimelineState();
        state["arp"].getDynamicObject()->setProperty ("timingHumanize", i * 5);
        state["arp"].getDynamicObject()->setProperty ("velocityHumanize", i * 6);
        state["arp"].getDynamicObject()->setProperty ("swingSubdivision", i % 3);
        REQUIRE (processor.setTimelineState (state).wasOk());
    }
    processor.setProjectMetadata ("Night drive", "ofsound", "Sixteen performance scenes");
    processor.setLaunchMode (4);
    processor.setPlaying (true);
    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    PluginProcessor restored;
    restored.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    CHECK (juce::JSON::toString (processor.getProjectState()) == juce::JSON::toString (restored.getProjectState()));
    CHECK_FALSE (static_cast<bool> (restored.getTransportState()["armed"]));
    auto corrupt = restored.getProjectState();
    corrupt["patterns"][15]["arp"].getDynamicObject()->setProperty ("timingHumanize", 101);
    CHECK (restored.setProjectState (corrupt).failed());
    CHECK (static_cast<int> (restored.getProjectState()["patterns"][15]["arp"]["timingHumanize"]) == 75);
    CHECK (restored.getProjectState()["projectName"].toString() == "Night drive");
}

TEST_CASE ("Pattern launches are sample accurate and independent of callback sizes", "[performance][timing]")
{
    for (int mode = 0; mode < 5; ++mode)
    {
        const auto expected = switchedPerformance (512, mode);
        REQUIRE (expected.size() > 4);
        const int boundary = mode == 0 ? 17301 : mode == 1 ? 18000 : mode == 2 ? 24000 : 96000;
        CHECK (expected[3].sample == boundary);
        CHECK_FALSE (expected[3].on);
        CHECK (switchedPerformance (17, mode) == expected);
        CHECK (switchedPerformance (4096, mode) == expected);
    }
}

TEST_CASE ("Selection note offs never escape and KSH mute releases all notes", "[performance][midi]")
{
    PluginProcessor processor;
    processor.setLaunchMode (0);
    preparePerformance (processor);
    processor.setPlaying (true);
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    processor.processBlock (audio, midi);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (4, 16, static_cast<juce::uint8> (100)), 123);
    midi.addEvent (juce::MidiMessage::noteOff (4, 16), 234);
    processor.processBlock (audio, midi);
    REQUIRE (hits (midi).size() == 3);
    for (const auto hit : hits (midi))
    {
        CHECK (hit.sample == 123);
        CHECK_FALSE (hit.on);
    }
    CHECK (static_cast<int> (processor.getTransportState()["activePattern"]) == -1);
    midi.clear();
    processor.processBlock (audio, midi);
    CHECK (midi.isEmpty());
    processor.selectPattern (1);
    processor.processBlock (audio, midi);
    REQUIRE (hits (midi).size() == 3);
    CHECK (hits (midi)[0].on);
}

TEST_CASE ("Queued launches commit when transport stops and the latest request wins", "[performance][timing]")
{
    PluginProcessor processor;
    preparePerformance (processor);
    processor.setPlaying (true);
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    processor.processBlock (audio, midi);
    processor.selectPattern (3);
    midi.clear(); processor.processBlock (audio, midi);
    CHECK (static_cast<int> (processor.getTransportState()["activePattern"]) == 0);
    CHECK (static_cast<int> (processor.getTransportState()["queuedPattern"]) == 3);
    processor.selectPattern (7);
    midi.clear(); processor.processBlock (audio, midi);
    CHECK (static_cast<int> (processor.getTransportState()["queuedPattern"]) == 7);
    processor.setPlaying (false);
    midi.clear(); processor.processBlock (audio, midi);
    CHECK (static_cast<int> (processor.getTransportState()["activePattern"]) == 7);
    CHECK (static_cast<int> (processor.getTransportState()["queuedPattern"]) == -2);
    for (const auto hit : hits (midi)) CHECK_FALSE (hit.on);
}

TEST_CASE ("Undo covers the full bank and rejected edits do not enter history", "[performance][undo]")
{
    PluginProcessor processor;
    chordrunner::ui::EditorController controller (processor);
    controller.draft = "Dm9 G13 Cmaj9";
    REQUIRE (controller.applyDraft());
    REQUIRE (controller.set ("swing", 80, true));
    REQUIRE (processor.copyPattern (0, 15).wasOk());
    processor.renamePattern (15, "Ending");
    processor.undo();
    CHECK (processor.getProjectState()["patterns"][15]["name"].toString() == "Pattern 1");
    processor.undo();
    CHECK (processor.getProjectState()["patterns"][15]["progression"].toString() == "C - G - Am - F");
    processor.redo();
    CHECK (static_cast<int> (processor.getProjectState()["patterns"][15]["arp"]["swing"]) == 80);
    processor.undo(); processor.undo(); processor.undo();
    CHECK (processor.getTimelineState()["progression"].toString() == "C - G - Am - F");
    CHECK_FALSE (processor.canUndo());
    controller.refresh (true);
    controller.draft = "invalid!";
    CHECK_FALSE (controller.applyDraft());
    CHECK_FALSE (processor.canUndo());
    REQUIRE (controller.set ("tempo", 141));
    REQUIRE (controller.set ("tempo", 142));
    REQUIRE (controller.set ("tempo", 143));
    processor.undo();
    CHECK (static_cast<double> (processor.getTimelineState()["tempo"]) == 120.0);
    CHECK_FALSE (processor.canUndo());
}

TEST_CASE ("Project Save remembers its path and invalid Open preserves it", "[performance][files]")
{
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("chord-runner-tests", "", false);
    REQUIRE (folder.createDirectory());
    const auto file = folder.getChildFile ("Set.chordrunner");
    PluginProcessor processor;
    pattern (processor, 15, "Fmaj9", true);
    REQUIRE (processor.saveProject (file).wasOk());
    CHECK_FALSE (processor.isDocumentModified());
    CHECK (processor.getProjectFile() == file);
    processor.renamePattern (15, "Finale");
    CHECK (processor.isDocumentModified());
    REQUIRE (processor.saveProject (file).wasOk());
    PluginProcessor restored;
    REQUIRE (restored.loadProject (file).wasOk());
    CHECK (restored.getProjectFile() == file);
    CHECK (restored.getTimelineState()["patternName"].toString() == "Finale");
    const auto invalid = folder.getChildFile ("Invalid.chordrunner");
    REQUIRE (invalid.replaceWithText ("{}"));
    CHECK (restored.loadProject (invalid).failed());
    CHECK (restored.getProjectFile() == file);
    CHECK (restored.getTimelineState()["patternName"].toString() == "Finale");
    CHECK (folder.deleteRecursively());
}

TEST_CASE ("Render performance editor at desktop and iPad sizes", "[.visual]")
{
    PluginProcessor processor;
    pattern (processor, 0, "Dm9@1/2 G13b9@1/2 Cmaj9@2", true);
    processor.renamePattern (0, "Opening");
    processor.setProjectMetadata ("Night drive", "ofsound", "");
    auto demo = processor.getTimelineState();
    demo.getDynamicObject()->setProperty ("voiceLeading", 2);
    auto* demoArp = demo["arp"].getDynamicObject();
    demoArp->setProperty ("gateSequenceEnabled", true);
    demoArp->setProperty ("gateLength", 10);
    demoArp->setProperty ("gateMask", 975);
    demoArp->setProperty ("tieSequenceEnabled", true);
    demoArp->setProperty ("tieLength", 8);
    demoArp->setProperty ("tieMask", 42);
    REQUIRE (processor.setTimelineState (demo).wasOk());
    REQUIRE (processor.editChordTone (0, 0, 63, static_cast<int> (processor.getTimelineState()["revision"])).wasOk());
    chordrunner::ui::NativeEditor editor (processor);
    const auto folder = juce::File ("/private/tmp/chord-runner-preview");
    REQUIRE (folder.createDirectory());
    for (const auto size : { juce::Point<int> (1120, 800), juce::Point<int> (1024, 768), juce::Point<int> (768, 1024), juce::Point<int> (1024, 724), juce::Point<int> (390, 844), juce::Point<int> (320, 568), juce::Point<int> (844, 390), juce::Point<int> (568, 320) })
    {
        editor.setSize (size.x, size.y);
        const bool phone = size.x < 700 || size.y <= 500;
        for (int page = 0; page < (phone ? 5 : 1); ++page)
        {
            if (phone)
                for (int i = 0; i < editor.getNumChildComponents(); ++i)
                    if (auto* button = dynamic_cast<juce::TextButton*> (editor.getChildComponent (i));
                        button != nullptr && button->getComponentID() == "phone.page." + juce::String (page))
                        button->onClick();
            editor.controller.poll();
            const auto image = editor.createComponentSnapshot (editor.getLocalBounds());
            juce::FileOutputStream stream (folder.getChildFile (juce::String (size.x) + "x" + juce::String (size.y) + "-" + juce::String (page) + ".png"));
            REQUIRE (stream.openedOk());
            REQUIRE (stream.setPosition (0));
            REQUIRE (stream.truncate().wasOk());
            REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));
        }
    }
}

namespace
{
std::vector<Hit> groovePerformance (int block, chordrunner::ArpeggiatorSettings settings, int total = 24000)
{
    chordrunner::Arpeggiator arp;
    juce::MidiBuffer input, output;
    input.ensureSize (65536);
    output.ensureSize (1048576);
    std::vector<Hit> result;
    for (int start = 0; start < total; start += block)
    {
        const int count = juce::jmin (block, total - start);
        input.clear(); output.clear();
        if (start == 0) input.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (100)), 0);
        arp.render (settings, input, output, start / 24000.0, 120, 48000, count, true, false);
        auto batch = hits (output, start);
        result.insert (result.end(), batch.begin(), batch.end());
    }
    return result;
}
}

TEST_CASE ("All KSH swing subdivisions follow the exact delayed-group formula", "[performance][groove]")
{
    chordrunner::ArpeggiatorSettings settings;
    settings.sync = false;
    settings.freeMs = 10;
    settings.gate = 20;
    settings.swing = 100;
    for (int grid = 0; grid < 3; ++grid)
    {
        settings.swingSubdivision = grid;
        const auto actual = groovePerformance (512, settings);
        std::vector<int> onsets, expected;
        for (const auto hit : actual) if (hit.on) onsets.push_back (hit.sample);
        const int subdivision = 1 << grid;
        for (int step = 0; step < 50; ++step)
        {
            const int sample = step * 480 + (step / subdivision % 2 != 0 ? subdivision * 240 : 0);
            if (sample < 24000) expected.push_back (sample);
        }
        std::sort (expected.begin(), expected.end());
        expected.erase (std::unique (expected.begin(), expected.end()), expected.end());
        CHECK (onsets == expected);
        CHECK (groovePerformance (17, settings) == actual);
        CHECK (groovePerformance (4096, settings) == actual);
    }
}

TEST_CASE ("KSH humanize preserves early and late timing with proportional velocity variation", "[performance][groove]")
{
    chordrunner::ArpeggiatorSettings settings;
    settings.sync = false;
    settings.freeMs = 10;
    settings.gate = 20;
    settings.velocityHumanize = 50;
    settings.timingHumanize = 100;
    const auto actual = groovePerformance (512, settings);
    CHECK (groovePerformance (1, settings) == actual);
    CHECK (groovePerformance (17, settings) == actual);
    CHECK (groovePerformance (4096, settings) == actual);
    bool early = false, late = false, softer = false, louder = false;
    int step = 0;
    for (const auto hit : actual)
    {
        if (! hit.on) continue;
        const int delta = hit.sample - step++ * 480;
        CHECK (delta >= -96);
        CHECK (delta <= 97);
        early = early || delta < 0;
        late = late || delta > 0;
        CHECK (hit.velocity >= 50);
        CHECK (hit.velocity <= 127);
        softer = softer || hit.velocity < 100;
        louder = louder || hit.velocity > 100;
    }
    CHECK (early); CHECK (late); CHECK (softer); CHECK (louder);
}

TEST_CASE ("Standalone restores its Save destination and unsaved document status", "[performance][state]")
{
    PluginProcessor processor;
    const auto path = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Session.chordrunner");
    processor.setProjectFile (path);
    pattern (processor, 0, "Am7 Dm9");
    REQUIRE (processor.isDocumentModified());
    juce::PropertySet preferences;
    processor.saveStandaloneSettings (preferences);
    juce::MemoryBlock data;
    processor.getStateInformation (data);
    PluginProcessor restored;
    restored.setStateInformation (data.getData(), static_cast<int> (data.getSize()));
    restored.restoreStandaloneSettings (preferences);
    CHECK (restored.getProjectFile() == path);
    CHECK (restored.isDocumentModified());
    CHECK (restored.getTimelineState()["progression"].toString() == "Am7 Dm9");
}

TEST_CASE ("Output preview renders actual arp pitches and gates over source chords", "[preview]")
{
    chordrunner::Timeline timeline;
    REQUIRE (chordrunner::parseProgression ("C", timeline).wasOk());
    timeline.arp.enabled = true;
    timeline.arp.gate = 50;
    const auto phrase = chordrunner::renderOutputPreview (timeline);
    REQUIRE (phrase.size() == 16);
    CHECK (phrase[0].note == 60);
    CHECK (phrase[1].note == 64);
    CHECK (phrase[2].note == 67);
    CHECK (phrase[1].start == Catch::Approx (0.25));
    CHECK (phrase[0].end == Catch::Approx (0.125));
    timeline.arp.enabled = false;
    CHECK (chordrunner::renderOutputPreview (timeline).empty());
}

TEST_CASE ("Gate rests and tie chains advance at arp rate without retriggering held notes", "[rhythm]")
{
    chordrunner::ArpeggiatorSettings settings;
    settings.sync = false; settings.freeMs = 10; settings.gate = 50;
    settings.gateSequenceEnabled = true; settings.gateLength = 4; settings.gateMask = 5;
    auto events = groovePerformance (512, settings, 2400);
    std::vector<int> onsets;
    for (const auto hit : events) if (hit.on) onsets.push_back (hit.sample);
    CHECK (onsets == std::vector<int> { 0, 960, 1920 });
    settings.gateSequenceEnabled = false;
    settings.tieSequenceEnabled = true; settings.tieLength = 4; settings.tieMask = 6;
    events = groovePerformance (512, settings, 2400);
    REQUIRE (events.size() >= 4);
    CHECK (events[0].on); CHECK (events[0].sample == 0);
    CHECK_FALSE (events[1].on); CHECK (events[1].sample == 1200);
    CHECK (events[2].on); CHECK (events[2].sample == 1440);
    CHECK (groovePerformance (17, settings, 2400) == events);
    CHECK (groovePerformance (1, settings, 2400) == events);
    settings.tieMask = 65535;
    events = groovePerformance (512, settings, 2400);
    REQUIRE (events.size() == 1);
    CHECK (events[0].on);
}

TEST_CASE ("Rhythm lanes recall with undo and reject malformed cycles atomically", "[rhythm][state]")
{
    PluginProcessor processor;
    auto state = processor.getTimelineState();
    state["arp"].getDynamicObject()->setProperty ("gateLength", 10);
    state["arp"].getDynamicObject()->setProperty ("gateMask", 975);
    state["arp"].getDynamicObject()->setProperty ("tieLength", 8);
    state["arp"].getDynamicObject()->setProperty ("tieMask", 42);
    REQUIRE (processor.editTimeline (state).wasOk());
    juce::MemoryBlock data; processor.getStateInformation (data);
    PluginProcessor restored;
    restored.setStateInformation (data.getData(), static_cast<int> (data.getSize()));
    CHECK (static_cast<int> (restored.getTimelineState()["arp"]["gateLength"]) == 10);
    CHECK (static_cast<int> (restored.getTimelineState()["arp"]["tieMask"]) == 42);
    state["arp"].getDynamicObject()->setProperty ("gateLength", 0);
    CHECK (processor.editTimeline (state).failed());
    processor.undo();
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateLength"]) == 16);
    processor.redo();
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateLength"]) == 10);
}

TEST_CASE ("Fully tied finite phrases stop and panic releases an indefinite tie", "[rhythm][midi]")
{
    chordrunner::ArpeggiatorSettings settings;
    settings.sync = false; settings.freeMs = 10;
    settings.tieSequenceEnabled = true; settings.tieMask = 65535;
    settings.repeats = 1;
    const auto finite = groovePerformance (512, settings, 2400);
    REQUIRE (finite.size() == 2);
    CHECK (finite[0].on); CHECK_FALSE (finite[1].on);
    CHECK (finite[1].sample == 480);
    settings.steps = 8; settings.distance = 0; settings.repeats = 64;
    const auto longPhrase = groovePerformance (512, settings, 277000);
    REQUIRE (longPhrase.size() == 2);
    CHECK (longPhrase[1].sample == 276480);
    settings.steps = 0; settings.repeats = 0;
    chordrunner::Arpeggiator arp;
    juce::MidiBuffer input, output;
    input.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (100)), 0);
    arp.render (settings, input, output, 0, 120, 48000, 2400, true, false);
    output.clear(); arp.stop (output);
    REQUIRE (hits (output).size() == 1);
    CHECK_FALSE (hits (output)[0].on);
}
