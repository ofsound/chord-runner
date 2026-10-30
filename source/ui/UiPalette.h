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
inline constexpr float sectionHeadingSize = 16.0f;
inline const juce::Colour accentGreen { 0xff54b570 };
inline const juce::Colour accentYellow { 0xffffda63 };
}
