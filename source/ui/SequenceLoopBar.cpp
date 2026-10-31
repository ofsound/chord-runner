#include "SequenceLoopBar.h"

namespace chordrunner::ui
{
SequenceLoopBar::SequenceLoopBar (EditorController& owner) : controller (owner)
{
    setComponentID ("sequence.loop.range");
    setTooltip ("Drag to draw a loop. Drag either end to resize; Shift-drag the bracket to move it. Snaps to sixteenth notes. LOOP toggles playback without losing the range.");
}
float SequenceLoopBar::xAt (double beat) const
{
    return 24.0f + static_cast<float> (beat / static_cast<double> (controller.state["totalBeats"])) * static_cast<float> (getWidth() - 24);
}
double SequenceLoopBar::beatAt (float x) const
{
    const double total = static_cast<double> (controller.state["totalBeats"]);
    return juce::jlimit (0.0, total, std::round ((x - 24) / juce::jmax (1, getWidth() - 24) * total * 4) / 4);
}
void SequenceLoopBar::finish()
{
    if (mode == 0) return;
    mode = 0;
    controller.processor.endEditGesture();
    repaint();
}
void SequenceLoopBar::paint (juce::Graphics& g)
{
    const double total = static_cast<double> (controller.state["totalBeats"]);
    if (total <= 0) return;
    const double start = static_cast<double> (controller.state["loopStart"]);
    double end = static_cast<double> (controller.state["loopEnd"]);
    if (end <= 0) end = total;
    const bool on = static_cast<bool> (controller.state["loopEnabled"]);
    const auto ink = on ? accentYellow : juce::Colour (0xff81949e);
    const float left = xAt (start), right = xAt (end);
    g.setColour (juce::Colour (0xff121a20)); g.fillRect (getLocalBounds());
    g.setColour (ink.withAlpha (on ? 0.16f : 0.05f));
    g.fillRect (left, 2.0f, right - left, static_cast<float> (getHeight() - 3));
    g.setColour (ink.withAlpha (on ? 0.95f : 0.55f));
    g.drawLine (left, 3, right, 3, 2);
    g.drawLine (left, 3, left, static_cast<float> (getHeight() - 2), 2);
    g.drawLine (right - 1, 3, right - 1, static_cast<float> (getHeight() - 2), 2);
    juce::Path handles;
    handles.addTriangle (left, 3, left + 8, 3, left, 11);
    handles.addTriangle (right, 3, right - 8, 3, right, 11);
    g.fillPath (handles);
    g.setFont (uiFont (10.0f));
    g.drawText (juce::String (start / 4 + 1, 2) + " - " + juce::String (end / 4 + 1, 2),
        static_cast<int> (left) + 12, 3, juce::jmax (0, static_cast<int> (right - left) - 24), getHeight() - 3, juce::Justification::centredLeft);
}
void SequenceLoopBar::mouseDown (const juce::MouseEvent& event)
{
    if (! isEnabled() || ! event.mods.isLeftButtonDown() || controller.processor.selectedPattern() < 0) return;
    finish();
    oldStart = static_cast<double> (controller.state["loopStart"]);
    oldEnd = static_cast<double> (controller.state["loopEnd"]);
    if (oldEnd <= 0) oldEnd = static_cast<double> (controller.state["totalBeats"]);
    originX = event.position.x; anchor = beatAt (originX);
    mode = std::abs (originX - xAt (oldStart)) <= 8 ? 1 : std::abs (originX - xAt (oldEnd)) <= 8 ? 2 : event.mods.isShiftDown() ? 3 : 4;
    pattern = controller.processor.selectedPattern(); revision = static_cast<int> (controller.state["revision"]);
    controller.processor.beginEditGesture();
}
void SequenceLoopBar::mouseDrag (const juce::MouseEvent& event)
{
    if (mode == 0 || std::abs (event.position.x - originX) < 3) return;
    if (pattern != controller.processor.selectedPattern() || revision != static_cast<int> (controller.processor.getTransportState()["revision"])) { finish(); return; }
    const double total = static_cast<double> (controller.state["totalBeats"]), at = beatAt (event.position.x);
    double start = oldStart, end = oldEnd;
    if (mode == 1) start = juce::jlimit (0.0, end - 0.25, at);
    if (mode == 2) end = juce::jlimit (start + 0.25, total, at);
    if (mode == 3)
    {
        start = juce::jlimit (0.0, total - (oldEnd - oldStart), oldStart + at - anchor);
        end = start + oldEnd - oldStart;
    }
    if (mode == 4)
    {
        start = juce::jmin (anchor, at); end = juce::jmax (anchor, at);
        if (end - start < 0.25) return;
    }
    auto next = controller.processor.getTimelineState();
    next.getDynamicObject()->setProperty ("loopStart", start);
    next.getDynamicObject()->setProperty ("loopEnd", end);
    if (mode == 4) next.getDynamicObject()->setProperty ("loopEnabled", true);
    controller.error = controller.processor.editTimeline (next, "sequence.loop").getErrorMessage();
    controller.refresh(); revision = static_cast<int> (controller.state["revision"]);
}
void SequenceLoopBar::mouseUp (const juce::MouseEvent&) { finish(); }
void SequenceLoopBar::mouseMove (const juce::MouseEvent& event)
{
    double end = static_cast<double> (controller.state["loopEnd"]);
    if (end <= 0) end = static_cast<double> (controller.state["totalBeats"]);
    const bool edge = std::abs (event.position.x - xAt (static_cast<double> (controller.state["loopStart"]))) < 8 || std::abs (event.position.x - xAt (end)) < 8;
    setMouseCursor (edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::CrosshairCursor);
}
}
