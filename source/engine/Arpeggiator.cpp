#include "Arpeggiator.h"
#include <algorithm>
#include <cmath>

namespace chordrunner
{
unsigned Arpeggiator::randomNumber()
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

double Arpeggiator::grooveRandom()
{
    grooveSeed ^= grooveSeed << 13;
    grooveSeed ^= grooveSeed >> 17;
    grooveSeed ^= grooveSeed << 5;
    return static_cast<double> (grooveSeed) / 4294967296.0;
}

void Arpeggiator::stop (juce::MidiBuffer& output, int sample, bool clearKeys)
{
    for (int i = 0; i < voiceCount; ++i)
    {
        const auto& voice = voices[static_cast<size_t> (i)];
        output.addEvent (juce::MidiMessage::noteOff (voice.channel, voice.note), sample);
    }
    voiceCount = pendingCount = 0;
    if (clearKeys)
    {
        keyCount = orderCount = 0;
        sustain.fill (false);
    }
    wasRunning = false;
    retriggerBucket = -1;
    restart (true);
}

void Arpeggiator::restart (bool velocity)
{
    step = 0;
    pendingCount = 0;
    next = nominalNext = clock;
    if (velocity)
        velocityStart = clock;
}

void Arpeggiator::rebuild()
{
    orderCount = keyCount;
    for (int i = 0; i < keyCount; ++i)
        order[static_cast<size_t> (i)] = i;
    std::sort (order.begin(), order.begin() + keyCount, [this] (int a, int b)
    {
        const auto& left = keys[static_cast<size_t> (a)];
        const auto& right = keys[static_cast<size_t> (b)];
        return left.note == right.note ? left.channel < right.channel : left.note < right.note;
    });
    shuffle = order;
    for (int i = keyCount - 1; i > 0; --i)
        std::swap (shuffle[static_cast<size_t> (i)], shuffle[randomNumber() % static_cast<unsigned> (i + 1)]);
}

void Arpeggiator::schedule (const Key& key, int transpose, const ArpeggiatorSettings& settings, double duration)
{
    if (pendingCount >= static_cast<int> (pending.size()))
        return;
    const auto subdivision = static_cast<juce::uint64> (1u << juce::jlimit (0, 2, settings.swingSubdivision));
    const double swingDelay = (step / subdivision) % 2 != 0
        ? static_cast<double> (subdivision) * 0.5 * settings.swing / 100.0 : 0.0;
    const double jitter = settings.timingHumanize > 0
        ? (grooveRandom() * 2.0 - 1.0) * 0.2 * settings.timingHumanize / 100.0 : 0.0;
    pending[static_cast<size_t> (pendingCount++)] = { key, transpose, nominalNext + duration * (swingDelay + jitter), duration };
}

void Arpeggiator::emit (const Key& key, int transpose, const ArpeggiatorSettings& settings, double duration,
                       double sampleRate, juce::MidiBuffer& output, int sample)
{
    const int note = key.note + transpose;
    if (note < 0 || note > 127)
        return;
    for (int i = voiceCount - 1; i >= 0; --i)
    {
        const auto& voice = voices[static_cast<size_t> (i)];
        if (voice.note == note && voice.channel == key.channel)
        {
            output.addEvent (juce::MidiMessage::noteOff (voice.channel, voice.note), sample);
            voices[static_cast<size_t> (i)] = voices[static_cast<size_t> (--voiceCount)];
        }
    }
    auto velocity = key.velocity;
    if (settings.velocityEnabled)
    {
        const auto fraction = juce::jlimit (0.0, 1.0, (clock - velocityStart) / (sampleRate * settings.decay / 1000.0));
        velocity = juce::roundToInt (key.velocity + (settings.target - key.velocity) * fraction);
    }
    if (settings.velocityHumanize > 0 && velocity > 0)
        velocity = juce::jlimit (1, 127, juce::roundToInt (velocity + (grooveRandom() * 2.0 - 1.0)
            * velocity * settings.velocityHumanize / 100.0));
    if (velocity <= 0 || voiceCount >= static_cast<int> (voices.size()))
        return;
    output.addEvent (juce::MidiMessage::noteOn (key.channel, note, static_cast<juce::uint8> (velocity)), sample);
    voices[static_cast<size_t> (voiceCount++)] = { note, key.channel, key.note, clock + juce::jmax (1.0, duration * settings.gate / 100.0) };
}

void Arpeggiator::render (const ArpeggiatorSettings& settings, const juce::MidiBuffer& input, juce::MidiBuffer& output,
                         double beat, double bpm, double sampleRate, int samples, bool running, bool changed)
{
    if (samples <= 0)
        return;
    if (! running || ! std::isfinite (beat) || std::abs (beat) > 1.0e12 || ! std::isfinite (bpm)
        || bpm <= 0.0 || ! std::isfinite (sampleRate) || sampleRate <= 0.0)
    {
        stop (output);
        return;
    }
    if (wasRunning && std::abs (beat - expectedBeat) > 1.0e-7)
        stop (output);
    if (changed)
    {
        stop (output, 0, false);
        if (! settings.hold)
        {
            for (int i = keyCount - 1; i >= 0; --i)
                if (! keys[static_cast<size_t> (i)].down && ! sustain[static_cast<size_t> (keys[static_cast<size_t> (i)].channel - 1)])
                {
                    std::move (keys.begin() + i + 1, keys.begin() + keyCount, keys.begin() + i);
                    --keyCount;
                }
            rebuild();
        }
    }
    const auto beatsPerSample = bpm / (60.0 * sampleRate);
    const auto duration = settings.sync ? settings.rate / beatsPerSample : settings.freeMs * sampleRate / 1000.0;
    auto event = input.begin();
    for (int sample = 0; sample < samples; ++sample, clock += 1.0)
    {
        sampleMidi.clear();
        // JUCE inserts by searching the whole buffer. Batch one sample at a time
        // and concatenate its already-ordered storage to avoid quadratic work
        // across dense chord-trigger blocks. Both buffers are preallocated.
        struct AppendSample
        {
            juce::MidiBuffer& destination;
            const juce::MidiBuffer& source;
            ~AppendSample() { destination.data.addArray (source.data); }
        } append { output, sampleMidi };
        bool dirty = false, newNote = false;
        while (event != input.end() && (*event).samplePosition <= sample)
        {
            const auto metadata = *event++;
            // Inspect short messages without constructing an allocating SysEx MidiMessage.
            if (metadata.numBytes < 1)
                continue;
            if (metadata.numBytes > 3)
            {
                sampleMidi.addEvent (metadata.data, metadata.numBytes, sample);
                continue;
            }
            const juce::MidiMessage message (metadata.data, metadata.numBytes);
            const int channel = message.getChannel();
            if (message.isNoteOn())
            {
                const bool anyDown = std::any_of (keys.begin(), keys.begin() + keyCount, [] (const Key& key) { return key.down; });
                if (settings.hold && ! anyDown)
                {
                    stop (sampleMidi, sample);
                    dirty = true;
                }
                int index = 0;
                while (index < keyCount && (keys[static_cast<size_t> (index)].note != message.getNoteNumber()
                    || keys[static_cast<size_t> (index)].channel != channel))
                    ++index;
                if (index < keyCount && settings.hold)
                {
                    std::move (keys.begin() + index + 1, keys.begin() + keyCount, keys.begin() + index);
                    --keyCount;
                }
                else if (index < capacity)
                {
                    keys[static_cast<size_t> (index)] = { message.getNoteNumber(), channel, message.getVelocity(), true };
                    if (index == keyCount)
                        ++keyCount;
                }
                dirty = newNote = true;
            }
            else if (message.isNoteOff())
            {
                for (int i = keyCount - 1; i >= 0; --i)
                    if (keys[static_cast<size_t> (i)].note == message.getNoteNumber() && keys[static_cast<size_t> (i)].channel == channel)
                    {
                        keys[static_cast<size_t> (i)].down = false;
                        if (! settings.hold && ! sustain[static_cast<size_t> (channel - 1)])
                        {
                            for (int v = voiceCount - 1; v >= 0; --v)
                                if (voices[static_cast<size_t> (v)].source == keys[static_cast<size_t> (i)].note
                                    && voices[static_cast<size_t> (v)].channel == channel)
                                {
                                    sampleMidi.addEvent (juce::MidiMessage::noteOff (channel, voices[static_cast<size_t> (v)].note), sample);
                                    voices[static_cast<size_t> (v)] = voices[static_cast<size_t> (--voiceCount)];
                                }
                            std::move (keys.begin() + i + 1, keys.begin() + keyCount, keys.begin() + i);
                            --keyCount;
                            dirty = true;
                        }
                    }
            }
            else if (message.isAllNotesOff() || message.isAllSoundOff())
            {
                // A channel panic must not leave a latched or sustained pattern sounding.
                for (int i = voiceCount - 1; i >= 0; --i)
                    if (voices[static_cast<size_t> (i)].channel == channel)
                    {
                        sampleMidi.addEvent (juce::MidiMessage::noteOff (channel, voices[static_cast<size_t> (i)].note), sample);
                        voices[static_cast<size_t> (i)] = voices[static_cast<size_t> (--voiceCount)];
                    }
                for (int i = keyCount - 1; i >= 0; --i)
                    if (keys[static_cast<size_t> (i)].channel == channel)
                    {
                        std::move (keys.begin() + i + 1, keys.begin() + keyCount, keys.begin() + i);
                        --keyCount;
                    }
                sustain[static_cast<size_t> (channel - 1)] = false;
                dirty = true;
                sampleMidi.addEvent (message, sample);
            }
            else if (message.isController() && message.getControllerNumber() == 64)
            {
                sustain[static_cast<size_t> (channel - 1)] = message.getControllerValue() >= 64;
                if (! sustain[static_cast<size_t> (channel - 1)] && ! settings.hold)
                    for (int i = keyCount - 1; i >= 0; --i)
                        if (keys[static_cast<size_t> (i)].channel == channel && ! keys[static_cast<size_t> (i)].down)
                        {
                            for (int v = voiceCount - 1; v >= 0; --v)
                                if (voices[static_cast<size_t> (v)].source == keys[static_cast<size_t> (i)].note
                                    && voices[static_cast<size_t> (v)].channel == channel)
                                {
                                    sampleMidi.addEvent (juce::MidiMessage::noteOff (channel, voices[static_cast<size_t> (v)].note), sample);
                                    voices[static_cast<size_t> (v)] = voices[static_cast<size_t> (--voiceCount)];
                                }
                            std::move (keys.begin() + i + 1, keys.begin() + keyCount, keys.begin() + i);
                            --keyCount;
                            dirty = true;
                        }
            }
            else
                sampleMidi.addEvent (metadata.data, metadata.numBytes, sample);
        }
        if (dirty)
        {
            if (keyCount == 0)
                stop (sampleMidi, sample);
            // Release tails from removed source notes before any replacement chord.
            else if (newNote && settings.retrigger == 1)
            {
                const auto previousVelocityStart = velocityStart;
                stop (sampleMidi, sample, false);
                if (! settings.velocityRetrigger)
                    velocityStart = previousVelocityStart;
            }
            if (orderCount == 0 && keyCount > 0)
                restart (true);
            rebuild();
        }
        for (int i = voiceCount - 1; i >= 0; --i)
            if (clock + 1.0e-7 >= voices[static_cast<size_t> (i)].off)
            {
                const auto& voice = voices[static_cast<size_t> (i)];
                sampleMidi.addEvent (juce::MidiMessage::noteOff (voice.channel, voice.note), sample);
                voices[static_cast<size_t> (i)] = voices[static_cast<size_t> (--voiceCount)];
            }
        const double currentBeat = beat + sample * beatsPerSample;
        if (settings.retrigger == 2 && currentBeat >= 0.0)
        {
            const auto bucket = static_cast<juce::int64> (std::floor ((currentBeat + 1.0e-9) / settings.interval));
            if (bucket != retriggerBucket)
            {
                restart (settings.velocityRetrigger);
                retriggerBucket = bucket;
            }
        }
        auto flushPending = [&]
        {
            int remaining = 0;
            for (int i = 0; i < pendingCount; ++i)
            {
                const auto hit = pending[static_cast<size_t> (i)];
                if (clock + 1.0e-7 < hit.at)
                {
                    pending[static_cast<size_t> (remaining++)] = hit;
                    continue;
                }
                const bool present = std::any_of (keys.begin(), keys.begin() + keyCount, [&] (const Key& key)
                {
                    return key.note == hit.key.note && key.channel == hit.key.channel;
                });
                if (present)
                    emit (hit.key, hit.transpose, settings, hit.duration, sampleRate, sampleMidi, sample);
            }
            pendingCount = remaining;
        };
        flushPending();
        if (keyCount == 0 || currentBeat < -1.0e-9 || clock + 1.0e-7 < next)
            continue;
        if (step == 0 || clock - nominalNext > duration)
            nominalNext = clock;
        const int baseLength = settings.style == ArpeggiatorSettings::chordTrigger ? 1
            : (settings.style == ArpeggiatorSettings::upDown || settings.style == ArpeggiatorSettings::downUp)
                ? juce::jmax (1, 2 * keyCount - 2) : keyCount;
        const int length = baseLength * (settings.steps + 1);
        if (settings.repeats > 0 && step >= static_cast<juce::uint64> (length * settings.repeats))
            continue;
        const int position = static_cast<int> ((step + static_cast<juce::uint64> (settings.offset)) % static_cast<juce::uint64> (length));
        const int transpose = position / baseLength * settings.distance;
        int index = position % baseLength;
        switch (settings.style)
        {
            case ArpeggiatorSettings::down: index = keyCount - 1 - index; break;
            case ArpeggiatorSettings::upDown: if (index >= keyCount) index = 2 * keyCount - 2 - index; break;
            case ArpeggiatorSettings::downUp: index = keyCount - 1 - (index < keyCount ? index : 2 * keyCount - 2 - index); break;
            case ArpeggiatorSettings::converge: index = index % 2 == 0 ? index / 2 : keyCount - 1 - index / 2; break;
            case ArpeggiatorSettings::diverge:
            {
                const int reverse = keyCount - 1 - index;
                index = reverse % 2 == 0 ? reverse / 2 : keyCount - 1 - reverse / 2;
                break;
            }
            case ArpeggiatorSettings::random: index = static_cast<int> (randomNumber() % static_cast<unsigned> (keyCount)); break;
            default: break;
        }
        if (settings.style == ArpeggiatorSettings::randomOther && step > 0 && step % static_cast<juce::uint64> (keyCount) == 0)
            for (int i = keyCount - 1; i > 0; --i)
                std::swap (shuffle[static_cast<size_t> (i)], shuffle[randomNumber() % static_cast<unsigned> (i + 1)]);
        if (settings.style == ArpeggiatorSettings::chordTrigger)
            for (int i = 0; i < keyCount; ++i)
                schedule (keys[static_cast<size_t> (order[static_cast<size_t> (i)])], transpose, settings, duration);
        else
        {
            const int keyIndex = settings.style == ArpeggiatorSettings::playOrder ? index
                : settings.style == ArpeggiatorSettings::randomOther || settings.style == ArpeggiatorSettings::randomOnce
                    ? shuffle[static_cast<size_t> (index)] : order[static_cast<size_t> (index)];
            schedule (keys[static_cast<size_t> (keyIndex)], transpose, settings, duration);
        }
        flushPending();
        // Generate ahead of the nominal onset for bipolar timing humanize.
        // Delayed hits live in fixed storage, so 4x swing may cross later steps.
        if (clock - nominalNext > duration)
            nominalNext = clock;
        ++step;
        nominalNext += duration;
        next = nominalNext - (settings.timingHumanize > 0 ? duration * 0.2 : 0.0);
    }
    wasRunning = true;
    expectedBeat = beat + samples * beatsPerSample;
}
}
