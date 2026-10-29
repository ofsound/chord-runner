#include "OutputPreview.h"

namespace chordrunner
{
std::vector<PreviewNote> renderOutputPreview (const Timeline& timeline, double maximumBeats)
{
    std::vector<PreviewNote> notes;
    if (! timeline.sequencerEnabled || ! timeline.arp.enabled)
        return notes;
    constexpr double sampleRate = 4000.0;
    const double beatsPerSample = timeline.tempo / (60.0 * sampleRate);
    const double endBeat = juce::jmin (timeline.length(), maximumBeats);
    const int total = static_cast<int> (std::ceil (endBeat / beatsPerSample));
    TimelineEngine sequence;
    Arpeggiator arp;
    juce::MidiBuffer input, output;
    input.ensureSize (65536);
    output.ensureSize (262144);
    std::array<int, 16 * 128> active;
    active.fill (-1);
    for (int start = 0; start < total && notes.size() < 8192; start += 256)
    {
        const int count = juce::jmin (256, total - start);
        input.clear(); output.clear();
        sequence.render (timeline, start * beatsPerSample, timeline.tempo, sampleRate, count, true, false, input);
        arp.render (timeline.arp, input, output, start * beatsPerSample, timeline.tempo, sampleRate, count, true, false);
        for (const auto event : output)
        {
            const auto message = event.getMessage();
            if (! message.isNoteOnOrOff()) continue;
            const double beat = (start + event.samplePosition) * beatsPerSample;
            auto& index = active[static_cast<size_t> ((message.getChannel() - 1) * 128 + message.getNoteNumber())];
            if (index >= 0) notes[static_cast<size_t> (index)].end = beat;
            index = -1;
            if (message.isNoteOn() && notes.size() < 8192)
            {
                index = static_cast<int> (notes.size());
                notes.push_back ({ message.getNoteNumber(), message.getVelocity(), beat, endBeat });
            }
        }
    }
    std::erase_if (notes, [] (const PreviewNote& note) { return note.end <= note.start; });
    return notes;
}
}
