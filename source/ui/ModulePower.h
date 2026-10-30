#pragma once
#include "UiPalette.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <melatonin_blur/melatonin_blur.h>

namespace chordrunner::ui
{
class ModulePower final : public juce::TextButton
{
public:
    explicit ModulePower (const juce::String& name) : juce::TextButton (name) { setTooltip (name); }
    void setFaceDiameter (float value) { faceDiameter = value; repaint(); }
    void paintButton (juce::Graphics&, bool, bool) override;
private:
    float faceDiameter = 28.0f;
    melatonin::DropShadow glow { accentGreen.withAlpha (0.55f), 6 };
    melatonin::InnerShadow recess { juce::Colours::black.withAlpha (0.6f), 3, { 0, 1 } };
};

}
