#pragma once
#include "../engine/Arpeggiator.h"
#include "UiPalette.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <melatonin_blur/melatonin_blur.h>

namespace chordrunner::ui
{
class VelocityEnvelopeView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    VelocityEnvelopeView();
    void update (const ArpeggiatorSettings&, int startVelocity, bool live, int outputVelocity, double envelopeMs, double bpm = 120, int notes = 4);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void visibilityChanged() override { if (! isVisible()) finishGesture(); }
    void enablementChanged() override { if (! isEnabled()) finishGesture(); }
    ~VelocityEnvelopeView() override { finishGesture(); }
    juce::Point<float> handlePosition (int index) const;
    std::function<void (int, int, double)> onEdit;
    std::function<void()> onGestureStart, onGestureEnd;
private:
    ArpeggiatorSettings settings;
    int start = 100, output = 0, activeHandle = -1, dragSource = -1;
    bool liveInput = false;
    double ageMs = 0, runMs = 1000, dragRatio = 0;
    float dragOriginX = 0;
    juce::Rectangle<float> plotBounds() const;
    double ratio() const;
    int endVelocity() const;
    void finishGesture();
    melatonin::DropShadow glow { accentYellow.withAlpha (0.3f), 5 };
};
}
