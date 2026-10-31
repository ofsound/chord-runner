#include "TechnicalBackdrop.h"
#include <array>
#include <cmath>

namespace chordrunner::ui
{
void TechnicalBackdrop::resize (int width, int height)
{
    if (width == cachedWidth && height == cachedHeight) return;
    cachedWidth = width;
    cachedHeight = height;
    grid.clear(); majorGrid.clear(); rearMesh.clear(); frontMesh.clear(); accents.clear();
    const float w = static_cast<float> (width), h = static_cast<float> (height);
    const auto segment = [] (juce::Path& path, juce::Point<float> a, juce::Point<float> b)
    {
        path.startNewSubPath (a);
        path.lineTo (b);
    };
    for (int x = 0; x <= width; x += 20)
        segment (x % 100 == 0 ? majorGrid : grid, { static_cast<float> (x), 0 }, { static_cast<float> (x), h });
    for (int y = 0; y <= height; y += 20)
        segment (y % 100 == 0 ? majorGrid : grid, { 0, static_cast<float> (y) }, { w, static_cast<float> (y) });

    struct Vector3
    {
        float x, y, z;
    };
    struct Surface
    {
        int kind;
        float x, y, size, yaw, tilt, roll;
    };
    // A torus, Gabriel's horn and a one-sheet hyperboloid. The surfaces have
    // independent 3D orientations; the drafting grid stays square to the editor.
    const std::array<Surface, 4> surfaces {{
        { 0, w * 0.34f, h * 0.075f, 116, 0.35f, 0.92f, -0.38f },
        { 1, w * 0.86f, h * 0.14f, 104, -0.55f, 0.46f, -0.58f },
        { 2, w * 0.055f, h * 0.59f, 112, 0.45f, 0.35f, 0.44f },
        { 0, w * 0.78f, h * 0.96f, 136, -0.45f, 1.06f, 0.32f }
    }};
    constexpr float tau = juce::MathConstants<float>::twoPi;
    for (const auto& surface : surfaces)
    {
        const auto pointAt = [&] (float u, float v) -> Vector3
        {
            const float angle = u * tau;
            if (surface.kind == 0)
            {
                const float tube = v * tau;
                const float radius = 1.0f + 0.34f * std::cos (tube);
                return { radius * std::cos (angle), radius * std::sin (angle), 0.34f * std::sin (tube) };
            }
            if (surface.kind == 1)
            {
                const float t = 1.0f + v * 4.0f;
                return { (t - 2.6f) * 0.65f, std::cos (angle) / t, std::sin (angle) / t };
            }
            const float z = (v - 0.5f) * 2.8f;
            const float radius = 0.42f * std::sqrt (1.0f + z * z * 1.8f);
            return { radius * std::cos (angle), radius * std::sin (angle), z };
        };
        const auto rotate = [&] (Vector3 p)
        {
            const float x = p.x * std::cos (surface.yaw) + p.z * std::sin (surface.yaw);
            const float z = -p.x * std::sin (surface.yaw) + p.z * std::cos (surface.yaw);
            const float y = p.y * std::cos (surface.tilt) - z * std::sin (surface.tilt);
            p.z = p.y * std::sin (surface.tilt) + z * std::cos (surface.tilt);
            p.x = x * std::cos (surface.roll) - y * std::sin (surface.roll);
            p.y = x * std::sin (surface.roll) + y * std::cos (surface.roll);
            return p;
        };
        const auto project = [&] (Vector3 p)
        {
            const float perspective = 5.0f / (5.0f - p.z);
            return juce::Point<float> { surface.x + p.x * surface.size * perspective,
                surface.y + p.y * surface.size * perspective };
        };
        const auto meshLine = [&] (bool ring, float fixed, bool accent)
        {
            auto previous = rotate (pointAt (ring ? 0.0f : fixed, ring ? fixed : 0.0f));
            for (int step = 1; step <= 80; ++step)
            {
                const float t = static_cast<float> (step) / 80.0f;
                const auto next = rotate (pointAt (ring ? t : fixed, ring ? fixed : t));
                auto& mesh = previous.z + next.z > 0 ? frontMesh : rearMesh;
                segment (mesh, project (previous), project (next));
                if (accent && previous.z + next.z > 0)
                    segment (accents, project (previous), project (next));
                previous = next;
            }
        };
        const int rings = surface.kind == 0 ? 12 : 10;
        for (int ring = 0; ring <= rings; ++ring)
        {
            if (surface.kind == 0 && ring == rings) break;
            meshLine (true, static_cast<float> (ring) / static_cast<float> (rings), ring % 5 == 0);
        }
        for (int meridian = 0; meridian < 12; ++meridian)
            meshLine (false, static_cast<float> (meridian) / 12.0f, false);
    }
}
void TechnicalBackdrop::paint (juce::Graphics& g, juce::Rectangle<int> clip) const
{
    juce::Graphics::ScopedSaveState saved (g);
    g.reduceClipRegion (clip);
    const float w = static_cast<float> (cachedWidth), h = static_cast<float> (cachedHeight);
    // A restrained cobalt wash adds colour without lifting the dark panel surround.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17436c).withAlpha (0.30f),
        w * 0.84f, h * 0.18f, juce::Colours::transparentBlack, w * 0.2f, h * 0.85f, true));
    g.fillRect (0.0f, 0.0f, w, h);
    const juce::Colour gridInk (0xff7b94aa), meshInk (0xff7baed0), blue (0xff559de0);
    g.setColour (gridInk.withAlpha (0.028f));
    g.strokePath (grid, juce::PathStrokeType (0.5f));
    g.setColour (gridInk.withAlpha (0.05f));
    g.strokePath (majorGrid, juce::PathStrokeType (0.6f));
    g.setColour (meshInk.withAlpha (0.065f));
    g.strokePath (rearMesh, juce::PathStrokeType (0.55f));
    g.setColour (meshInk.withAlpha (0.16f));
    g.strokePath (frontMesh, juce::PathStrokeType (0.7f));
    g.setColour (blue.withAlpha (0.24f));
    g.strokePath (accents, juce::PathStrokeType (0.9f));
}
}
