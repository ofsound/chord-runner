#include "TimelineEngine.h"
#include <algorithm>
#include <cmath>

namespace chordrunner
{
void TimelineEngine::stop (juce::MidiBuffer& midi, int sample)
{
    for (int pitch = 0; pitch < 128; ++pitch)
        if (pitchCounts[static_cast<size_t> (pitch)] > 0)
            midi.addEvent (juce::MidiMessage::noteOff (heldChannel, pitch), sample);
    held.fill (false);
    pitchCounts.fill (0);
    activeIndex = -1;
    wasPlaying = false;
}

void TimelineEngine::render (const Timeline& timeline, double startBeat, double bpm, double sampleRate,
                             int samples, bool playing, bool changed, juce::MidiBuffer& midi)
{
    if (samples <= 0)
        return;
    if (! playing || ! std::isfinite (startBeat) || std::abs (startBeat) > 1.0e12 || ! std::isfinite (bpm)
        || bpm <= 0.0 || ! std::isfinite (sampleRate) || sampleRate <= 0.0)
    {
        stop (midi);
        return;
    }
    if (changed || ! wasPlaying || std::abs (startBeat - expectedBeat) > 1.0e-7)
        stop (midi);
    heldChannel = timeline.channel;
    const double bps = bpm / (60.0 * sampleRate), length = timeline.playbackLength();
    struct Event
    {
        int sample, id, pitch;
        bool on;
    };
    std::array<Event, maxChords * maxNotes * 2> events;
    auto send = [&] (int id, int pitch, bool on, int sample) {
        auto& voice = held[static_cast<size_t> (id)];
        if (voice == on)
            return;
        voice = on;
        auto& count = pitchCounts[static_cast<size_t> (pitch)];
        if (on)
        {
            if (count++ == 0)
                midi.addEvent (juce::MidiMessage::noteOn (heldChannel, pitch, static_cast<juce::uint8> (timeline.velocity)), sample);
        }
        else if (--count == 0)
            midi.addEvent (juce::MidiMessage::noteOff (heldChannel, pitch), sample);
    };
    int cursor = 0;
    while (cursor < samples)
    {
        const double beat = startBeat + cursor * bps;
        if (beat < -1.0e-9)
        {
            cursor = juce::jmin (samples, juce::jmax (cursor + 1, static_cast<int> (std::ceil (-startBeat / bps))));
            continue;
        }
        const auto cycle = static_cast<juce::int64> (std::floor ((beat + 1.0e-9) / length));
        const double cycleStart = static_cast<double> (cycle) * length - timeline.playbackStart();
        const double phase = juce::jmax (0.0, beat - cycleStart);
        if (cycle != lastCycle)
        {
            stop (midi, cursor);
            lastCycle = cycle;
        }
        const double remainingSamples = std::ceil ((static_cast<double> (cycle + 1) * length - startBeat) / bps - 1.0e-7);
        const int end = remainingSamples >= samples ? samples : juce::jmax (cursor + 1, static_cast<int> (remainingSamples));
        int eventCount = 0;
        double origin = 0.0;
        for (int c = 0; c < timeline.count; ++c)
        {
            const auto& chord = timeline.chords[static_cast<size_t> (c)];
            for (int n = 0; n < chord.noteCount; ++n)
            {
                const auto index = static_cast<size_t> (n);
                const double on = origin + chord.startOffsets[index];
                const double off = on + (chord.durations[index] > 0 ? chord.durations[index] : chord.beats);
                const int id = c * maxNotes + n, pitch = chord.notes[index];
                // Chase sustained notes after transport starts, seeks or edits.
                if (!wasPlaying && on < phase - 1.0e-9 && off > phase + 1.0e-9)
                    send (id, pitch, true, cursor);
                for (const bool isOn : { false, true })
                {
                    const double position = std::ceil ((cycleStart + (isOn ? on : off) - startBeat) / bps - 1.0e-7);
                    if (position >= cursor && position < end)
                        events[static_cast<size_t> (eventCount++)] = { static_cast<int> (position), id, pitch, isOn };
                }
            }
            origin += chord.beats;
        }
        std::sort (events.begin(), events.begin() + eventCount, [] (const Event& a, const Event& b) {
            if (a.sample != b.sample)
                return a.sample < b.sample;
            if (a.on != b.on)
                return !a.on;
            return a.id < b.id;
        });
        for (int i = 0; i < eventCount; ++i)
        {
            const auto& event = events[static_cast<size_t> (i)];
            send (event.id, event.pitch, event.on, event.sample);
        }
        activeIndex = -1;
        const double lastPhase = startBeat + (end - 1) * bps - cycleStart;
        origin = 0.0;
        for (int c = 0; c < timeline.count; ++c)
        {
            origin += timeline.chords[static_cast<size_t> (c)].beats;
            if (lastPhase < origin)
            {
                activeIndex = c;
                break;
            }
        }
        wasPlaying = true;
        cursor = end;
    }
    wasPlaying = true;
    expectedBeat = startBeat + samples * bps;
}
}
