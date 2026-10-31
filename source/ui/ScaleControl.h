#pragma once
#include "UiScale.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace chordrunner::ui
{
class ScaleControl final : public juce::Component, public juce::SettableTooltipClient
{
public:
    ScaleControl();
    void setValue (int);
    int getValue() const { return percent; }
    std::function<void (int)> onChange;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    juce::TextEditor input;
    int percent = UiScale::defaultPercent, dragStartPercent = 100, dragStartY = 0;
    bool dragging = false;
    void update (int);
    void acceptInput();
};
}
