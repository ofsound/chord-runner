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
    int octaveMode = 0; // 0: complete phrase per octave; 1: octaves of each note.
    int style = up, steps = 0, distance = 12, offset = 0, retrigger = 1, target = 64;
    double repeats = 0.0;
    bool gateSequenceEnabled = false, tieSequenceEnabled = false;
    int gateLength = 4, tieLength = 4, gateMask = 15, tieMask = 0;
    int swingSubdivision = 1, velocityHumanize = 0, timingHumanize = 0;
    double strength = 100.0;
    int startVelocity = -1; // Legacy/input velocity until the graph start is edited.
    double durationRatio = -1.0; // Legacy absolute duration until graph editing.
    double rate = 0.25, freeMs = 100.0, gate = 100.0, swing = 0.0, interval = 4.0, decay = 1000.0;
    double envelopeRunMs (int notes, double bpm) const
    {
        const int base = style == chordTrigger ? 1 : style == upDown || style == downUp ? juce::jmax (1, notes * 2 - 2) : juce::jmax (1, notes);
        const int cycle = base * (steps + 1);
        const double count = repeats > 0.0 ? static_cast<double> (repeatStepCount (cycle)) : static_cast<double> (cycle);
        return retrigger == 2 ? interval * 60000.0 / bpm : count * (sync ? rate * 60000.0 / bpm : freeMs);
    }
    int envelopeVelocity (int input, double elapsedMs, double runMs = 0.0) const
    {
        if (! velocityEnabled) return input;
        const int start = startVelocity >= 0 ? startVelocity : input;
        const double duration = durationRatio >= 0.0 ? durationRatio * runMs : decay;
        const double fraction = elapsedMs <= 0.0 ? 0.0 : duration <= 0.0 ? 1.0 : juce::jlimit (0.0, 1.0, elapsedMs / duration);
        return juce::roundToInt (start + (target - start) * fraction * strength / 100.0);
    }
    int humanizedVelocity (int velocity, double variation) const
    {
        if (velocityHumanize <= 0 || velocity <= 0) return velocity;
        return juce::jlimit (1, 127, juce::roundToInt (velocity + variation * velocity * velocityHumanize / 100.0));
    }
    juce::uint64 repeatStepCount (int cycleLength) const
    {
        return static_cast<juce::uint64> (juce::jmax (1.0, std::round (static_cast<double> (cycleLength) * repeats)));
    }
    bool operator== (const ArpeggiatorSettings& other) const
    {
        return octaveMode == other.octaveMode && gateSequenceEnabled == other.gateSequenceEnabled && tieSequenceEnabled == other.tieSequenceEnabled
            && gateLength == other.gateLength && tieLength == other.tieLength
            && gateMask == other.gateMask && tieMask == other.tieMask
            && swingSubdivision == other.swingSubdivision
            && velocityHumanize == other.velocityHumanize
            && timingHumanize == other.timingHumanize
            && enabled == other.enabled
            && sync == other.sync
            && hold == other.hold
            && std::abs (strength - other.strength) < 1.0e-12
            && velocityEnabled == other.velocityEnabled
            && velocityRetrigger == other.velocityRetrigger
            && startVelocity == other.startVelocity
            && std::abs (durationRatio - other.durationRatio) < 1.0e-12
            && style == other.style
            && steps == other.steps
            && distance == other.distance
            && offset == other.offset
            && retrigger == other.retrigger
            && std::abs (repeats - other.repeats) < 1.0e-12
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
    // Sequencer input can update its velocity without replacing held source notes.
    // A negative override preserves the velocities of physical MIDI input.
    void render (const ArpeggiatorSettings&, const juce::MidiBuffer& input, juce::MidiBuffer& output,
                 double beat, double bpm, double sampleRate, int samples, bool running, bool changed, int velocityOverride = -1);
    void stop (juce::MidiBuffer&, int sample = 0, bool clearKeys = true);
    int inputNoteCount() const { return keyCount; }
    juce::uint64 triggerSerial() const { return emittedSerial; }
    juce::uint64 triggerStep() const { return emittedStep; }
    juce::uint64 triggerRhythmStep() const { return emittedRhythmStep; }
    int triggerSourceVelocity() const { return emittedSourceVelocity; }
    double triggerEnvelopeMs() const { return emittedEnvelopeMs; }
    int triggerVelocity() const { return emittedVelocity; }
    double triggerAgeMs (double rate) const { return (clock - emittedClock) * 1000.0 / juce::jmax (1.0, rate); }

private:
    struct Key { int note = 0, channel = 1, velocity = 100; bool down = false; };
    struct Pending { Key key; int transpose = 0; double at = 0.0, duration = 0.0, nominal = 0.0; juce::uint64 ordinal = 0, rhythmOrdinal = 0; };
    struct Voice { int note = 0, channel = 1, source = 0; double off = 0.0; };
    juce::MidiBuffer sampleMidi;
    std::array<Key, capacity> keys {};
    std::array<int, capacity> order {}, shuffle {};
    std::array<Voice, capacity * 2> voices {};
    std::array<bool, 16> sustain {};
    std::array<Pending, capacity * 8> pending {};
    int pendingCount = 0;
    int keyCount = 0, voiceCount = 0, orderCount = 0;
    juce::uint64 step = 0, rhythmStep = 0;
    bool rhythmContinuation = false;
    juce::uint32 seed = 0x6d2b79f5u;
    double clock = 0.0, next = 0.0, nominalNext = 0.0, velocityStart = 0.0, expectedBeat = 0.0;
    juce::int64 retriggerBucket = -1;
    bool wasRunning = false;
    ArpeggiatorSettings previousSettings;
    juce::uint64 emittedSerial = 0, emittedStep = 0, emittedRhythmStep = 0;
    int emittedVelocity = 0, emittedSourceVelocity = 100;
    double emittedEnvelopeMs = 0;
    double emittedClock = 0, currentBpm = 120.0;
    juce::uint64 randomOctaveGroup = ~juce::uint64 { 0 };
    int randomOctaveIndex = 0;
    unsigned randomNumber();
    juce::uint32 grooveSeed = 0x92d68ca2u;
    double grooveRandom();
    void rebuild();
    void restart (bool velocity);
    void schedule (const Key&, int, const ArpeggiatorSettings&, double);
    void emit (const Key&, int transpose, const ArpeggiatorSettings&, double duration, double sampleRate,
               juce::MidiBuffer&, int sample, juce::uint64 ordinal, juce::uint64 rhythmOrdinal);
};
}
