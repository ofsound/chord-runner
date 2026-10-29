#include "PluginProcessor.h"
#include <algorithm>

namespace
{
juce::var encodeTimeline (const chordrunner::Timeline& state, const juce::String& progression)
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty ("schema", 4);
    object->setProperty ("app", "Chord Runner");
    object->setProperty ("progression", progression);
    object->setProperty ("tempo", state.tempo);
    object->setProperty ("channel", state.channel);
    object->setProperty ("velocity", state.velocity);
    object->setProperty ("octave", state.octave);
    object->setProperty ("voiceLeading", state.voiceLeading);
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
    arp->setProperty ("swingSubdivision", state.arp.swingSubdivision);
    arp->setProperty ("velocityHumanize", state.arp.velocityHumanize);
    arp->setProperty ("timingHumanize", state.arp.timingHumanize);
    arp->setProperty ("gateSequenceEnabled", state.arp.gateSequenceEnabled);
    arp->setProperty ("tieSequenceEnabled", state.arp.tieSequenceEnabled);
    arp->setProperty ("gateLength", state.arp.gateLength);
    arp->setProperty ("tieLength", state.arp.tieLength);
    arp->setProperty ("gateMask", state.arp.gateMask);
    arp->setProperty ("tieMask", state.arp.tieMask);
    arp->setProperty ("interval", state.arp.interval);
    arp->setProperty ("decay", state.arp.decay);
    object->setProperty ("arp", juce::var (arp.release()));
    bool edited = false;
    juce::Array<juce::var> offsets;
    for (int c = 0; c < state.count; ++c)
    {
        juce::Array<juce::var> row;
        const auto& chord = state.chords[static_cast<size_t> (c)];
        for (int n = 0; n < chord.noteCount; ++n)
        {
            const int offset = chord.toneOffsets[static_cast<size_t> (n)];
            edited = edited || offset != 0;
            row.add (offset);
        }
        offsets.add (row);
    }
    if (edited) object->setProperty ("toneOffsets", offsets);
    object->setProperty ("totalBeats", state.length());
    juce::Array<juce::var> chords;
    double start = 0.0;
    for (int i = 0; i < state.count; ++i)
    {
        const auto& chord = state.chords[static_cast<size_t> (i)];
        auto item = std::make_unique<juce::DynamicObject>();
        item->setProperty ("edited", std::any_of (chord.toneOffsets.begin(), chord.toneOffsets.end(), [] (int offset) { return offset != 0; }));
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

juce::Result decodeTimeline (const juce::var& value, chordrunner::Timeline& next, juce::String& text)
{
    if (! value.isObject() || value["app"].toString() != "Chord Runner"
        || ! numberInRange (value["schema"], 1, 4)
        || ! numberInRange (value["tempo"], 20, 300, false)
        || ! numberInRange (value["channel"], 1, 16)
        || ! numberInRange (value["velocity"], 1, 127)
        || ! numberInRange (value["octave"], 0, 8)
        || ! value["hostSync"].isBool() || ! value["progression"].isString())
        return juce::Result::fail ("Invalid Chord Runner settings or unsupported project version.");
    next.tempo = static_cast<double> (value["tempo"]);
    next.channel = static_cast<int> (value["channel"]);
    next.velocity = static_cast<int> (value["velocity"]);
    next.octave = static_cast<int> (value["octave"]);
    if (value.hasProperty ("voiceLeading"))
    {
        if (! numberInRange (value["voiceLeading"], 0, 3)) return juce::Result::fail ("Invalid voice-leading mode.");
        next.voiceLeading = static_cast<int> (value["voiceLeading"]);
    }
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
            || ! numberInRange (arp["swing"], 0, 100, false)
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
    if (static_cast<int> (value["schema"]) >= 4)
    {
        const auto arp = value["arp"];
        if (! numberInRange (arp["swingSubdivision"], 0, 2)
            || ! numberInRange (arp["velocityHumanize"], 0, 100)
            || ! numberInRange (arp["timingHumanize"], 0, 100))
            return juce::Result::fail ("Invalid groove settings.");
        next.arp.swingSubdivision = static_cast<int> (arp["swingSubdivision"]);
        next.arp.velocityHumanize = static_cast<int> (arp["velocityHumanize"]);
        next.arp.timingHumanize = static_cast<int> (arp["timingHumanize"]);
    }
    else
    {
        // Old alternating-step swing used a 0-50 scale.
        next.arp.swing *= 2.0;
        next.arp.swingSubdivision = 0;
    }
    const auto lanes = value["arp"];
    if (lanes.hasProperty ("gateSequenceEnabled") || lanes.hasProperty ("tieSequenceEnabled")
        || lanes.hasProperty ("gateLength") || lanes.hasProperty ("tieLength")
        || lanes.hasProperty ("gateMask") || lanes.hasProperty ("tieMask"))
    {
        if (! lanes["gateSequenceEnabled"].isBool() || ! lanes["tieSequenceEnabled"].isBool()
            || ! numberInRange (lanes["gateLength"], 1, 16) || ! numberInRange (lanes["tieLength"], 1, 16)
            || ! numberInRange (lanes["gateMask"], 0, 65535) || ! numberInRange (lanes["tieMask"], 0, 65535))
            return juce::Result::fail ("Invalid gate or tie sequence.");
        next.arp.gateSequenceEnabled = static_cast<bool> (lanes["gateSequenceEnabled"]);
        next.arp.tieSequenceEnabled = static_cast<bool> (lanes["tieSequenceEnabled"]);
        next.arp.gateLength = static_cast<int> (lanes["gateLength"]);
        next.arp.tieLength = static_cast<int> (lanes["tieLength"]);
        next.arp.gateMask = static_cast<int> (lanes["gateMask"]);
        next.arp.tieMask = static_cast<int> (lanes["tieMask"]);
    }
    text = value["progression"].toString();
    const auto result = static_cast<int> (value["schema"]) < 3
        ? chordrunner::migrateLegacyProgression (text, next, text)
        : chordrunner::parseProgression (text, next);
    if (result.failed())
        return result;
    if (value.hasProperty ("toneOffsets"))
    {
        const auto* offsets = value["toneOffsets"].getArray();
        if (offsets == nullptr || offsets->size() != next.count)
            return juce::Result::fail ("Invalid chord-tone edits.");
        for (int c = 0; c < next.count; ++c)
        {
            auto& chord = next.chords[static_cast<size_t> (c)];
            const auto* row = offsets->getReference (c).getArray();
            if (row == nullptr || row->size() != chord.noteCount)
                return juce::Result::fail ("Invalid chord-tone edits.");
            for (int n = 0; n < chord.noteCount; ++n)
            {
                if (! numberInRange (row->getReference (n), -127, 127))
                    return juce::Result::fail ("Invalid chord-tone pitch.");
                const int offset = static_cast<int> (row->getReference (n));
                const int pitch = chord.notes[static_cast<size_t> (n)] + offset;
                if (pitch < 0 || pitch > 127)
                    return juce::Result::fail ("Chord tones must stay within MIDI 0-127.");
                for (int previous = 0; previous < n; ++previous)
                    if (chord.notes[static_cast<size_t> (previous)] == pitch)
                        return juce::Result::fail ("Each chord tone needs a distinct pitch.");
                chord.toneOffsets[static_cast<size_t> (n)] = offset;
                chord.notes[static_cast<size_t> (n)] = pitch;
            }
        }
    }
    return juce::Result::ok();
}
}

void PluginProcessor::publishBank()
{
    PlaybackBank bank;
    for (int i = 0; i < patternCount; ++i)
        bank.patterns[static_cast<size_t> (i)] = patterns[static_cast<size_t> (i)].timeline;
    snapshots->publish (bank);
    revision.fetch_add (1, std::memory_order_release);
}

juce::var PluginProcessor::getTimelineState() const
{
    const juce::ScopedLock lock (stateLock);
    const int selection = selectedStatus.load (std::memory_order_acquire);
    const auto& pattern = patterns[static_cast<size_t> (juce::jlimit (0, patternCount - 1, selection))];
    auto value = encodeTimeline (pattern.timeline, pattern.progression);
    value.getDynamicObject()->setProperty ("revision", revision.load());
    value.getDynamicObject()->setProperty ("projectName", projectName);
    value.getDynamicObject()->setProperty ("patternName", pattern.name);
    value.getDynamicObject()->setProperty ("selectedPattern", selection);
    return value;
}

juce::Result PluginProcessor::setTimelineState (const juce::var& value)
{
    chordrunner::Timeline next;
    juce::String text;
    if (const auto result = decodeTimeline (value, next, text); result.failed())
        return result;
    {
        const juce::ScopedLock lock (stateLock);
        const int target = value.hasProperty ("selectedPattern") ? static_cast<int> (value["selectedPattern"]) : selectedStatus.load();
        if (target != selectedStatus.load() || target < -1 || target >= patternCount)
            return juce::Result::fail ("The selected pattern changed while editing. Try the edit again.");
        auto& pattern = patterns[static_cast<size_t> (juce::jmax (0, target))];
        const auto& state = pattern.timeline;
        bool tonesChanged = next.count != state.count;
        for (int c = 0; c < juce::jmin (next.count, state.count); ++c)
            tonesChanged = tonesChanged || next.chords[static_cast<size_t> (c)].notes != state.chords[static_cast<size_t> (c)].notes;
        const bool changed = tonesChanged || text != pattern.progression || next.octave != state.octave
            || next.channel != state.channel || next.voiceLeading != state.voiceLeading || next.velocity != state.velocity
            || next.sequencerEnabled != state.sequencerEnabled || next.arp != state.arp;
        next.noteRevision = state.noteRevision + (changed ? 1u : 0u);
        // Tempo and clock belong to the project, and never restart held notes.
        for (auto& item : patterns)
        {
            item.timeline.tempo = next.tempo;
            item.timeline.hostSync = next.hostSync;
        }
        pattern.timeline = next;
        pattern.progression = text;
        publishBank();
    }
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
    return juce::Result::ok();
}

juce::var PluginProcessor::getProjectState() const
{
    const juce::ScopedLock lock (stateLock);
    auto document = std::make_unique<juce::DynamicObject>();
    document->setProperty ("app", "Chord Runner");
    document->setProperty ("schema", 4);
    document->setProperty ("selectedPattern", selectedStatus.load());
    document->setProperty ("launchMode", launchMode.load());
    document->setProperty ("projectName", projectName);
    document->setProperty ("projectAuthor", projectAuthor);
    document->setProperty ("projectDescription", projectDescription);
    document->setProperty ("createdAt", createdAt);
    document->setProperty ("modifiedAt", modifiedAt);
    juce::Array<juce::var> slots;
    for (const auto& pattern : patterns)
    {
        auto item = encodeTimeline (pattern.timeline, pattern.progression);
        item.getDynamicObject()->removeProperty ("chords");
        item.getDynamicObject()->removeProperty ("totalBeats");
        item.getDynamicObject()->setProperty ("name", pattern.name);
        slots.add (item);
    }
    document->setProperty ("patterns", slots);
    return juce::var (document.release());
}

juce::Result PluginProcessor::setProjectState (const juce::var& value, bool stop)
{
    const juce::ScopedLock documentLock (stateLock);
    std::array<Pattern, patternCount> next;
    int selection = 0, mode = 3;
    juce::String name { "Untitled Project" }, author, description, created, modified;
    if (value.hasProperty ("patterns"))
    {
        const auto* slots = value["patterns"].getArray();
        if (value["app"].toString() != "Chord Runner" || ! numberInRange (value["schema"], 4, 4)
            || slots == nullptr || slots->size() != patternCount
            || ! numberInRange (value["selectedPattern"], -1, 15) || ! numberInRange (value["launchMode"], 0, 4))
            return juce::Result::fail ("Invalid pattern bank or unsupported project version.");
        for (int i = 0; i < patternCount; ++i)
        {
            auto& pattern = next[static_cast<size_t> (i)];
            const auto& slot = slots->getReference (i);
            if (! slot["name"].isString() || slot["name"].toString().length() > 64)
                return juce::Result::fail ("Invalid pattern name.");
            const auto result = decodeTimeline (slot, pattern.timeline, pattern.progression);
            if (result.failed())
                return result;
            pattern.name = slot["name"].toString();
        }
        for (const auto* key : { "projectName", "projectAuthor", "projectDescription", "createdAt", "modifiedAt" })
            if (! value[key].isString() || value[key].toString().length() > 4096)
                return juce::Result::fail ("Invalid project details.");
        selection = static_cast<int> (value["selectedPattern"]);
        mode = static_cast<int> (value["launchMode"]);
        name = value["projectName"].toString();
        author = value["projectAuthor"].toString();
        description = value["projectDescription"].toString();
        created = value["createdAt"].toString();
        modified = value["modifiedAt"].toString();
    }
    else
    {
        const auto result = decodeTimeline (value, next[0].timeline, next[0].progression);
        if (result.failed())
            return result;
        for (int i = 1; i < patternCount; ++i)
        {
            next[static_cast<size_t> (i)].timeline.tempo = next[0].timeline.tempo;
            next[static_cast<size_t> (i)].timeline.hostSync = next[0].timeline.hostSync;
        }
        for (int i = 0; i < patternCount; ++i)
            next[static_cast<size_t> (i)].name = "Pattern " + juce::String (i + 1);
    }
    if (stop)
        setPlaying (false);
    {
        const juce::ScopedLock lock (stateLock);
        for (int i = 0; i < patternCount; ++i)
            next[static_cast<size_t> (i)].timeline.noteRevision = patterns[static_cast<size_t> (i)].timeline.noteRevision + 1;
        patterns = std::move (next);
        projectName = name.trim().isEmpty() ? juce::String ("Untitled Project") : name;
        projectAuthor = author;
        projectDescription = description;
        createdAt = created;
        modifiedAt = modified;
        launchMode.store (mode);
        if (selection != selectedStatus.exchange (selection))
            selectionRequest.store (selection, std::memory_order_release);
        publishBank();
    }
    if (stop)
    {
        projectFile = juce::File {};
        undoStates.clear();
        redoStates.clear();
        editGestureActive = false;
        lastTransaction.clear();
        markDocumentClean();
    }
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
    return juce::Result::ok();
}

void PluginProcessor::rememberEdit (const juce::var& before, const juce::String& transaction)
{
    const juce::ScopedLock documentLock (stateLock);
    if (juce::JSON::toString (before, true) == juce::JSON::toString (getProjectState(), true))
        return;
    const auto now = juce::Time::getMillisecondCounterHiRes();
    if (transaction.isEmpty() || transaction != lastTransaction || (now - lastEditTime > 450.0 && ! editGestureActive) || undoStates.empty())
    {
        if (undoStates.size() == 128)
            undoStates.erase (undoStates.begin());
        undoStates.push_back (before);
    }
    redoStates.clear();
    lastTransaction = transaction;
    lastEditTime = now;
}

juce::Result PluginProcessor::editTimeline (const juce::var& value, const juce::String& transaction)
{
    const juce::ScopedLock documentLock (stateLock);
    const auto before = getProjectState();
    const auto result = setTimelineState (value);
    if (result.wasOk())
        rememberEdit (before, transaction);
    return result;
}

void PluginProcessor::selectPattern (int slot)
{
    const juce::ScopedLock documentLock (stateLock);
    if (slot < -1 || slot >= patternCount)
        return;
    selectedStatus.store (slot, std::memory_order_release);
    selectionRequest.store (slot, std::memory_order_release);
    lastTransaction.clear();
    revision.fetch_add (1);
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

void PluginProcessor::setLaunchMode (int mode)
{
    const juce::ScopedLock documentLock (stateLock);
    if (mode < 0 || mode > 4)
        return;
    const auto before = getProjectState();
    launchMode.store (mode);
    revision.fetch_add (1);
    rememberEdit (before);
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

juce::Result PluginProcessor::copyPattern (int source, int destination)
{
    const juce::ScopedLock documentLock (stateLock);
    if (source < 0 || source >= patternCount || destination < 0 || destination >= patternCount)
        return juce::Result::fail ("Choose a pattern to copy.");
    const auto before = getProjectState();
    {
        const juce::ScopedLock lock (stateLock);
        auto copy = patterns[static_cast<size_t> (source)];
        copy.timeline.noteRevision = patterns[static_cast<size_t> (destination)].timeline.noteRevision + 1;
        patterns[static_cast<size_t> (destination)] = std::move (copy);
        publishBank();
    }
    rememberEdit (before);
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
    return juce::Result::ok();
}

void PluginProcessor::clearPattern (int slot)
{
    const juce::ScopedLock documentLock (stateLock);
    if (slot < 0 || slot >= patternCount)
        return;
    const auto before = getProjectState();
    {
        const juce::ScopedLock lock (stateLock);
        auto& pattern = patterns[static_cast<size_t> (slot)];
        Pattern blank;
        blank.timeline.tempo = pattern.timeline.tempo;
        blank.timeline.hostSync = pattern.timeline.hostSync;
        blank.timeline.noteRevision = pattern.timeline.noteRevision + 1;
        blank.name = "Pattern " + juce::String (slot + 1);
        pattern = blank;
        publishBank();
    }
    rememberEdit (before);
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

void PluginProcessor::renamePattern (int slot, const juce::String& name)
{
    const juce::ScopedLock documentLock (stateLock);
    if (slot < 0 || slot >= patternCount)
        return;
    const auto before = getProjectState();
    {
        const juce::ScopedLock lock (stateLock);
        patterns[static_cast<size_t> (slot)].name = name.trim().substring (0, 64);
        revision.fetch_add (1);
    }
    rememberEdit (before);
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

void PluginProcessor::setProjectMetadata (const juce::String& name, const juce::String& author, const juce::String& description)
{
    const juce::ScopedLock documentLock (stateLock);
    const auto before = getProjectState();
    {
        const juce::ScopedLock lock (stateLock);
        projectName = name.trim().substring (0, 128);
        if (projectName.isEmpty())
            projectName = "Untitled Project";
        projectAuthor = author.substring (0, 256);
        projectDescription = description.substring (0, 4096);
        revision.fetch_add (1);
    }
    rememberEdit (before);
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

void PluginProcessor::undo()
{
    const juce::ScopedLock documentLock (stateLock);
    if (undoStates.empty())
        return;
    const auto target = undoStates.back();
    redoStates.push_back (getProjectState());
    undoStates.pop_back();
    setProjectState (target, false);
    lastTransaction.clear();
}

void PluginProcessor::redo()
{
    const juce::ScopedLock documentLock (stateLock);
    if (redoStates.empty())
        return;
    const auto target = redoStates.back();
    undoStates.push_back (getProjectState());
    redoStates.pop_back();
    setProjectState (target, false);
    lastTransaction.clear();
}

namespace
{
juce::String documentFingerprint (const juce::var& document)
{
    auto value = document.clone();
    value.getDynamicObject()->removeProperty ("modifiedAt");
    value.getDynamicObject()->removeProperty ("createdAt");
    return juce::JSON::toString (value, true);
}
}
void PluginProcessor::markDocumentClean() { const juce::ScopedLock lock (stateLock); cleanDocument = documentFingerprint (getProjectState()); revision.fetch_add (1); }
bool PluginProcessor::isDocumentModified() const { const juce::ScopedLock lock (stateLock); return cleanDocument != documentFingerprint (getProjectState()); }

void PluginProcessor::newProject()
{
    const juce::ScopedLock documentLock (stateLock);
   #if JUCE_IOS
    iosLibrary->cancel();
   #endif
    auto document = getProjectState();
    auto* object = document.getDynamicObject();
    object->setProperty ("projectName", "Untitled Project");
    object->setProperty ("projectAuthor", "");
    object->setProperty ("projectDescription", "");
    const auto now = juce::Time::getCurrentTime().toISO8601 (true);
    object->setProperty ("createdAt", now);
    object->setProperty ("modifiedAt", now);
    object->setProperty ("selectedPattern", 0);
    object->setProperty ("launchMode", 3);
    juce::Array<juce::var> slots;
    for (int i = 0; i < patternCount; ++i)
    {
        auto item = encodeTimeline (chordrunner::Timeline {}, "C - G - Am - F");
        item.getDynamicObject()->setProperty ("name", "Pattern " + juce::String (i + 1));
        slots.add (item);
    }
    object->setProperty ("patterns", slots);
    setProjectState (document);
}

juce::Result PluginProcessor::saveProject (const juce::File& file)
{
    const juce::ScopedLock documentLock (stateLock);
    if (! file.getParentDirectory().createDirectory())
        return juce::Result::fail ("The project folder could not be created.");
    auto document = getProjectState();
    const auto now = juce::Time::getCurrentTime().toISO8601 (true);
    document.getDynamicObject()->setProperty ("modifiedAt", now);
    if (createdAt.isEmpty())
        document.getDynamicObject()->setProperty ("createdAt", now);
    const auto text = juce::JSON::toString (document);
    juce::TemporaryFile temporary (file);
    if (! temporary.getFile().replaceWithText (text) || ! temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("The project could not be saved. Check file access or use Save As.");
    completeProjectSave (file, document);
    return juce::Result::ok();
}

juce::Result PluginProcessor::loadProject (const juce::File& file)
{
    const juce::ScopedLock documentLock (stateLock);
    if (file.getSize() <= 0 || file.getSize() > 1048576)
        return juce::Result::fail ("The project is empty or too large.");
    const auto result = setProjectState (juce::JSON::parse (file.loadFileAsString()));
    if (result.wasOk())
        projectFile = file;
    return result;
}

void PluginProcessor::completeProjectSave (const juce::File& file, const juce::var& document)
{
    const juce::ScopedLock documentLock (stateLock);
    modifiedAt = document["modifiedAt"].toString();
    createdAt = document["createdAt"].toString();
    projectFile = file;
    cleanDocument = documentFingerprint (document);
    revision.fetch_add (1);
}

void PluginProcessor::restoreStandaloneSettings (const juce::PropertySet& properties)
{
    const juce::ScopedLock documentLock (stateLock);
    const auto path = properties.getValue ("projectFile");
    if (juce::File::isAbsolutePath (path)) projectFile = juce::File (path);
    cleanDocument = properties.getValue ("cleanDocument", cleanDocument);
   #if JUCE_IOS
    iosLibrary->restoreAccess (properties);
   #endif
}

void PluginProcessor::saveStandaloneSettings (juce::PropertySet& properties) const
{
    const juce::ScopedLock documentLock (stateLock);
    properties.setValue ("projectFile", projectFile.getFullPathName());
    properties.setValue ("cleanDocument", cleanDocument);
   #if JUCE_IOS
    iosLibrary->saveAccess (properties);
   #endif
}

std::vector<chordrunner::PreviewNote> PluginProcessor::getOutputPreview() const
{
    chordrunner::Timeline timeline;
    {
        const juce::ScopedLock lock (stateLock);
        const int selection = selectedStatus.load();
        if (selection < 0) return {};
        timeline = patterns[static_cast<size_t> (selection)].timeline;
    }
    return chordrunner::renderOutputPreview (timeline);
}

juce::Result PluginProcessor::editChordTone (int chordIndex, int toneIndex, int pitch, int expectedRevision)
{
    const juce::ScopedLock lock (stateLock);
    const int selection = selectedStatus.load();
    if (revision.load() != expectedRevision || selection < 0)
        return juce::Result::fail ("The pattern changed during the note drag.");
    auto& timeline = patterns[static_cast<size_t> (selection)].timeline;
    if (chordIndex < 0 || chordIndex >= timeline.count || pitch < 0 || pitch > 127)
        return juce::Result::fail ("Invalid chord-tone pitch.");
    auto& chord = timeline.chords[static_cast<size_t> (chordIndex)];
    if (toneIndex < 0 || toneIndex >= chord.noteCount) return juce::Result::fail ("Invalid chord tone.");
    for (int n = 0; n < chord.noteCount; ++n)
        if (n != toneIndex && chord.notes[static_cast<size_t> (n)] == pitch)
            return juce::Result::fail ("Each chord tone needs a distinct pitch.");
    if (chord.notes[static_cast<size_t> (toneIndex)] == pitch) return juce::Result::ok();
    const auto before = getProjectState();
    if (selectedStatus.load() != selection || revision.load() != expectedRevision)
        return juce::Result::fail ("The pattern changed during the note drag.");
    // Edit the already compiled voicing. Dragging must not rerun progression
    // parsing or the whole voice-leading search on every pointer movement.
    chord.toneOffsets[static_cast<size_t> (toneIndex)] += pitch - chord.notes[static_cast<size_t> (toneIndex)];
    chord.notes[static_cast<size_t> (toneIndex)] = pitch;
    ++timeline.noteRevision;
    publishBank();
    rememberEdit (before, "chord-tone drag");
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
    return juce::Result::ok();
}

chordrunner::ArpeggiatorSettings PluginProcessor::getArpPreviewSettings() const
{
    const juce::ScopedLock lock (stateLock);
    const int selection = selectedStatus.load();
    return patterns[static_cast<size_t> (juce::jmax (0, selection))].timeline.arp;
}
