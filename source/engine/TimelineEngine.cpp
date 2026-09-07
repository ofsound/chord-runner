#include "TimelineEngine.h"
#include <cmath>

namespace chordrunner
{
void TimelineEngine::stop (juce::MidiBuffer& midi, int sample)
{
    for (int i = 0; i < heldCount; ++i)
        midi.addEvent (juce::MidiMessage::noteOff (heldChannel, held[static_cast<size_t> (i)]), sample);
    heldCount = 0;
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

    const auto beatsPerSample = bpm / (60.0 * sampleRate);
    const auto length = timeline.length();
    // Evaluate from the block origin to avoid accumulating per-sample rounding.
    // The fixed-capacity search also handles host seeks and loop discontinuities.
    for (int sample = 0; sample < samples; ++sample)
    {
        const auto beat = startBeat + sample * beatsPerSample;
        if (beat < -1.0e-9)
            continue;
        const auto cycle = static_cast<juce::int64> (std::floor ((beat + 1.0e-9) / length));
        const auto phase = beat - static_cast<double> (cycle) * length;
        int index = 0;
        double end = timeline.chords[0].beats;
        while (index + 1 < timeline.count && phase + 1.0e-9 >= end)
            end += timeline.chords[static_cast<size_t> (++index)].beats;
        if (index == activeIndex && cycle == lastCycle)
            continue;

        stop (midi, sample);
        const auto& chord = timeline.chords[static_cast<size_t> (index)];
        heldChannel = timeline.channel;
        held = chord.notes;
        heldCount = chord.noteCount;
        activeIndex = index;
        lastCycle = cycle;
        for (int i = 0; i < heldCount; ++i)
            midi.addEvent (juce::MidiMessage::noteOn (heldChannel, held[static_cast<size_t> (i)],
                              static_cast<juce::uint8> (timeline.velocity)), sample);
    }
    wasPlaying = true;
    expectedBeat = startBeat + samples * beatsPerSample;
}
}
