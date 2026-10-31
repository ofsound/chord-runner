#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/TimelineEngine.h"
#include "engine/OutputPreview.h"
#include "ui/UiScale.h"
#include "platform/IOSProjectLibrary.h"

class ArpModeParameter;

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override = default;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlockBypassed;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    std::vector<chordrunner::PreviewNote> getOutputPreview() const;
    chordrunner::ArpeggiatorSettings getArpPreviewSettings() const; // Message thread only.
    juce::var getTimelineState() const;
    juce::Result setTimelineState (const juce::var&);
    juce::var getTransportState() const;
    void setPlaying (bool);
    void panic();
    static constexpr int patternCount = 16;
    juce::var getProjectState() const;
    juce::Result setProjectState (const juce::var&, bool stop = true);
    juce::Result editTimeline (const juce::var&, const juce::String& transaction = {});
    void selectPattern (int); // -1 is the KSH mute slot, MIDI note 16.
    int selectedPattern() const { return selectedStatus.load (std::memory_order_acquire); }
    void setLaunchMode (int);
    int getLaunchMode() const { return launchMode.load(); }
    juce::Result copyPattern (int source, int destination);
    void clearPattern (int);
    void renamePattern (int, const juce::String&);
    void setProjectMetadata (const juce::String&, const juce::String&, const juce::String&);
    void newProject();
    bool canUndo() const { const juce::ScopedLock lock (stateLock); return ! undoStates.empty(); }
    bool canRedo() const { const juce::ScopedLock lock (stateLock); return ! redoStates.empty(); }
    juce::Result editChordTone (int chord, int tone, int pitch, int expectedRevision);
    juce::Result editNotes (const std::vector<chordrunner::NoteEdit>&, int expectedRevision);
    juce::Result resizeLoop (double beats, int expectedRevision);
    void beginArpModeGesture();
    void endArpModeGesture();
    void beginEditGesture() { const juce::ScopedLock lock (stateLock); lastTransaction.clear(); editGestureActive = true; }
    void endEditGesture() { const juce::ScopedLock lock (stateLock); lastTransaction.clear(); editGestureActive = false; }
    void undo();
    void redo();
    void markDocumentClean();
    void completeProjectSave (const juce::File&, const juce::var&);
    void restoreStandaloneSettings (const juce::PropertySet&);
    void saveStandaloneSettings (juce::PropertySet&) const;
    bool isDocumentModified() const;
    juce::File getProjectFile() const { const juce::ScopedLock lock (stateLock); return projectFile; }
    juce::Result saveProject (const juce::File&);
    juce::Result loadProject (const juce::File&);
   #if JUCE_IOS
    std::shared_ptr<chordrunner::IOSProjectLibrary> getIOSProjectLibrary() { return iosLibrary; }
   #endif
    void setProjectFile (const juce::File& file) { const juce::ScopedLock lock (stateLock); projectFile = file; }
    double lastMidiInputTime() const { return midiInputTime.load (std::memory_order_relaxed); }
    double lastNoteAttackTime (int note) const { return noteAttackTimes[static_cast<size_t> (juce::jlimit (0, 127, note))].load (std::memory_order_relaxed); }
    double lastMidiOutputTime() const { return midiOutputTime.load (std::memory_order_relaxed); }
    bool isStandalone() const { return wrapperType == wrapperType_Standalone; }
    bool isHosted() const { return wrapperType != wrapperType_Undefined && ! isStandalone(); }
    int getUiScalePercent() const { return uiScalePercent.load (std::memory_order_relaxed); }
    void setUiScalePercent (int value)
    {
        value = juce::jlimit (chordrunner::ui::UiScale::minimumPercent, chordrunner::ui::UiScale::maximumPercent, value);
        if (uiScalePercent.exchange (value, std::memory_order_relaxed) != value)
            revision.fetch_add (1, std::memory_order_release);
    }

private:
    friend class ArpModeParameter;
    std::array<std::atomic<int>, patternCount> liveStyles {};
    juce::AudioProcessorParameter* arpModeParameter = nullptr;
    std::atomic<int> notifiedMode { -1 };
    void notifyArpMode(); // Producers only; never called from processBlock.
    std::atomic<int> uiScalePercent { chordrunner::ui::UiScale::defaultPercent };
   #if JUCE_IOS
    std::shared_ptr<chordrunner::IOSProjectLibrary> iosLibrary;
   #endif
    mutable juce::CriticalSection stateLock; // Never acquired by processBlock.
    struct Pattern
    {
        chordrunner::Timeline timeline;
        juce::String progression { "C - G - Am - F" }, name;
    };
    struct PlaybackBank { std::array<chordrunner::Timeline, patternCount> patterns {}; };
    std::array<Pattern, patternCount> patterns;
    std::unique_ptr<chordrunner::SnapshotExchange<PlaybackBank>> snapshots;
    std::atomic<int> selectedStatus { 0 }, activeStatus { 0 }, queuedStatus { -2 }, selectionRequest { -2 }, launchMode { 3 };
    int activePattern = 0, queuedPattern = -2;
    double launchBeat = 0.0, patternOrigin = 0.0, expectedHostBeat = 0.0;
    juce::uint64 renderedRevision = 0;
    bool transportWasRunning = false;
    juce::String projectName { "Untitled Project" }, projectAuthor, projectDescription, createdAt, modifiedAt;
    juce::File projectFile;
    juce::String cleanDocument, lastTransaction;
    bool editGestureActive = false;
    double lastEditTime = 0.0;
    std::vector<juce::var> undoStates, redoStates;
    void publishBank();
    void rememberEdit (const juce::var&, const juce::String& transaction = {});
    chordrunner::TimelineEngine engine;
    chordrunner::Arpeggiator arpeggiator;
    juce::MidiBuffer outgoingMidi, bootstrapMidi, sequenceMidi, inputMidi, segmentMidi, appendMidi;
    std::array<bool, 16 * 128> passedNotes {};
    std::array<bool, 16 * 128> inputKeysDown {}; // Audio-thread owned, no UI access.
    int inputKeyCount = 0;
    std::atomic<double> midiInputTime { -1000.0 }, midiOutputTime { -1000.0 };
    std::array<std::atomic<double>, 128> noteAttackTimes {};
    std::atomic<int> heldInputStatus { 0 };
    std::atomic<int> arpInputCount { 0 };
    std::atomic<juce::uint64> arpPulseSerial { 0 }, arpPulseStep { 0 }, arpPulseRhythmStep { 0 };
    std::atomic<int> arpPulseVelocity { 0 }, arpSourceVelocity { 100 };
    std::atomic<double> arpEnvelopeMs { 0 };
    std::atomic<double> arpPulseAge { 1000 }, arpPulseDueMs { 0 };
    void trackInputKeys (const juce::MidiBuffer&, int samples, bool clear);
    std::array<bool, 16> passedSustain {};
    bool previousSequencer = true, previousArp = false, bypassed = false;
    void releasePassedNotes (juce::MidiBuffer&);
    void passMidi (const juce::MidiBuffer&, juce::MidiBuffer&);
    static constexpr int maxInputBytes = 65536;
    bool primeMidiBuffer = true;
    void finishMidiBlock (juce::MidiBuffer&);
    std::atomic<bool> playRequested { false }, resetRequested { false };
    std::atomic<bool> playingStatus { false }, hostAvailable { false };
    std::atomic<double> beatStatus { 0.0 }, tempoStatus { 120.0 };
    std::atomic<int> chordStatus { -1 }, revision { 0 };
    double internalBeat = 0.0;
    bool previousHostSync = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
