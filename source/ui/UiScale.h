#pragma once

namespace chordrunner::ui
{
struct UiScale
{
    static constexpr int canvasWidth = 1400, canvasHeight = 1050;
    static constexpr int minimumPercent = 50, maximumPercent = 400, defaultPercent = 100;
    static constexpr float baseline = 0.85f;
    static constexpr double aspectRatio = static_cast<double> (canvasWidth) / canvasHeight;
    static constexpr float factor (int percent) { return static_cast<float> (percent) * baseline / 100.0f; }
};
}
