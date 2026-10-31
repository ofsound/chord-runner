#pragma once
#include <juce_graphics/juce_graphics.h>

namespace chordrunner::ui
{
inline const juce::String uiFontFamily { "Helvetica Neue" };
inline juce::FontOptions uiFont (float height, int style = juce::Font::plain)
{
    const float scaledHeight = height + juce::jmax (2.0f, height * 0.2f);
    return juce::FontOptions (uiFontFamily, scaledHeight, style);
}
// Shared desktop/iPad hierarchy inside the fitted module bodies.
inline constexpr float controlLabelSize = 30.0f;
inline constexpr int controlLabelHeight = 40;
inline constexpr float controlValueSize = 28.0f;
inline constexpr int controlValueHeight = 46;
inline constexpr float sectionHeadingSize = 28.0f;
inline const juce::Colour accentGreen { 0xff54b570 };
inline const juce::Colour accentYellow { 0xffffda63 };
}
