#pragma once
#include "Timeline.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace chordrunner
{
class TimelineEngine
{
public:
    void render (const Timeline&, double startBeat, double bpm, double sampleRate,
                 int samples, bool playing, bool changed, juce::MidiBuffer&);
    void stop (juce::MidiBuffer&, int sample = 0);
    int activeChord() const { return activeIndex; }

private:
    std::array<int, maxNotes> held {};
    int heldCount = 0, heldChannel = 1, activeIndex = -1;
    juce::int64 lastCycle = -1;
    double expectedBeat = 0.0;
    bool wasPlaying = false;
};
}
