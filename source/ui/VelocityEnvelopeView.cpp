#include "VelocityEnvelopeView.h"

namespace chordrunner::ui
{
VelocityEnvelopeView::VelocityEnvelopeView()
{
    setComponentID ("arp.envelope.display");
    setTooltip ("Drag START and END vertically for velocity. Drag the lower diamond horizontally for duration (0-1 of the run). Beat mode spans retriggers; Note/Off use the finite phrase or one cycle as a reference. Humanize is the shaded range. Power bypasses the envelope.");
}
void VelocityEnvelopeView::update (const ArpeggiatorSettings& value, int source, bool live, int emitted, double age, double bpm, int notes)
{
    const double period = value.envelopeRunMs (notes, bpm);
    source = value.startVelocity >= 0 ? value.startVelocity : source;
    if (settings == value && start == source && liveInput == live && output == emitted && std::abs (ageMs - age) < 0.01 && std::abs (period - runMs) < 0.01) return;
    settings = value; start = source; liveInput = live; output = emitted; ageMs = age; runMs = period;
    repaint();
}
juce::Rectangle<float> VelocityEnvelopeView::plotBounds() const
{
    return getLocalBounds().toFloat().reduced (20, 0).withTrimmedTop (38).withTrimmedBottom (getHeight() < 150 ? 42 : 60);
}
double VelocityEnvelopeView::ratio() const { return settings.durationRatio >= 0.0 ? settings.durationRatio : juce::jlimit (0.0, 1.0, settings.decay / runMs); }
int VelocityEnvelopeView::endVelocity() const
{
    auto model = settings; model.velocityEnabled = true;
    return model.envelopeVelocity (start, runMs, runMs);
}
juce::Point<float> VelocityEnvelopeView::handlePosition (int index) const
{
    const auto plot = plotBounds();
    const auto yAt = [&] (int velocity) { return plot.getBottom() - static_cast<float> (velocity) / 127 * plot.getHeight(); };
    if (index == 0) return { plot.getX(), yAt (start) };
    if (index == 1) return { plot.getRight(), yAt (endVelocity()) };
    return { plot.getX() + static_cast<float> (ratio()) * plot.getWidth(), plot.getBottom() + 18 };
}
void VelocityEnvelopeView::finishGesture()
{
    if (activeHandle < 0) return;
    activeHandle = -1; dragSource = -1;
    if (onGestureEnd) onGestureEnd();
    repaint();
}
void VelocityEnvelopeView::mouseDown (const juce::MouseEvent& event)
{
    if (! isEnabled() || ! event.mods.isLeftButtonDown()) return;
    int closest = -1;
    float distance = 22;
    for (int i = 0; i < 3; ++i)
    {
        const float next = event.position.getDistanceFrom (handlePosition (i));
        if (next < distance) { distance = next; closest = i; }
    }
    if (closest >= 0)
    {
        activeHandle = closest; dragSource = event.source.getIndex();
        if (onGestureStart) onGestureStart();
        repaint();
    }
}
void VelocityEnvelopeView::mouseDrag (const juce::MouseEvent& event)
{
    if (activeHandle < 0 || event.source.getIndex() != dragSource || ! isEnabled()) return;
    const auto plot = plotBounds();
    const int velocity = juce::jlimit (0, 127, juce::roundToInt ((plot.getBottom() - event.position.y) * 127 / juce::jmax (1.0f, plot.getHeight())));
    int first = start, last = settings.target;
    double duration = ratio();
    if (activeHandle == 0) first = velocity;
    else if (activeHandle == 1) last = velocity;
    else duration = juce::jlimit (0.0, 1.0, static_cast<double> ((event.position.x - plot.getX()) / juce::jmax (1.0f, plot.getWidth())));
    if (onEdit) onEdit (first, last, duration);
}
void VelocityEnvelopeView::mouseUp (const juce::MouseEvent& event) { if (event.source.getIndex() == dragSource) finishGesture(); }
void VelocityEnvelopeView::mouseMove (const juce::MouseEvent& event)
{
    auto cursor = juce::MouseCursor::NormalCursor;
    for (int i = 0; i < 3; ++i) if (event.position.getDistanceFrom (handlePosition (i)) <= 22)
        cursor = i == 2 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor;
    setMouseCursor (cursor);
}
void VelocityEnvelopeView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17242c), bounds.getTopLeft(), juce::Colour (0xff10171c), bounds.getBottomLeft(), false));
    g.fillRoundedRectangle (bounds, 6);
    g.setColour (juce::Colour (0xff43525e).withAlpha (0.5f)); g.drawRoundedRectangle (bounds, 6, 1);
    const auto plot = plotBounds();
    if (plot.getHeight() < 10 || plot.getWidth() < 10) return;
    const auto yAt = [&] (double velocity) { return plot.getBottom() - static_cast<float> (velocity / 127.0) * plot.getHeight(); };
    const auto xAt = [&] (double t) { return plot.getX() + static_cast<float> (t) * plot.getWidth(); };
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    for (int i = 0; i <= 8; ++i) g.drawVerticalLine (juce::roundToInt (xAt (static_cast<double> (i) / 8)), plot.getY(), plot.getBottom());
    for (int v : { 0, 32, 64, 96, 127 }) g.drawHorizontalLine (juce::roundToInt (yAt (v)), plot.getX(), plot.getRight());
    auto model = settings; model.velocityEnabled = true;
    juce::Path curve, band;
    for (int i = 0; i <= 160; ++i)
    {
        const double t = static_cast<double> (i) / 160;
        const int velocity = model.envelopeVelocity (start, runMs * t, runMs);
        if (i == 0) { curve.startNewSubPath (xAt (t), yAt (velocity)); band.startNewSubPath (xAt (t), yAt (model.humanizedVelocity (velocity, 1))); }
        else { curve.lineTo (xAt (t), yAt (velocity)); band.lineTo (xAt (t), yAt (model.humanizedVelocity (velocity, 1))); }
    }
    for (int i = 160; i >= 0; --i)
    {
        const double t = static_cast<double> (i) / 160;
        band.lineTo (xAt (t), yAt (model.humanizedVelocity (model.envelopeVelocity (start, runMs * t, runMs), -1)));
    }
    band.closeSubPath(); g.setColour (accentGreen.withAlpha (0.16f)); g.fillPath (band);
    juce::Path illuminated; juce::PathStrokeType (2.0f).createStrokedPath (illuminated, curve);
    glow.render (g, illuminated);
    g.setColour (settings.velocityEnabled ? accentYellow : juce::Colour (0xff97a6af)); g.strokePath (curve, juce::PathStrokeType (2));
    const auto durationHandle = handlePosition (2);
    const float dash[] { 3, 4 };
    g.setColour (accentYellow.withAlpha (0.3f));
    g.drawDashedLine ({ durationHandle.x, plot.getY(), durationHandle.x, durationHandle.y - 6 }, dash, 2);
    for (int i = 0; i < 3; ++i)
    {
        const auto point = handlePosition (i);
        g.setColour (juce::Colour (0xff10191e));
        g.fillEllipse (point.x - 7, point.y - 7, 14, 14);
        g.setColour (activeHandle == i ? juce::Colours::white : accentYellow);
        if (i < 2) { g.drawEllipse (point.x - 7, point.y - 7, 14, 14, 2); g.fillEllipse (point.x - 2, point.y - 2, 4, 4); }
        else { juce::Path diamond; diamond.addQuadrilateral (point.x, point.y - 8, point.x + 8, point.y, point.x, point.y + 8, point.x - 8, point.y); g.strokePath (diamond, juce::PathStrokeType (2)); }
    }
    if (liveInput && output > 0)
    {
        g.setColour (juce::Colours::white);
        g.fillEllipse (xAt (juce::jlimit (0.0, 1.0, ageMs / runMs)) - 3, yAt (output) - 3, 6, 6);
    }
    g.setColour (juce::Colour (0xffdce3e8)); g.setFont (uiFont (18.0f, juce::Font::bold));
    g.drawText ("START " + juce::String (start), 12, 4, getWidth() / 2, 26, juce::Justification::centredLeft);
    g.drawText ("END " + juce::String (endVelocity()), getWidth() / 2, 4, getWidth() / 2 - 12, 26, juce::Justification::centredRight);
    g.setFont (uiFont (14.0f));
    g.drawText ("0", 12, getHeight() - 23, 50, 20, juce::Justification::centredLeft);
    g.drawText ("DURATION " + juce::String (ratio(), 2) + "  /  " + juce::String (runMs * ratio(), 0) + " ms", 60, getHeight() - 23, getWidth() - 220, 20, juce::Justification::centred);
    g.drawText (juce::String (runMs, 0) + " ms " + (settings.retrigger == 2 ? "RETRIGGER" : "CYCLE REF"), getWidth() - 170, getHeight() - 23, 158, 20, juce::Justification::centredRight);
}
}
