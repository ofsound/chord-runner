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
        / (60.0 * juce::jmax (1.0, rate) * 0.0625 * 0.5)) + 4.0;
    // Fastest sync rate at 1000 BPM, 50% swing, 128-note chord trigger,
    // overlapping gates, plus all held passthrough notes and bounded input.
    const auto bytes = static_cast<size_t> (maxInputBytes + 65536.0
        + (boundaries + juce::jmin (juce::jmax (0, maximumBlockSize), maxInputBytes / 8))
            * chordrunner::Arpeggiator::capacity * 4.0 * 16.0);
    outgoingMidi.ensureSize (bytes);
    bootstrapMidi.ensureSize (bytes);
    sequenceMidi.ensureSize (static_cast<size_t> (65536.0 + boundaries * chordrunner::maxNotes * 32.0));
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
    outgoingMidi.clear();
    if (audio.getNumSamples() <= 0)
        return;
    sequenceMidi.clear();
    const auto oldRevision = snapshots.current().noteRevision;
    snapshots.consume();
    const auto& timeline = snapshots.current();
    const bool changed = oldRevision != timeline.noteRevision;
    const bool routeChanged = timeline.sequencerEnabled != previousSequencer || timeline.arp.enabled != previousArp || bypassed;
    bypassed = false;
    const bool sync = timeline.hostSync && ! isStandalone();
    const bool reset = resetRequested.exchange (false, std::memory_order_acq_rel) || sync != previousHostSync;
    if (routeChanged || reset)
    {
        engine.stop (previousArp ? sequenceMidi : outgoingMidi);
        arpeggiator.stop (outgoingMidi);
        releasePassedNotes (outgoingMidi);
        sequenceMidi.clear();
    }
    previousSequencer = timeline.sequencerEnabled;
    previousArp = timeline.arp.enabled;
    if (reset)
        internalBeat = 0.0;
    previousHostSync = sync;

    double beat = internalBeat, bpm = timeline.tempo;
    bool playing = ! timeline.sequencerEnabled || playRequested.load (std::memory_order_acquire);
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
                    playing = position->getIsPlaying() && (! timeline.sequencerEnabled || playRequested.load (std::memory_order_acquire));
                }
            }
        }
    }
    if (timeline.sequencerEnabled)
    {
        engine.render (timeline, beat, bpm, getSampleRate(), audio.getNumSamples(), playing,
                       changed || reset || routeChanged, timeline.arp.enabled ? sequenceMidi : outgoingMidi);
        if (timeline.arp.enabled)
            arpeggiator.render (timeline.arp, sequenceMidi, outgoingMidi, beat, bpm, getSampleRate(),
                                audio.getNumSamples(), playing, changed);
    }
    else if (timeline.arp.enabled)
    {
        if (midi.data.size() <= maxInputBytes)
            arpeggiator.render (timeline.arp, midi, outgoingMidi, beat, bpm, getSampleRate(),
                                audio.getNumSamples(), playing, changed);
        else
            arpeggiator.stop (outgoingMidi);
    }
    else
        passMidi (midi, outgoingMidi);
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
    outgoingMidi.clear();
    sequenceMidi.clear();
    if (audio.getNumSamples() > 0)
    {
        engine.stop (previousArp ? sequenceMidi : outgoingMidi);
        arpeggiator.stop (outgoingMidi);
        if (! bypassed)
            releasePassedNotes (outgoingMidi);
        passMidi (midi, outgoingMidi);
        finishMidiBlock (midi);
        bypassed = true;
    }
    playingStatus.store (false);
    chordStatus.store (-1);
    resetRequested.store (true, std::memory_order_release);
}

void PluginProcessor::releasePassedNotes (juce::MidiBuffer& midi)
{
    for (int channel = 1; channel <= 16; ++channel)
    {
        if (passedSustain[static_cast<size_t> (channel - 1)])
            midi.addEvent (juce::MidiMessage::controllerEvent (channel, 64, 0), 0);
        for (int note = 0; note < 128; ++note)
            if (passedNotes[static_cast<size_t> ((channel - 1) * 128 + note)])
                midi.addEvent (juce::MidiMessage::noteOff (channel, note), 0);
    }
    passedNotes.fill (false);
    passedSustain.fill (false);
}

void PluginProcessor::passMidi (const juce::MidiBuffer& input, juce::MidiBuffer& output)
{
    if (input.data.size() > maxInputBytes)
    {
        releasePassedNotes (output);
        return;
    }
    for (const auto event : input)
    {
        output.addEvent (event.data, event.numBytes, event.samplePosition);
        if (event.numBytes <= 3)
        {
            const juce::MidiMessage message (event.data, event.numBytes);
            if (message.isNoteOnOrOff())
                passedNotes[static_cast<size_t> ((message.getChannel() - 1) * 128 + message.getNoteNumber())] = message.isNoteOn();
            if (message.isController() && message.getControllerNumber() == 64)
                passedSustain[static_cast<size_t> (message.getChannel() - 1)] = message.getControllerValue() >= 64;
        }
    }
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
    object->setProperty ("schema", 3);
    object->setProperty ("app", "Chord Runner");
    object->setProperty ("progression", progression);
    object->setProperty ("tempo", state.tempo);
    object->setProperty ("channel", state.channel);
    object->setProperty ("velocity", state.velocity);
    object->setProperty ("octave", state.octave);
    object->setProperty ("hostSync", state.hostSync);
    object->setProperty ("sequencerEnabled", state.sequencerEnabled);
    auto arp = std::make_unique<juce::DynamicObject>();
    arp->setProperty ("enabled", state.arp.enabled);
    arp->setProperty ("sync", state.arp.sync);
    arp->setProperty ("hold", state.arp.hold);
    arp->setProperty ("velocityEnabled", state.arp.velocityEnabled);
    arp->setProperty ("velocityRetrigger", state.arp.velocityRetrigger);
    arp->setProperty ("style", state.arp.style);
    arp->setProperty ("steps", state.arp.steps);
    arp->setProperty ("distance", state.arp.distance);
    arp->setProperty ("offset", state.arp.offset);
    arp->setProperty ("retrigger", state.arp.retrigger);
    arp->setProperty ("repeats", state.arp.repeats);
    arp->setProperty ("target", state.arp.target);
    arp->setProperty ("rate", state.arp.rate);
    arp->setProperty ("freeMs", state.arp.freeMs);
    arp->setProperty ("gate", state.arp.gate);
    arp->setProperty ("swing", state.arp.swing);
    arp->setProperty ("interval", state.arp.interval);
    arp->setProperty ("decay", state.arp.decay);
    object->setProperty ("arp", juce::var (arp.release()));
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
        || ! numberInRange (value["schema"], 1, 3)
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
    if (static_cast<int> (value["schema"]) >= 2)
    {
        const auto arp = value["arp"];
        if (! value["sequencerEnabled"].isBool() || ! arp.isObject()
            || ! arp["enabled"].isBool()
            || ! arp["sync"].isBool()
            || ! arp["hold"].isBool()
            || ! arp["velocityEnabled"].isBool()
            || ! arp["velocityRetrigger"].isBool()
            || ! numberInRange (arp["style"], 0, 10)
            || ! numberInRange (arp["steps"], 0, 8)
            || ! numberInRange (arp["distance"], -36, 36)
            || ! numberInRange (arp["offset"], 0, 128)
            || ! numberInRange (arp["retrigger"], 0, 2)
            || ! numberInRange (arp["repeats"], 0, 64)
            || ! numberInRange (arp["target"], 0, 127)
            || ! numberInRange (arp["rate"], 0.0625, 16, false)
            || ! numberInRange (arp["freeMs"], 10, 2000, false)
            || ! numberInRange (arp["gate"], 1, 200, false)
            || ! numberInRange (arp["swing"], 0, 50, false)
            || ! numberInRange (arp["interval"], 0.0625, 64, false)
            || ! numberInRange (arp["decay"], 1, 60000, false)
            )
            return juce::Result::fail ("Invalid sequencer or arpeggiator settings.");
        next.sequencerEnabled = static_cast<bool> (value["sequencerEnabled"]);
        next.arp.enabled = static_cast<bool> (arp["enabled"]);
        next.arp.sync = static_cast<bool> (arp["sync"]);
        next.arp.hold = static_cast<bool> (arp["hold"]);
        next.arp.velocityEnabled = static_cast<bool> (arp["velocityEnabled"]);
        next.arp.velocityRetrigger = static_cast<bool> (arp["velocityRetrigger"]);
        next.arp.style = static_cast<int> (arp["style"]);
        next.arp.steps = static_cast<int> (arp["steps"]);
        next.arp.distance = static_cast<int> (arp["distance"]);
        next.arp.offset = static_cast<int> (arp["offset"]);
        next.arp.retrigger = static_cast<int> (arp["retrigger"]);
        next.arp.repeats = static_cast<int> (arp["repeats"]);
        next.arp.target = static_cast<int> (arp["target"]);
        next.arp.rate = static_cast<double> (arp["rate"]);
        next.arp.freeMs = static_cast<double> (arp["freeMs"]);
        next.arp.gate = static_cast<double> (arp["gate"]);
        next.arp.swing = static_cast<double> (arp["swing"]);
        next.arp.interval = static_cast<double> (arp["interval"]);
        next.arp.decay = static_cast<double> (arp["decay"]);
    }
    auto text = value["progression"].toString();
    const auto result = static_cast<int> (value["schema"]) < 3
        ? chordrunner::migrateLegacyProgression (text, next, text)
        : chordrunner::parseProgression (text, next);
    if (result.failed())
        return result;
    {
        const juce::ScopedLock lock (stateLock);
        const bool notesChanged = text != progression || next.octave != state.octave
            || next.channel != state.channel || next.velocity != state.velocity
            || next.sequencerEnabled != state.sequencerEnabled || next.arp != state.arp;
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
