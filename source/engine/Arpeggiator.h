#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

namespace chordrunner
{
struct ArpeggiatorSettings
{
    enum Style { up, down, upDown, downUp, converge, diverge, playOrder, chordTrigger, random, randomOther, randomOnce };
    bool enabled = false, sync = true, hold = false, velocityEnabled = false, velocityRetrigger = true;
    int style = up, steps = 0, distance = 12, offset = 0, retrigger = 1, repeats = 0, target = 64;
    double rate = 0.25, freeMs = 100.0, gate = 100.0, swing = 0.0, interval = 4.0, decay = 1000.0;
    bool operator== (const ArpeggiatorSettings& other) const
    {
        return enabled == other.enabled
            && sync == other.sync
            && hold == other.hold
            && velocityEnabled == other.velocityEnabled
            && velocityRetrigger == other.velocityRetrigger
            && style == other.style
            && steps == other.steps
            && distance == other.distance
            && offset == other.offset
            && retrigger == other.retrigger
            && repeats == other.repeats
            && target == other.target
            && std::abs (rate - other.rate) < 1.0e-12
            && std::abs (freeMs - other.freeMs) < 1.0e-12
            && std::abs (gate - other.gate) < 1.0e-12
            && std::abs (swing - other.swing) < 1.0e-12
            && std::abs (interval - other.interval) < 1.0e-12
            && std::abs (decay - other.decay) < 1.0e-12;
    }
};

class Arpeggiator
{
public:
    static constexpr int capacity = 128;
    Arpeggiator() { sampleMidi.ensureSize (73728); }
    void render (const ArpeggiatorSettings&, const juce::MidiBuffer& input, juce::MidiBuffer& output,
                 double beat, double bpm, double sampleRate, int samples, bool running, bool changed);
    void stop (juce::MidiBuffer&, int sample = 0, bool clearKeys = true);

private:
    struct Key { int note = 0, channel = 1, velocity = 100; bool down = false; };
    struct Voice { int note = 0, channel = 1, source = 0; double off = 0.0; };
    juce::MidiBuffer sampleMidi;
    std::array<Key, capacity> keys {};
    std::array<int, capacity> order {}, shuffle {};
    std::array<Voice, capacity * 2> voices {};
    std::array<bool, 16> sustain {};
    int keyCount = 0, voiceCount = 0, orderCount = 0;
    juce::uint64 step = 0;
    juce::uint32 seed = 0x6d2b79f5u;
    double clock = 0.0, next = 0.0, velocityStart = 0.0, expectedBeat = 0.0;
    juce::int64 retriggerBucket = -1;
    bool wasRunning = false;
    unsigned randomNumber();
    void rebuild();
    void restart (bool velocity);
    void emit (const Key&, int transpose, const ArpeggiatorSettings&, double duration, double sampleRate,
               juce::MidiBuffer&, int sample);
};
}
