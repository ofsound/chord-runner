#pragma once
#include <juce_core/juce_core.h>
#include "Arpeggiator.h"
#include <array>
#include <atomic>

namespace chordrunner
{
constexpr int maxChords = 64;
constexpr int maxNotes = 8;

struct Chord
{
    std::array<int, maxNotes> notes { 60, 64, 67 };
    std::array<char, 32> label { 'C', '\0' };
    int noteCount = 3;
    double beats = 4.0;
};

struct Timeline
{
    std::array<Chord, maxChords> chords {};
    int count = 4, channel = 1, velocity = 100, octave = 4;
    double tempo = 120.0;
    bool hostSync = false, sequencerEnabled = true;
    ArpeggiatorSettings arp;
    juce::uint64 noteRevision = 0;

    Timeline()
    {
        chords[1].notes = { 67, 71, 74 };
        chords[1].label = { 'G', '\0' };
        chords[2].notes = { 69, 72, 76 };
        chords[2].label = { 'A', 'm', '\0' };
        chords[3].notes = { 65, 69, 72 };
        chords[3].label = { 'F', '\0' };
    }
    double length() const
    {
        double result = 0.0;
        for (int i = 0; i < count; ++i)
            result += chords[static_cast<size_t> (i)].beats;
        return result;
    }
};

// Call only from non-audio threads. Invalid text leaves the supplied timeline intact.
juce::Result parseProgression (const juce::String&, Timeline&);

// Producers serialize outside the callback. Three fixed slots let the audio
// consumer take the latest value without allocation, locks, or dropped edits.
template <typename T>
class SnapshotExchange
{
public:
    void publish (const T& value)
    {
        slots[static_cast<size_t> (back)] = value;
        back = middle.exchange (back | dirty, std::memory_order_acq_rel) & indexMask;
    }
    bool consume()
    {
        if ((middle.load (std::memory_order_acquire) & dirty) == 0)
            return false;
        front = middle.exchange (front, std::memory_order_acq_rel) & indexMask;
        return true;
    }
    const T& current() const { return slots[static_cast<size_t> (front)]; }

private:
    static constexpr int dirty = 4, indexMask = 3;
    std::array<T, 3> slots {};
    std::atomic<int> middle { 1 };
    int front = 0, back = 2;
};
}
