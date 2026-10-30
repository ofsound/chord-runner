#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true))
{
   #if JUCE_IOS
    iosLibrary = std::make_shared<chordrunner::IOSProjectLibrary>();
   #endif
    snapshots = std::make_unique<chordrunner::SnapshotExchange<PlaybackBank>>();
    for (int i = 0; i < patternCount; ++i)
        patterns[static_cast<size_t> (i)].name = "Pattern " + juce::String (i + 1);
    createdAt = modifiedAt = juce::Time::getCurrentTime().toISO8601 (true);
    markDocumentClean();
}

void PluginProcessor::prepareToPlay (double rate, int maximumBlockSize)
{
    // Wrappers recycle their MIDI buffers. Two preallocated buffers seed that
    // cycle without allocating when the first host/standalone buffer is empty.
    const double boundaries = std::ceil (juce::jmax (0, maximumBlockSize) * 1000.0
        / (60.0 * juce::jmax (1.0, rate) * 0.0625 * 0.5)) + 4.0;
    // Fastest sync rate at 1000 BPM, swing/humanize overlap, 128-note chord trigger,
    // overlapping gates, plus all held passthrough notes and bounded input.
    const auto bytes = static_cast<size_t> (maxInputBytes + 65536.0
        + (boundaries + juce::jmin (juce::jmax (0, maximumBlockSize), maxInputBytes / 8))
            * chordrunner::Arpeggiator::capacity * 4.0 * 16.0);
    outgoingMidi.ensureSize (bytes);
    bootstrapMidi.ensureSize (bytes);
    sequenceMidi.ensureSize (static_cast<size_t> (65536.0 + boundaries * chordrunner::maxNotes * 32.0));
    appendMidi.ensureSize (maxInputBytes + 8);
    inputMidi.ensureSize (maxInputBytes + 8);
    segmentMidi.ensureSize (bytes);
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
    const int samples = audio.getNumSamples();
    if (samples <= 0)
        return;
    const double blockTimeMs = juce::Time::getMillisecondCounterHiRes();
    snapshots->consume();
    const auto& bank = snapshots->current();
    const auto& master = bank.patterns[static_cast<size_t> (juce::jmax (0, activePattern))];
    const bool sync = master.hostSync && ! isStandalone();
    bool reset = resetRequested.exchange (false, std::memory_order_acq_rel) || sync != previousHostSync || bypassed;
    const bool hadQueuedPattern = queuedPattern != -2;
    if (reset)
    {
        internalBeat = patternOrigin = 0.0;
        queuedPattern = -2;
    }
    trackInputKeys (midi, samples, reset);
    bypassed = false;
    previousHostSync = sync;
    double beat = internalBeat, bpm = master.tempo;
    bool playing = ! master.sequencerEnabled || playRequested.load (std::memory_order_acquire);
    bool available = false, hostPlaying = false;
    if (sync)
    {
        playing = false;
        if (auto* playHead = getPlayHead())
            if (const auto position = playHead->getPosition())
            {
                const auto hostBeat = position->getPpqPosition();
                const auto hostTempo = position->getBpm();
                if (hostBeat && hostTempo && std::isfinite (*hostBeat) && std::abs (*hostBeat) <= 1.0e12
                    && std::isfinite (*hostTempo) && *hostTempo > 0.0 && *hostTempo <= 1000.0)
                {
                    available = true;
                    beat = *hostBeat;
                    bpm = *hostTempo;
                    hostPlaying = position->getIsPlaying();
                    playing = hostPlaying && (! master.sequencerEnabled || playRequested.load (std::memory_order_acquire));
                }
            }
    }
    const auto beatsPerSample = bpm / (60.0 * juce::jmax (1.0, getSampleRate()));
    auto queue = [&] (int slot, double at)
    {
        queuedPattern = slot;
        const int mode = launchMode.load (std::memory_order_relaxed);
        const double quantum = mode == 1 ? 0.25 : mode == 2 ? 1.0 : mode == 3 ? 4.0 : master.length();
        const double origin = mode == 4 ? patternOrigin : 0.0;
        launchBeat = ! playing || mode == 0 ? at : origin + std::ceil ((at - origin - 1.0e-9) / quantum) * quantum;
        queuedStatus.store (slot, std::memory_order_release);
    };
    if (transportWasRunning && std::abs (beat - expectedHostBeat) > 1.0e-7)
    {
        patternOrigin = 0.0;
        if (queuedPattern != -2)
            queue (queuedPattern, beat);
    }
    int request = selectionRequest.exchange (-2, std::memory_order_acq_rel);
    if (reset && (hadQueuedPattern || selectedStatus.load() != activePattern))
        request = selectedStatus.load();
    if (request != -2)
        queue (request, beat);
    bool force = reset;
    auto launch = [&] (double at)
    {
        activePattern = queuedPattern;
        queuedPattern = -2;
        patternOrigin = at;
        force = true;
        activeStatus.store (activePattern, std::memory_order_release);
        queuedStatus.store (-2, std::memory_order_release);
    };
    auto render = [&] (int start, int count)
    {
        if (count <= 0)
            return;
        const auto previousTrigger = arpeggiator.triggerSerial();
        segmentMidi.clear();
        sequenceMidi.clear();
        inputMidi.clear();
        const double localBeat = beat + start * beatsPerSample - patternOrigin;
        if (activePattern < 0)
        {
            engine.stop (previousArp ? sequenceMidi : segmentMidi);
            arpeggiator.stop (segmentMidi);
            releasePassedNotes (segmentMidi);
        }
        else
        {
            const auto& timeline = bank.patterns[static_cast<size_t> (activePattern)];
            const bool segmentPlaying = (! timeline.sequencerEnabled || playRequested.load (std::memory_order_acquire))
                && (! sync || hostPlaying);
            const bool changed = renderedRevision != timeline.noteRevision;
            const bool route = timeline.sequencerEnabled != previousSequencer || timeline.arp.enabled != previousArp;
            if (route || force)
            {
                engine.stop (previousArp ? sequenceMidi : segmentMidi);
                arpeggiator.stop (segmentMidi);
                releasePassedNotes (segmentMidi);
                sequenceMidi.clear();
            }
            previousSequencer = timeline.sequencerEnabled;
            previousArp = timeline.arp.enabled;
            renderedRevision = timeline.noteRevision;
            if (midi.data.size() <= maxInputBytes)
                for (const auto event : midi)
                {
                    if (event.samplePosition < start || event.samplePosition >= start + count)
                        continue;
                    const bool selection = event.numBytes == 3 && (event.data[0] & 0xe0) == 0x80 && event.data[1] <= 16;
                    if (! selection)
                        inputMidi.addEvent (event.data, event.numBytes, event.samplePosition - start);
                }
            if (timeline.sequencerEnabled)
            {
                engine.render (timeline, localBeat, bpm, getSampleRate(), count, segmentPlaying, changed || force || route,
                               timeline.arp.enabled ? sequenceMidi : segmentMidi);
                if (timeline.arp.enabled)
                    arpeggiator.render (timeline.arp, sequenceMidi, segmentMidi, localBeat, bpm, getSampleRate(), count, segmentPlaying, changed || force);
            }
            else if (timeline.arp.enabled)
            {
                if (midi.data.size() <= maxInputBytes)
                    arpeggiator.render (timeline.arp, inputMidi, segmentMidi, localBeat, bpm, getSampleRate(), count, segmentPlaying, changed || force);
                else
                    arpeggiator.stop (segmentMidi);
            }
            else if (midi.data.size() <= maxInputBytes)
                passMidi (inputMidi, segmentMidi);
            else
                releasePassedNotes (segmentMidi);
        }
        if (arpeggiator.triggerSerial() != previousTrigger)
            arpPulseDueMs.store (blockTimeMs + (start + count) * 1000.0 / juce::jmax (1.0, getSampleRate())
                - arpeggiator.triggerAgeMs (getSampleRate()), std::memory_order_relaxed);
        // Concatenate already ordered segments in linear time. A one-event
        // scratch buffer lets JUCE encode timestamps without an ever-growing
        // addEvent search or relying on the private MIDI storage format.
        if (start == 0)
            outgoingMidi.data.addArray (segmentMidi.data);
        else
            for (const auto event : segmentMidi)
            {
                appendMidi.clear();
                appendMidi.addEvent (event.data, event.numBytes, event.samplePosition + start);
                outgoingMidi.data.addArray (appendMidi.data);
            }
        force = false;
    };
    int cursor = 0;
    auto renderThrough = [&] (int end)
    {
        if (queuedPattern != -2)
        {
            const int boundary = juce::jlimit (cursor, samples,
                static_cast<int> (juce::jlimit (0.0, static_cast<double> (samples), std::ceil ((launchBeat - beat) / beatsPerSample - 1.0e-7))));
            if (boundary < end || (boundary == end && end < samples))
            {
                render (cursor, boundary - cursor);
                cursor = boundary;
                launch (beat + boundary * beatsPerSample);
            }
        }
        render (cursor, end - cursor);
        cursor = end;
    };
    if (midi.data.size() <= maxInputBytes)
        for (const auto event : midi)
        {
            if (event.numBytes != 3 || (event.data[0] & 0xf0) != 0x90 || event.data[2] == 0 || event.data[1] > 16)
                continue;
            const int at = juce::jlimit (cursor, samples - 1, event.samplePosition);
            renderThrough (at);
            const int slot = event.data[1] == 16 ? -1 : event.data[1];
            selectedStatus.store (slot, std::memory_order_release);
            revision.fetch_add (1, std::memory_order_relaxed);
            queue (slot, beat + at * beatsPerSample);
        }
    renderThrough (samples);
    if (! sync && playing)
        internalBeat += samples * beatsPerSample;
    transportWasRunning = playing;
    expectedHostBeat = beat + samples * beatsPerSample;
    arpPulseRhythmStep.store (arpeggiator.triggerRhythmStep(), std::memory_order_relaxed);
    arpPulseStep.store (arpeggiator.triggerStep(), std::memory_order_relaxed);
    arpPulseVelocity.store (arpeggiator.triggerVelocity(), std::memory_order_relaxed);
    arpPulseAge.store (arpeggiator.triggerAgeMs (getSampleRate()), std::memory_order_relaxed);
    arpInputCount.store (arpeggiator.inputNoteCount(), std::memory_order_relaxed);
    arpPulseSerial.store (arpeggiator.triggerSerial(), std::memory_order_release);
    finishMidiBlock (midi);
    playingStatus.store (playing, std::memory_order_relaxed);
    hostAvailable.store (available, std::memory_order_relaxed);
    const auto length = bank.patterns[static_cast<size_t> (juce::jmax (0, activePattern))].length();
    beatStatus.store (playing ? std::fmod (juce::jmax (0.0, beat + samples * beatsPerSample - patternOrigin), length) : 0.0, std::memory_order_relaxed);
    tempoStatus.store (bpm, std::memory_order_relaxed);
    chordStatus.store (activePattern < 0 ? -1 : engine.activeChord(), std::memory_order_relaxed);
}

void PluginProcessor::processBlockBypassed (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    audio.clear();
    outgoingMidi.clear();
    sequenceMidi.clear();
    if (audio.getNumSamples() > 0)
    {
        trackInputKeys (midi, audio.getNumSamples(), ! bypassed);
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

void PluginProcessor::trackInputKeys (const juce::MidiBuffer& input, int samples, bool clear)
{
    if (! input.isEmpty())
        midiInputTime.store (juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);
    if (clear || input.data.size() > maxInputBytes)
    {
        inputKeysDown.fill (false);
        inputKeyCount = 0;
    }
    if (input.data.size() <= maxInputBytes)
        for (const auto event : input)
        {
            if (event.samplePosition < 0 || event.samplePosition >= samples || event.numBytes < 1) continue;
            const auto* bytes = event.data;
            if (bytes[0] == 0xff && event.numBytes == 1)
            {
                inputKeysDown.fill (false);
                inputKeyCount = 0;
            }
            if (event.numBytes != 3 || bytes[1] > 127 || bytes[2] > 127) continue;
            const int channel = bytes[0] & 0x0f;
            const int type = bytes[0] & 0xf0;
            if (type == 0xb0 && (bytes[1] == 120 || bytes[1] == 123))
            {
                for (int key = 0; key < 128; ++key)
                {
                    auto& down = inputKeysDown[static_cast<size_t> (channel * 128 + key)];
                    if (down) { down = false; --inputKeyCount; }
                }
            }
            // Notes 0..16 are consumed as pattern selection, never arp input.
            if ((type != 0x80 && type != 0x90) || bytes[1] <= 16) continue;
            auto& down = inputKeysDown[static_cast<size_t> (channel * 128 + bytes[1])];
            const bool next = type == 0x90 && bytes[2] > 0;
            if (next != down) { down = next; inputKeyCount += next ? 1 : -1; }
        }
    heldInputStatus.store (inputKeyCount, std::memory_order_relaxed);
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
    // Rewrite channel-voice statuses in place, retaining timestamps and buffer storage.
    // System messages (including SysEx and realtime clock) have no channel.
    auto* bytes = outgoingMidi.data.begin();
    for (const auto event : outgoingMidi)
        if (event.numBytes > 0 && event.data[0] >= 0x80 && event.data[0] < 0xf0)
            bytes[event.data - bytes] = static_cast<juce::uint8> (event.data[0] & 0xf0);
    if (! outgoingMidi.isEmpty())
        midiOutputTime.store (juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);
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

juce::var PluginProcessor::getTransportState() const
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty ("selectedPattern", selectedStatus.load());
    object->setProperty ("activePattern", activeStatus.load());
    object->setProperty ("queuedPattern", queuedStatus.load());
    object->setProperty ("playing", playingStatus.load());
    object->setProperty ("armed", playRequested.load());
    object->setProperty ("beat", beatStatus.load());
    object->setProperty ("tempo", tempoStatus.load());
    object->setProperty ("activeChord", chordStatus.load());
    object->setProperty ("arpPulseSerial", static_cast<juce::int64> (arpPulseSerial.load (std::memory_order_acquire)));
    object->setProperty ("arpPulseStep", static_cast<juce::int64> (arpPulseStep.load (std::memory_order_relaxed)));
    object->setProperty ("arpPulseRhythmStep", static_cast<juce::int64> (arpPulseRhythmStep.load (std::memory_order_relaxed)));
    object->setProperty ("arpPulseVelocity", arpPulseVelocity.load (std::memory_order_relaxed));
    object->setProperty ("arpPulseSampleAgeMs", arpPulseAge.load (std::memory_order_relaxed));
    object->setProperty ("arpPulseAgeMs", juce::Time::getMillisecondCounterHiRes() - arpPulseDueMs.load (std::memory_order_relaxed));
    object->setProperty ("arpInputNotes", resetRequested.load (std::memory_order_acquire) ? 0 : arpInputCount.load (std::memory_order_relaxed));
    object->setProperty ("heldInputNotes", resetRequested.load (std::memory_order_acquire) ? 0 : heldInputStatus.load (std::memory_order_relaxed));
    object->setProperty ("hostAvailable", hostAvailable.load());
    object->setProperty ("standalone", isStandalone());
    object->setProperty ("revision", revision.load());
    return juce::var (object.release());
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& data)
{
    const auto json = juce::JSON::toString (getProjectState(), true);
    data.replaceAll (json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void PluginProcessor::setStateInformation (const void* data, int size)
{
    if (data == nullptr || size <= 0 || size > 1048576)
        return;
    if (setProjectState (juce::JSON::parse (juce::String::fromUTF8 (static_cast<const char*> (data), size))).wasOk())
        setPlaying (false); // Restoring a session never starts unsolicited MIDI.
}

juce::AudioProcessorEditor* PluginProcessor::createEditor() { return new PluginEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PluginProcessor(); }
