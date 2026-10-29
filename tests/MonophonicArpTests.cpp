#include <PluginProcessor.h>
#include <ui/NativeEditor.h>
#include <catch2/catch_test_macros.hpp>
#include <map>

namespace
{
struct NoteEvent
{
    int sample = 0, note = 0, channel = 0;
    bool on = false;
    bool operator== (const NoteEvent&) const = default;
};
chordrunner::Timeline screenshotPattern()
{
    chordrunner::Timeline timeline;
    timeline.octave = 3; timeline.tempo = 117;
    REQUIRE (chordrunner::parseProgression ("C F G@2", timeline).wasOk());
    auto& arp = timeline.arp;
    arp.enabled = true; arp.style = chordrunner::ArpeggiatorSettings::playOrder;
    arp.rate = 0.5; arp.steps = 2; arp.offset = 1; arp.repeats = 1;
    arp.gate = 60; arp.swing = 4; arp.swingSubdivision = 0;
    arp.velocityHumanize = 34; arp.timingHumanize = 38;
    arp.retrigger = 2; arp.interval = 4;
    return timeline;
}
std::vector<NoteEvent> renderPhrase (const chordrunner::Timeline& timeline, int block)
{
    constexpr double rate = 48000;
    const double bps = timeline.tempo / (60.0 * rate);
    const int total = static_cast<int> (std::ceil (timeline.length() / bps));
    chordrunner::TimelineEngine sequence;
    chordrunner::Arpeggiator arp;
    juce::MidiBuffer input, output;
    input.ensureSize (65536); output.ensureSize (262144);
    std::vector<NoteEvent> events;
    for (int start = 0; start < total; start += block)
    {
        const int count = juce::jmin (block, total - start);
        input.clear(); output.clear();
        sequence.render (timeline, start * bps, timeline.tempo, rate, count, true, false, input);
        arp.render (timeline.arp, input, output, start * bps, timeline.tempo, rate, count, true, false);
        for (const auto event : output)
        {
            const auto message = event.getMessage();
            if (message.isNoteOnOrOff())
                events.push_back ({ start + event.samplePosition, message.getNoteNumber(), message.getChannel(), message.isNoteOn() });
        }
    }
    return events;
}
void requireMono (const std::vector<NoteEvent>& events)
{
    std::map<std::pair<int, int>, int> sounding;
    int lastOn = -1;
    for (const auto& event : events)
    {
        auto key = std::pair { event.channel, event.note };
        if (event.on)
        {
            REQUIRE (sounding.empty());
            REQUIRE (event.sample != lastOn);
            sounding[key] = 1;
            lastOn = event.sample;
        }
        else
        {
            REQUIRE (sounding.contains (key));
            sounding.erase (key);
        }
    }
}
void configureScreenshot (PluginProcessor& processor, const chordrunner::Timeline& timeline)
{
    auto state = processor.getTimelineState();
    auto* root = state.getDynamicObject();
    root->setProperty ("progression", "C F G@2"); root->setProperty ("octave", 3); root->setProperty ("tempo", 117);
    auto* arp = state["arp"].getDynamicObject();
    arp->setProperty ("enabled", true); arp->setProperty ("style", timeline.arp.style);
    arp->setProperty ("rate", 0.5); arp->setProperty ("steps", 2); arp->setProperty ("offset", 1);
    arp->setProperty ("repeats", 1); arp->setProperty ("gate", 60);
    arp->setProperty ("swing", 4); arp->setProperty ("swingSubdivision", 0);
    arp->setProperty ("velocityHumanize", 34); arp->setProperty ("timingHumanize", 38);
    arp->setProperty ("retrigger", 2); arp->setProperty ("interval", 4);
    REQUIRE (processor.setTimelineState (state).wasOk());
}

}
TEST_CASE ("Humanized beat retriggers produce one attack per step without a duplicate bar downbeat", "[arp][regression][mono]")
{
    const auto timeline = screenshotPattern();
    const auto events = renderPhrase (timeline, 512);
    requireMono (events);
    std::array<int, 4> attacks {};
    for (const auto& event : events)
        if (event.on)
        {
            const int bar = static_cast<int> (std::floor (event.sample * timeline.tempo / (60.0 * 48000.0 * 4.0)));
            REQUIRE (bar >= 0); REQUIRE (bar < 4);
            ++attacks[static_cast<size_t> (bar)];
        }
    CHECK (attacks == std::array<int, 4> { 8, 8, 8, 8 });
    CHECK (renderPhrase (timeline, 1) == events);
    CHECK (renderPhrase (timeline, 17) == events);
    CHECK (renderPhrase (timeline, 4096) == events);
}
TEST_CASE ("Single-note styles stay monophonic with long gates extreme swing and humanize", "[arp][mono]")
{
    auto timeline = screenshotPattern();
    REQUIRE (chordrunner::parseProgression ("C", timeline).wasOk());
    timeline.arp.repeats = 0;
    timeline.arp.gate = 200;
    timeline.arp.swing = 100;
    timeline.arp.timingHumanize = 100;
    for (int style = 0; style <= 10; ++style)
    {
        if (style == chordrunner::ArpeggiatorSettings::chordTrigger) continue;
        timeline.arp.style = style;
        for (int grid = 0; grid < 3; ++grid)
        {
            timeline.arp.swingSubdivision = grid;
            requireMono (renderPhrase (timeline, 512));
        }
    }
}
TEST_CASE ("Amber output preview contains one non-overlapping melodic line", "[preview][mono]")
{
    auto timeline = screenshotPattern();
    timeline.arp.gate = 200;
    const auto notes = chordrunner::renderOutputPreview (timeline);
    REQUIRE (! notes.empty());
    for (size_t i = 0; i < notes.size(); ++i)
    {
        CHECK (notes[i].end > notes[i].start);
        if (i > 0) CHECK (notes[i].start + 1.0e-9 >= notes[i - 1].end);
    }
}

TEST_CASE ("Standalone internal transport emits the same monophonic humanized phrase", "[processor][standalone][mono]")
{
    juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
    PluginProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);
    REQUIRE (processor.isStandalone());
    const auto timeline = screenshotPattern();
    configureScreenshot (processor, timeline);
    processor.setRateAndBufferSizeDetails (48000, 512); processor.prepareToPlay (48000, 512);
    processor.setPlaying (true);
    juce::AudioBuffer<float> audio (2, 512); juce::MidiBuffer midi;
    std::vector<NoteEvent> events;
    const int total = static_cast<int> (std::ceil (timeline.length() * 60.0 * 48000.0 / timeline.tempo));
    for (int start = 0; start < total; start += 512)
    {
        audio.setSize (2, juce::jmin (512, total - start), false, false, true);
        midi.clear(); processor.processBlock (audio, midi);
        for (const auto event : midi)
        {
            const auto message = event.getMessage();
            if (message.isNoteOnOrOff()) events.push_back ({ start + event.samplePosition, message.getNoteNumber(), message.getChannel(), message.isNoteOn() });
        }
    }
    requireMono (events);
    CHECK (events == renderPhrase (timeline, 512));
}

TEST_CASE ("Render the reported arp settings after the monophonic scheduling fix", "[.visual]")
{
    juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
    PluginProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);
    configureScreenshot (processor, screenshotPattern());
    chordrunner::ui::NativeEditor editor (processor);
    editor.setSize (1200, 800);
    const auto image = editor.createComponentSnapshot (editor.getLocalBounds());
    juce::FileOutputStream stream (juce::File ("/private/tmp/chord-runner-monophonic.png"));
    REQUIRE (stream.openedOk()); REQUIRE (stream.setPosition (0)); REQUIRE (stream.truncate().wasOk());
    REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));
}
