#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/TimelineEngine.h"

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

    juce::var getTimelineState() const;
    juce::Result setTimelineState (const juce::var&);
    juce::var getTransportState() const;
    void setPlaying (bool);
    void panic();
    bool isStandalone() const { return wrapperType == wrapperType_Standalone; }

private:
    mutable juce::CriticalSection stateLock; // Never acquired by processBlock.
    juce::String progression { "C - G - Am - F" };
    chordrunner::Timeline state;
    chordrunner::SnapshotExchange<chordrunner::Timeline> snapshots;
    chordrunner::TimelineEngine engine;
    chordrunner::Arpeggiator arpeggiator;
    juce::MidiBuffer outgoingMidi, bootstrapMidi, sequenceMidi;
    std::array<bool, 16 * 128> passedNotes {};
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
