#pragma once
#include <juce_graphics/juce_graphics.h>
#include <cmath>

namespace chordrunner::ui
{
inline const juce::String uiFontFamily { "Helvetica Neue" };
inline juce::FontOptions uiFont (float height, int style = juce::Font::plain)
{
    const float scaledHeight = height + juce::jmax (2.0f, height * 0.2f);
    return juce::FontOptions (uiFontFamily, scaledHeight, style);
}
// Fit by changing point size, never by narrowing glyphs.
inline juce::Font fitUiFont (juce::Font font, const juce::String& text, float width, float height)
{
    const float naturalWidth = juce::GlyphArrangement::getStringWidth (font, text);
    const float scale = juce::jmin (1.0f, juce::jmax (1.0f, width) / juce::jmax (1.0f, naturalWidth),
        juce::jmax (1.0f, height) / juce::jmax (1.0f, font.getHeight()));
    return font.withHeight (font.getHeight() * scale);
}
// Shared desktop/iPad hierarchy inside the fitted module bodies.
inline constexpr float controlLabelSize = 30.0f;
inline constexpr int controlLabelHeight = 40;
inline constexpr float controlValueSize = 28.0f;
inline constexpr int controlValueHeight = 46;
inline constexpr float sectionHeadingSize = 28.0f;
inline const juce::Colour accentGreen { 0xff54b570 };
inline const juce::Colour accentYellow { 0xffffda63 };
inline juce::Colour velocityColour (int velocity)
{
    const float amount = juce::jlimit (0.0f, 1.0f, static_cast<float> (velocity) / 127.0f);
    // Soft notes lose both luminance and opacity; hard accents approach warm white.
    const float highlight = std::pow (juce::jlimit (0.0f, 1.0f, (amount - 0.65f) / 0.35f), 1.4f);
    return juce::Colour (0xff675426).interpolatedWith (accentYellow, std::pow (amount, 1.4f))
        .interpolatedWith (juce::Colour (0xfffff9e2), highlight)
        .withAlpha (0.18f + 0.82f * std::pow (amount, 0.65f));
}
}
