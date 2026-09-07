#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true))
{
}

void PluginProcessor::prepareToPlay (double rate, int maximumBlockSize)
{
    // Wrappers recycle their MIDI buffers. Two preallocated buffers seed that
    // cycle without allocating when the first host/standalone buffer is empty.
    const double boundaries = std::ceil (juce::jmax (0, maximumBlockSize) * 1000.0
        / (60.0 * juce::jmax (1.0, rate) * 0.25)) + 2.0;
    const auto bytes = static_cast<size_t> (juce::jmax (2048.0, boundaries * chordrunner::maxNotes * 2.0 * 16.0));
    outgoingMidi.ensureSize (bytes);
    bootstrapMidi.ensureSize (bytes);
    primeMidiBuffer = true;
    resetRequested.store (true, std::memory_order_release);
}

void PluginProcessor::releaseResources()
{
    // Retain held notes until the next callback can deliver their note-offs.
    // The standalone holder flushes its physical endpoint when audio stops.
    resetRequested.store (true, std::memory_order_release);
    playingStatus.store (false);
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layout) const
{
    return layout.getMainInputChannelSet().isDisabled()
        && (layout.getMainOutputChannelSet().isDisabled()
            || layout.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
            || layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    audio.clear();
    midi.clear(); // A generator: incoming notes are deliberately not echoed.
    outgoingMidi.clear();
    if (audio.getNumSamples() <= 0)
        return;
    const auto oldRevision = snapshots.current().noteRevision;
    snapshots.consume();
    const auto& timeline = snapshots.current();
    const bool changed = oldRevision != timeline.noteRevision;
    const bool sync = timeline.hostSync && ! isStandalone();
    const bool reset = resetRequested.exchange (false, std::memory_order_acq_rel) || sync != previousHostSync;
    if (reset)
        internalBeat = 0.0;
    previousHostSync = sync;

    double beat = internalBeat, bpm = timeline.tempo;
    bool playing = playRequested.load (std::memory_order_acquire);
    bool available = false;
    if (sync)
    {
        playing = false;
        if (auto* playHead = getPlayHead())
        {
            if (const auto position = playHead->getPosition())
            {
                const auto hostBeat = position->getPpqPosition();
                const auto hostTempo = position->getBpm();
                if (hostBeat && hostTempo && std::isfinite (*hostBeat)
                    && std::isfinite (*hostTempo) && *hostTempo > 0.0 && *hostTempo <= 1000.0)
                {
                    available = true;
                    beat = *hostBeat;
                    bpm = *hostTempo;
                    playing = position->getIsPlaying() && playRequested.load (std::memory_order_acquire);
                }
            }
        }
    }
    engine.render (timeline, beat, bpm, getSampleRate(), audio.getNumSamples(), playing, changed || reset, outgoingMidi);
    finishMidiBlock (midi);
    if (! sync && playing && getSampleRate() > 0.0)
        internalBeat += audio.getNumSamples() * bpm / (60.0 * getSampleRate());
    playingStatus.store (playing, std::memory_order_relaxed);
    hostAvailable.store (available, std::memory_order_relaxed);
    beatStatus.store (playing ? std::fmod (juce::jmax (0.0, beat), timeline.length()) : 0.0, std::memory_order_relaxed);
    tempoStatus.store (bpm, std::memory_order_relaxed);
    chordStatus.store (engine.activeChord(), std::memory_order_relaxed);
}

void PluginProcessor::processBlockBypassed (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    audio.clear();
    midi.clear();
    outgoingMidi.clear();
    if (audio.getNumSamples() > 0)
    {
        engine.stop (outgoingMidi);
        finishMidiBlock (midi);
    }
    playingStatus.store (false);
    chordStatus.store (-1);
    resetRequested.store (true, std::memory_order_release);
}

void PluginProcessor::finishMidiBlock (juce::MidiBuffer& midi)
{
    midi.swapWith (outgoingMidi);
    if (primeMidiBuffer)
    {
        outgoingMidi.swapWith (bootstrapMidi);
        primeMidiBuffer = false;
    }
}

void PluginProcessor::setPlaying (bool playing)
{
    if (! playing || ! playRequested.load())
        resetRequested.store (true, std::memory_order_release);
    playRequested.store (playing, std::memory_order_release);
}

void PluginProcessor::panic() { setPlaying (false); }

juce::var PluginProcessor::getTimelineState() const
{
    const juce::ScopedLock lock (stateLock);
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty ("schema", 1);
    object->setProperty ("app", "Chord Runner");
    object->setProperty ("progression", progression);
    object->setProperty ("tempo", state.tempo);
    object->setProperty ("channel", state.channel);
    object->setProperty ("velocity", state.velocity);
    object->setProperty ("octave", state.octave);
    object->setProperty ("hostSync", state.hostSync);
    object->setProperty ("revision", revision.load());
    object->setProperty ("totalBeats", state.length());
    juce::Array<juce::var> chords;
    double start = 0.0;
    for (int i = 0; i < state.count; ++i)
    {
        const auto& chord = state.chords[static_cast<size_t> (i)];
        auto item = std::make_unique<juce::DynamicObject>();
        item->setProperty ("label", juce::String::fromUTF8 (chord.label.data()));
        item->setProperty ("beats", chord.beats);
        item->setProperty ("startBeat", start);
        start += chord.beats;
        juce::Array<juce::var> notes;
        for (int n = 0; n < chord.noteCount; ++n)
            notes.add (chord.notes[static_cast<size_t> (n)]);
        item->setProperty ("notes", notes);
        chords.add (juce::var (item.release()));
    }
    object->setProperty ("chords", chords);
    return juce::var (object.release());
}

namespace
{
bool numberInRange (const juce::var& value, double low, double high, bool integral = true)
{
    if (! value.isInt() && ! value.isInt64() && ! value.isDouble())
        return false;
    const double number = static_cast<double> (value);
    return std::isfinite (number) && number >= low && number <= high
        && (! integral || std::abs (std::floor (number) - number) < std::numeric_limits<double>::epsilon());
}
}

juce::Result PluginProcessor::setTimelineState (const juce::var& value)
{
    if (! value.isObject() || value["app"].toString() != "Chord Runner"
        || ! numberInRange (value["schema"], 1, 1)
        || ! numberInRange (value["tempo"], 20, 300, false)
        || ! numberInRange (value["channel"], 1, 16)
        || ! numberInRange (value["velocity"], 1, 127)
        || ! numberInRange (value["octave"], 0, 8)
        || ! value["hostSync"].isBool() || ! value["progression"].isString())
        return juce::Result::fail ("Invalid Chord Runner settings or unsupported project version.");
    chordrunner::Timeline next;
    next.tempo = static_cast<double> (value["tempo"]);
    next.channel = static_cast<int> (value["channel"]);
    next.velocity = static_cast<int> (value["velocity"]);
    next.octave = static_cast<int> (value["octave"]);
    next.hostSync = static_cast<bool> (value["hostSync"]);
    const auto text = value["progression"].toString();
    const auto result = chordrunner::parseProgression (text, next);
    if (result.failed())
        return result;
    {
        const juce::ScopedLock lock (stateLock);
        const bool notesChanged = text != progression || next.octave != state.octave
            || next.channel != state.channel || next.velocity != state.velocity;
        next.noteRevision = state.noteRevision + (notesChanged ? 1u : 0u);
        state = next;
        progression = text;
        snapshots.publish (next);
        revision.fetch_add (1);
    }
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
    return juce::Result::ok();
}

juce::var PluginProcessor::getTransportState() const
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty ("playing", playingStatus.load());
    object->setProperty ("armed", playRequested.load());
    object->setProperty ("beat", beatStatus.load());
    object->setProperty ("tempo", tempoStatus.load());
    object->setProperty ("activeChord", chordStatus.load());
    object->setProperty ("hostAvailable", hostAvailable.load());
    object->setProperty ("standalone", isStandalone());
    object->setProperty ("revision", revision.load());
    return juce::var (object.release());
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& data)
{
    const auto json = juce::JSON::toString (getTimelineState(), true);
    data.replaceAll (json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void PluginProcessor::setStateInformation (const void* data, int size)
{
    if (data == nullptr || size <= 0 || size > 65536)
        return;
    if (setTimelineState (juce::JSON::parse (juce::String::fromUTF8 (static_cast<const char*> (data), size))).wasOk())
        setPlaying (false); // Restoring a session never starts unsolicited MIDI.
}

juce::AudioProcessorEditor* PluginProcessor::createEditor() { return new PluginEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PluginProcessor(); }
