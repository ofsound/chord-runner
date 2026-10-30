#pragma once
#include "../engine/OutputPreview.h"
#include "UiPalette.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <melatonin_blur/melatonin_blur.h>

namespace chordrunner::ui
{
    // A generalized cycle or full beat-restart window through the scheduler. Anonymous input
    // ranks are independent of the sequencer, incoming MIDI, and stored chord tones.
    class ArpVisualizer final : public juce::Component
    {
    public:
        ArpVisualizer();
        void update (const ArpeggiatorSettings&, double tempo, int heldNotes = 0);
        void showOutputPulse (juce::uint64 serial, juce::uint64 step, int velocity, double ageMs, juce::uint64 rhythmStep);
        void resized() override;
        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent& event) override { mouseMove (event); }
        void mouseExit (const juce::MouseEvent&) override;
        const std::vector<PreviewNote>& previewNotes() const { return notes; }
        double windowBeats() const { return beats; }
        double orderCycleBeats() const { return naturalBeats; }
        juce::String statusText() const { return inspection.getText(); }
        float outputPulse() const { return pulse; }
        int inputRanks() const { return rankCount; }

    private:
        juce::uint64 pulseSerial = 0, pulseOrdinal = 0, pulseRhythmOrdinal = 0;
        double pulseStarted = -1000;
        float pulse = 0;
        ArpeggiatorSettings settings;
        double tempo = 120, beats = 1, naturalBeats = 1, stepBeats = 0.25;
        int stepCount = 4, hovered = -1, low = 48, high = 57;
        int heldCount = 0, rankCount = 4, referenceBase = 48, rankSpacing = 3;
        std::array<melatonin::DropShadow, 4> noteGlows {
            melatonin::DropShadow { accentYellow.withAlpha (0.015f), 4 },
            melatonin::DropShadow { accentYellow.withAlpha (0.06f), 6 },
            melatonin::DropShadow { accentYellow.withAlpha (0.13f), 7 },
            melatonin::DropShadow { accentYellow.withAlpha (0.24f), 8 }
        };
        juce::Label inspection;
        std::vector<PreviewNote> notes;
        std::vector<bool> reachesWindowEdge;
        juce::Rectangle<float> plot;
        void rebuild();
        juce::Rectangle<float> noteBounds (const PreviewNote&) const;
        juce::String description() const;
        juce::String timeText (double) const;
    };
}
