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
    grid.clear(); majorGrid.clear(); webs.clear(); wisps.clear(); construction.clear();
    const float w = static_cast<float> (width), h = static_cast<float> (height);
    const auto segment = [] (juce::Path& path, juce::Point<float> a, juce::Point<float> b)
    {
        path.startNewSubPath (a); path.lineTo (b);
    };
    for (int x = 0; x <= width; x += 12)
        segment (x % 60 == 0 ? majorGrid : grid, { static_cast<float> (x), 0 }, { static_cast<float> (x), h });
    for (int y = 0; y <= height; y += 12)
        segment (y % 60 == 0 ? majorGrid : grid, { 0, static_cast<float> (y) }, { w, static_cast<float> (y) });

    // Straight-edged webs: nested irregular polygons, spokes, triangulated bridges,
    // registration marks, dimension ticks, and tiny square junctions.
    constexpr int spokes = 24;
    for (int cluster = 0; cluster < 15; ++cluster)
    {
        const float cx = w * (0.04f + static_cast<float> (cluster % 5) * 0.235f);
        const float cy = h * (0.11f + static_cast<float> (cluster / 5) * 0.39f);
        const float radius = 96.0f + static_cast<float> ((cluster * 47) % 180);
        std::array<juce::Point<float>, spokes> previous {};
        for (int ring = 0; ring < 14; ++ring)
        {
            std::array<juce::Point<float>, spokes> points {};
            const float t = static_cast<float> (ring + 1) / 14.0f;
            for (int i = 0; i < spokes; ++i)
            {
                const float angle = static_cast<float> (i) * juce::MathConstants<float>::twoPi / spokes + static_cast<float> (cluster) * 0.17f;
                const float reach = radius * t * (0.82f + 0.18f * std::sin (angle * 3.0f + static_cast<float> (cluster)));
                points[static_cast<size_t> (i)] = { cx + std::cos (angle) * reach, cy + std::sin (angle) * reach * 0.67f };
            }
            for (int i = 0; i < spokes; ++i)
            {
                const auto index = static_cast<size_t> (i), next = static_cast<size_t> ((i + 1) % spokes);
                segment (webs, points[index], points[next]);
                if (ring > 0)
                {
                    segment (webs, previous[index], points[index]);
                    if ((ring + i + cluster) % 3 == 0) segment (construction, previous[index], points[next]);
                }
                if (ring % 4 == 0 && i % 3 == 0)
                    construction.addRectangle (points[index].x - 1.5f, points[index].y - 1.5f, 3, 3);
            }
            previous = points;
        }
        segment (construction, { cx - 12, cy }, { cx + 12, cy });
        segment (construction, { cx, cy - 12 }, { cx, cy + 12 });
        const float dimensionY = cy + radius * 0.76f;
        segment (construction, { cx - radius, dimensionY }, { cx + radius, dimensionY });
        for (int tick = -8; tick <= 8; ++tick)
        {
            const float x = cx + static_cast<float> (tick) * radius / 8;
            segment (construction, { x - 2, dimensionY - 3 }, { x + 2, dimensionY + 3 });
        }
    }
    // Ghostly bundles trail across the sheet as fine, angular polylines. No curves.
    for (int bundle = 0; bundle < 9; ++bundle)
        for (int strand = 0; strand < 14; ++strand)
        {
            const float y = static_cast<float> (bundle) * h / 8 + static_cast<float> (strand) * 3;
            wisps.startNewSubPath (-20, y);
            for (int point = 1; point <= 36; ++point)
            {
                const float x = static_cast<float> (point) * w / 35;
                const float drift = std::sin (static_cast<float> (point) * 0.24f + static_cast<float> (bundle))
                    * (22.0f + static_cast<float> (strand) * 2.0f);
                wisps.lineTo (x, y + drift + static_cast<float> ((point + bundle) % 5) * 4);
            }
        }
}
void TechnicalBackdrop::paint (juce::Graphics& g, juce::Rectangle<int> clip) const
{
    juce::Graphics::ScopedSaveState saved (g);
    g.reduceClipRegion (clip);
    const juce::Colour ink (0xff879da6);
    g.setColour (ink.withAlpha (0.045f));
    g.strokePath (grid, juce::PathStrokeType (0.5f));
    g.setColour (ink.withAlpha (0.075f));
    g.strokePath (majorGrid, juce::PathStrokeType (0.65f));
    g.setColour (ink.withAlpha (0.07f));
    g.strokePath (construction, juce::PathStrokeType (0.5f));
    g.setColour (ink.withAlpha (0.12f));
    g.strokePath (webs, juce::PathStrokeType (0.6f));
    g.setColour (ink.withAlpha (0.08f));
    g.strokePath (wisps, juce::PathStrokeType (0.6f));
}
}
