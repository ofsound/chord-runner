#pragma once
#include "EditorController.h"
#include "UiPalette.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace chordrunner::ui
{
class SequenceLoopBar final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SequenceLoopBar (EditorController&);
    ~SequenceLoopBar() override { finish(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void enablementChanged() override { if (! isEnabled()) finish(); }
    void visibilityChanged() override { if (! isVisible()) finish(); }
private:
    EditorController& controller;
    int mode = 0, pattern = -1, revision = -1;
    double oldStart = 0, oldEnd = 0, anchor = 0;
    float originX = 0;
    bool dragged = false, toggleOnClick = false;
    double beatAt (float) const;
    float xAt (double) const;
    void finish();
};
}
