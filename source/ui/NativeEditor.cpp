#include "NativeEditor.h"
#include "UiPalette.h"
#include <cstdlib>

namespace chordrunner::ui
{
namespace
{
const juce::Colour background { 0xff101114 }, surface { 0xff1b1d22 }, border { 0xff393d45 },
    foreground { 0xffe3e5e8 }, muted { 0xffa0a6b0 }, green { accentGreen }, amber { accentYellow };
constexpr std::array<double, 12> rates { 4, 2, 1, 0.5, 0.25, 0.125, 0.0625, 2.0 / 3, 1.0 / 3, 1.0 / 6, 0.75, 0.375 };

juce::String noteName (int note)
{
    return juce::MidiMessage::getMidiNoteName (note, true, true, 4);
}
juce::String joined (const juce::var& array, const juce::String& separator = "  ")
{
    juce::StringArray values;
    if (const auto* items = array.getArray())
        for (const auto& item : *items)
            values.add (item.toString());
    return values.joinIntoString (separator);
}
void labelStyle (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setColour (juce::Label::textColourId, muted);
    label.setFont (uiFont (12.0f));
}
void identify (juce::Component& component, const juce::String& id, const juce::String& name)
{
    component.setComponentID (id);
    component.setName (name);
    component.setTitle (name);
}
}

void ModuleCard::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (0.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff22262c), 0, 0,
        juce::Colour (0xff181c21), 0, static_cast<float> (getHeight()), false));
    g.fillRoundedRectangle (area, 10.0f);
    g.setColour (juce::Colour (0xff363e46));
    g.drawRoundedRectangle (area, 10.0f, 1.0f);
    g.setColour (foreground);
    g.setFont (uiFont (24.0f).withStyle ("Bold"));
    g.drawText (title, 50, 0, 174, headerHeight, juce::Justification::centredLeft);
    if (getHeight() == headerHeight && getWidth() > 650)
    {
        g.setColour (muted);
        g.setFont (uiFont (11.0f));
        g.drawText ("Double-click to expand", getWidth() - 180, 3, 160, 30, juce::Justification::centredRight);
    }
}

void NativeEditor::MidiActivity::paint (juce::Graphics& g)
{
    const bool stacked = getWidth() < 120;
    const float cellWidth = static_cast<float> (getWidth()) / (stacked ? 1.0f : 2.0f);
    const float cellHeight = static_cast<float> (getHeight()) / (stacked ? 2.0f : 1.0f);
    g.setFont (uiFont (10.0f));
    for (int i = 0; i < 2; ++i)
    {
        const float x = stacked ? 0.0f : static_cast<float> (i) * cellWidth;
        const float top = stacked ? static_cast<float> (i) * cellHeight : 0.0f;
        const float y = top + cellHeight * 0.5f;
        g.setColour (green.withAlpha (0.15f + (i == 0 ? input : output) * 0.85f));
        g.fillEllipse (x + 2, y - 3, 6, 6);
        g.setColour (muted);
        g.drawText (i == 0 ? "MIDI IN" : "MIDI OUT",
            juce::Rectangle<float> (x + 12, top, cellWidth - 12, cellHeight), juce::Justification::centredLeft);
    }
}

void NativeEditor::MidiActivity::update (double inputTime, double outputTime)
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const auto level = [now] (double time)
    {
        return static_cast<float> (juce::jlimit (0.0, 1.0, 1.0 - (now - time) / 180.0));
    };
    input = level (inputTime);
    output = level (outputTime);
    repaint();
}

void NativeEditor::TipsButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    getLookAndFeel().drawButtonBackground (g, *this, findColour (buttonColourId), over, down);
    g.setColour (getToggleState() ? green : muted);
    juce::Path glyph;
    glyph.startNewSubPath (6.5f, 8.5f);
    glyph.cubicTo (6.5f, 2.5f, 17.5f, 2.5f, 17.5f, 8.5f);
    glyph.cubicTo (17.5f, 12.5f, 12, 11.5f, 12, 15.5f);
    const auto glyphOffset = getLocalBounds().getCentre().toFloat() - juce::Point<float> (12, 12);
    glyph.applyTransform (juce::AffineTransform::translation (glyphOffset.x, glyphOffset.y));
    g.strokePath (glyph, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.fillEllipse (glyphOffset.x + 10.8f, glyphOffset.y + 18, 2.4f, 2.4f);
}

void NativeEditor::DictionaryButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    getLookAndFeel().drawButtonBackground (g, *this, findColour (buttonColourId), over, down);
    const auto book = getLocalBounds().toFloat().withSizeKeepingCentre (19.0f, 23.0f);
    g.setColour (muted);
    g.drawRoundedRectangle (book, 2.0f, 1.5f);
    g.drawVerticalLine (static_cast<int> (book.getX() + 4), book.getY(), book.getBottom());
    g.drawHorizontalLine (static_cast<int> (book.getBottom() - 4), book.getX() + 4, book.getRight());
    g.setFont (uiFont (12.0f, juce::Font::bold));
    g.drawText ("A", book.withTrimmedLeft (5).withTrimmedBottom (4), juce::Justification::centred);
}

void NativeEditor::IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    getLookAndFeel().drawButtonBackground (g, *this, findColour (buttonColourId), over, down);
    g.setColour ((getToggleState() ? green : foreground).withMultipliedAlpha (isEnabled() ? 1.0f : 0.35f));
    const auto centre = getLocalBounds().getCentre().toFloat();
    if (getComponentID() == "play")
    {
        if (getButtonText() == "Stop")
            g.fillRoundedRectangle (centre.x - 5, centre.y - 5, 10, 10, 1);
        else
        {
            juce::Path triangle;
            triangle.addTriangle (centre.x - 4, centre.y - 6, centre.x - 4, centre.y + 6, centre.x + 6, centre.y);
            g.fillPath (triangle);
        }
        return;
    }
    juce::Path arrow;
    arrow.startNewSubPath (5, -4);
    arrow.cubicTo (-5, -9, -8, 2, -3, 5);
    arrow.cubicTo (0, 7, 5, 5, 5, 2);
    arrow.startNewSubPath (5, -4);
    arrow.lineTo (0, -5);
    arrow.startNewSubPath (5, -4);
    arrow.lineTo (4, -9);
    const float direction = getComponentID() == "undo" ? -1.0f : 1.0f;
    arrow.applyTransform (juce::AffineTransform::scale (direction, 1).translated (centre.x, centre.y));
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ModulePower::paintButton (juce::Graphics& g, bool over, bool down)
{
    const float diameter = juce::jmin (faceDiameter, static_cast<float> (juce::jmin (getWidth(), getHeight()) - 10));
    auto face = getLocalBounds().toFloat().withSizeKeepingCentre (diameter, diameter);
    if (down) face = face.reduced (0.7f);
    juce::Path silhouette;
    silhouette.addEllipse (face);
    const bool on = getToggleState();
    if (on && isEnabled()) glow.render (g, silhouette);
    g.setGradientFill (juce::ColourGradient (juce::Colour (over ? 0xff43515b : 0xff354149), face.getTopLeft(),
        juce::Colour (0xff0e1419), face.getBottomLeft(), false));
    g.fillEllipse (face);
    recess.render (g, silhouette);
    g.setColour ((on ? green.withAlpha (hasKeyboardFocus (true) ? 1.0f : 0.8f)
                       : juce::Colour (hasKeyboardFocus (true) ? 0xffa0a6b0 : 0xff52616b))
                     .withMultipliedAlpha (isEnabled() ? 1.0f : 0.4f));
    g.drawEllipse (face.reduced (0.5f), 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawEllipse (face.reduced (1.5f), 0.6f);
    const auto centre = face.getCentre();
    const float radius = diameter * 0.23f;
    juce::Path glyph;
    glyph.addCentredArc (centre.x, centre.y + 0.8f, radius, radius, 0, 0.65f,
        juce::MathConstants<float>::twoPi - 0.65f, true);
    glyph.startNewSubPath (centre.x, centre.y - radius - 1.6f);
    glyph.lineTo (centre.x, centre.y - 0.5f);
    g.setColour ((on ? green.brighter (0.15f) : juce::Colour (0xff8b99a3)).withMultipliedAlpha (isEnabled() ? 1.0f : 0.4f));
    g.strokePath (glyph, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font PerformanceLook::getTextButtonFont (juce::TextButton&, int height)
{
    return uiFont (juce::jmin (15.0f, static_cast<float> (height) * 0.6f));
}
juce::Font PerformanceLook::getPopupMenuFont() { return uiFont (14.0f); }

void PerformanceLook::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool over, bool down)
{
    if (! button.getComponentID().startsWith ("rhythm."))
    {
        juce::LookAndFeel_V4::drawToggleButton (g, button, over, down);
        return;
    }
    const float alpha = button.isEnabled() ? 1.0f : 0.4f;
    drawTickBox (g, button, 4.0f, (button.getHeight() - 18.0f) * 0.5f, 18.0f, 18.0f, button.getToggleState(), button.isEnabled(), over, down);
    g.setColour (accentGreen.withAlpha (alpha));
    g.setFont (uiFont (sectionHeadingSize, juce::Font::bold));
    g.drawText (button.getButtonText(), 28, 0, button.getWidth() - 32, button.getHeight(), juce::Justification::centredLeft);
}

void PerformanceLook::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& colour, bool over, bool down)
{
    const auto area = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool power = button.getComponentID() == "sequencerEnabled" || button.getComponentID() == "arp.enabled";
    const auto fill = button.getToggleState() ? juce::Colour (0xff27483d) : colour;
    g.setColour (fill.brighter (down ? 0.14f : over ? 0.07f : 0.0f).withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.4f));
    g.fillRoundedRectangle (area, power ? 15.0f : 5.0f);
    g.setColour (button.getToggleState() ? green.withAlpha (0.65f) : border);
    g.drawRoundedRectangle (area, power ? 15.0f : 5.0f, 1.0f);
}

void PerformanceLook::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
    float position, float, float, juce::Slider::SliderStyle, juce::Slider& slider)
{
    if (slider.getComponentID() == "arp.interval" || slider.getComponentID() == "tempo" || slider.getComponentID() == "velocity") return;
    const float alpha = slider.isEnabled() ? 1.0f : 0.3f;
    const float left = static_cast<float> (x), right = static_cast<float> (x + width);
    const float middle = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
    g.setColour (juce::Colour (0xff354047).withAlpha (alpha));
    g.fillRoundedRectangle (left, middle - 2, static_cast<float> (width), 4, 2);
    g.setColour (amber.withAlpha (alpha * 0.75f));
    g.fillRoundedRectangle (left, middle - 2, juce::jmax (0.0f, position - left), 4, 2);
    if (slider.getMinimum() < 0)
    {
        const float zero = left + static_cast<float> (slider.valueToProportionOfLength (0)) * static_cast<float> (width);
        g.setColour (foreground.withAlpha (0.35f * alpha)); g.fillRect (zero, middle - 5, 1.0f, 10.0f);
    }
    const float thumb = juce::jlimit (left, right, position);
    g.setColour (amber.withAlpha (alpha)); g.fillRoundedRectangle (thumb - 2, middle - 6, 4, 12, 1.5f);
}
void PerformanceLook::drawComboBox (juce::Graphics& g, int width, int height, bool down, int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0, 0, static_cast<float> (width), static_cast<float> (height)).reduced (0.5f);
    const float alpha = box.isEnabled() ? 1.0f : 0.3f;
    g.setColour (juce::Colour (down || box.isMouseOver() ? 0xff2a383e : 0xff202b31).withAlpha (alpha));
    g.fillRoundedRectangle (bounds, 5);
    g.setColour (juce::Colour (0xff3b4c53).withAlpha (alpha));
    g.drawLine (5, bounds.getBottom(), bounds.getRight() - 5, bounds.getBottom(), 1);
    juce::Path arrow;
    const float cx = static_cast<float> (width) - 14, cy = static_cast<float> (height) * 0.5f;
    arrow.startNewSubPath (cx - 3, cy - 1); arrow.lineTo (cx, cy + 2); arrow.lineTo (cx + 3, cy - 1);
    g.setColour (green.withAlpha (alpha)); g.strokePath (arrow, juce::PathStrokeType (1.4f));
}
juce::Font PerformanceLook::getComboBoxFont (juce::ComboBox&) { return uiFont (13.0f); }
void PerformanceLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 0, box.getWidth() - 28, box.getHeight());
    label.setFont (getComboBoxFont (box));
}
void PerformanceLook::drawGroupComponentOutline (juce::Graphics& g, int w, int h, const juce::String& text,
    const juce::Justification&, juce::GroupComponent&)
{
    const auto area = juce::Rectangle<float> (0, 0, static_cast<float> (w), static_cast<float> (h));
    g.setColour (juce::Colour (0xff151e23)); g.fillRoundedRectangle (area, 6);
    g.setColour (green); g.setFont (uiFont (sectionHeadingSize, juce::Font::bold));
    g.drawFittedText (text, 10, 7, w - 20, 24, juce::Justification::centredLeft, 1, 0.85f);
}
void PerformanceLook::drawLabel (juce::Graphics& g, juce::Label& label)
{
    if (dynamic_cast<juce::Slider*> (label.getParentComponent()) == nullptr)
    {
        juce::LookAndFeel_V4::drawLabel (g, label);
        return;
    }
    const auto area = label.getLocalBounds().toFloat().reduced (0.5f);
    const float alpha = label.isEnabled() ? 1.0f : 0.3f;
    g.setColour (juce::Colour (0xff253139).withAlpha (alpha)); g.fillRoundedRectangle (area, 4);
    if (! label.isBeingEdited())
    {
        g.setColour (foreground.withAlpha (alpha)); g.setFont (uiFont (12.0f));
        g.drawText (label.getText(), label.getLocalBounds().reduced (3, 0), juce::Justification::centred);
    }
}

juce::Slider::SliderLayout PerformanceLook::getSliderLayout (juce::Slider& slider)
{
    if (slider.getComponentID() == "arp.interval" || slider.getComponentID() == "tempo" || slider.getComponentID() == "velocity")
    {
        juce::Slider::SliderLayout layout;
        layout.textBoxBounds = slider.getLocalBounds();
        layout.sliderBounds = slider.getLocalBounds();
        return layout;
    }
    auto layout = juce::LookAndFeel_V4::getSliderLayout (slider);
    if (slider.getTextBoxPosition() == juce::Slider::TextBoxRight)
    {
        const juce::Font font (uiFont (12.0f));
        const float textWidth = juce::jmax (juce::GlyphArrangement::getStringWidth (font, slider.getTextFromValue (slider.getMinimum())),
            juce::GlyphArrangement::getStringWidth (font, slider.getTextFromValue (slider.getMaximum())));
        const int width = juce::jmin (juce::jmax (28, slider.getWidth() - 24), juce::jmax (28, static_cast<int> (std::ceil (textWidth)) + 12));
        layout.textBoxBounds.setX (slider.getWidth() - width);
        layout.textBoxBounds.setWidth (width);
        layout.sliderBounds.setRight (layout.textBoxBounds.getX() - 6);
    }
    return layout;
}

bool PatternPad::keyPressed (const juce::KeyPress& key)
{
    if ((key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) && onReset)
    {
        onReset();
        return true;
    }
    return juce::TextButton::keyPressed (key);
}

void PatternPad::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto area = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour ((active || copyArmed) ? juce::Colour (0xff284f43) : selected ? juce::Colour (0xff303c47) : surface);
    g.fillRoundedRectangle (area, 6.0f);
    if (over || down)
    {
        g.setColour (foreground.withAlpha (down ? 0.12f : 0.05f));
        g.fillRoundedRectangle (area, 6.0f);
    }
    g.setColour (copyArmed ? green : queued ? amber : active ? green : selected ? juce::Colour (0xff8caccc) : border);
    g.drawRoundedRectangle (area, 6.0f, queued || selected ? 1.5f : 1.0f);
    g.setFont (uiFont (14.0f).withStyle ("Bold"));
    g.setColour ((active || copyArmed) ? green : foreground);
    g.drawText (juce::String (slot + 1), getLocalBounds().reduced (4), juce::Justification::centred);
    if (queued)
    {
        g.setColour (amber);
        g.fillEllipse (static_cast<float> (getWidth() - 9), 5.0f, 4.0f, 4.0f);
    }
}

TimelineView::TimelineView (EditorController& c) : controller (c)
{
    identify (*this, "timeline", "Chord timeline");
    setViewportIgnoreDragFlag (true);
    setWantsKeyboardFocus (true);
    setTitle ("Editable piano roll");
    setTooltip ("Drag notes to move. Drag edges to resize. Shift-click or drag empty space to select. Click a chord heading to select its tones. Snap: sixteenth notes; Alt bypasses snap. Double-click the bar count to enter a new loop length.");
    identify (lengthReadout, "timeline.length", "Loop length in bars");
    labelStyle (lengthReadout, "");
    lengthReadout.setFont (uiFont (10.0f));
    lengthReadout.setJustificationType (juce::Justification::centredRight);
    lengthReadout.setEditable (false, true);
    lengthReadout.setTooltip ("Double-click to enter the loop length in bars.");
    addAndMakeVisible (lengthReadout);
    lengthReadout.onEditorShow = [this]
    {
        lengthEditRevision = static_cast<int> (controller.state["revision"]);
        if (auto* input = lengthReadout.getCurrentTextEditor())
        {
            input->setInputRestrictions (16, "0123456789.-");
            input->setText (juce::String (static_cast<double> (controller.state["totalBeats"]) / 4.0, 2), false);
            input->selectAll();
        }
    };
    lengthReadout.onTextChange = [this]
    {
        const auto text = lengthReadout.getText().trim().toStdString();
        char* end = nullptr;
        const double bars = std::strtod (text.c_str(), &end);
        const auto result = ! text.empty() && end == text.c_str() + text.size() && std::isfinite (bars)
            ? controller.processor.resizeLoop (bars * 4.0, lengthEditRevision)
            : juce::Result::fail ("Enter a valid loop length in bars.");
        controller.error = result.getErrorMessage();
        controller.refresh();
        lengthReadout.setText (juce::String (static_cast<double> (controller.state["totalBeats"]) / 4.0, 2) + " bars", juce::dontSendNotification);
    };
    refreshLength();
}
void TimelineView::refreshLength()
{
    if (! lengthReadout.isBeingEdited())
        lengthReadout.setText (juce::String (static_cast<double> (controller.state["totalBeats"]) / 4.0, 2) + " bars", juce::dontSendNotification);
}
TimelineView::~TimelineView() { finishDrag(); }
void TimelineView::finishDrag()
{
    if (dragMode > 0 && dragMode < 4)
        controller.processor.endEditGesture();
    dragMode = 0;
    marquee = {};
    dragTone = -1; dragSource = -1;
    repaint();
}
void TimelineView::paint (juce::Graphics& g)
{
    g.fillAll (surface);
    const auto* chords = controller.state["chords"].getArray();
    if (chords == nullptr || chords->isEmpty())
        return;
    selected = juce::jlimit (0, chords->size() - 1, selected);
    const auto total = static_cast<double> (controller.state["totalBeats"]);
    if (total <= 0)
        return;
    int lowest = 127, highest = 0;
    for (const auto& chord : *chords)
        if (const auto* notes = chord["notes"].getArray())
            for (const auto& note : *notes)
            {
                lowest = juce::jmin (lowest, static_cast<int> (note));
                highest = juce::jmax (highest, static_cast<int> (note));
            }
    if (previewRevision != controller.refreshSerial)
    {
        previewRevision = controller.refreshSerial;
        outputPreview = controller.processor.getOutputPreview();
    }
    for (const auto& note : outputPreview)
    {
        lowest = juce::jmin (lowest, note.note);
        highest = juce::jmax (highest, note.note);
    }
    lowest = juce::jmax (0, lowest - 2);
    highest = juce::jmin (127, highest + 2);
    if (dragTone >= 0) { lowest = dragLow; highest = dragHigh; }
    gridLow = lowest; gridHigh = highest;
    constexpr float keyWidth = 46.0f, top = 54.0f;
    const auto width = static_cast<float> (getWidth()) - keyWidth;
    const auto rowHeight = (static_cast<float> (getHeight()) - top) / static_cast<float> (highest - lowest + 1);
    const auto xAt = [total, width] (double beat) { return keyWidth + static_cast<float> (beat / total) * width; };
    g.setFont (uiFont (10.0f));
    for (int note = highest; note >= lowest; --note)
    {
        const auto y = top + static_cast<float> (highest - note) * rowHeight;
        const bool black = juce::MidiMessage::isMidiNoteBlack (note);
        g.setColour (black ? juce::Colour (0xff131519) : juce::Colour (0xff1d2026));
        g.fillRect (0.0f, y, static_cast<float> (getWidth()), rowHeight);
        g.setColour (muted);
        if (rowHeight >= 8.0f)
            g.drawText (noteName (note), 1, juce::roundToInt (y), 39, juce::roundToInt (rowHeight), juce::Justification::centredRight);
    }
    const double pixelsPerBeat = width / total;
    const double division = pixelsPerBeat >= 32.0 ? 0.25 : 1.0;
    if (pixelsPerBeat >= 10.0)
    {
        const auto clip = g.getClipBounds();
        const int first = juce::jmax (0, static_cast<int> ((clip.getX() - keyWidth) / pixelsPerBeat / division));
        const int last = juce::jmin (static_cast<int> (std::ceil (total / division)), first + static_cast<int> (clip.getWidth() / pixelsPerBeat / division) + 2);
        for (int tick = first; tick < last; ++tick)
        {
            const double beat = tick * division;
            const bool wholeBeat = std::abs (beat - std::round (beat)) < 1.0e-9;
            g.setColour (border.withAlpha (wholeBeat ? 0.5f : 0.18f));
            g.drawVerticalLine (juce::roundToInt (xAt (beat)), top, static_cast<float> (getHeight()));
        }
    }
    const auto active = static_cast<int> (controller.transport["activeChord"]);
    const bool sequenceOn = static_cast<bool> (controller.state["sequencerEnabled"]);
    for (int i = 0; i < chords->size(); ++i)
    {
        const auto& chord = chords->getReference (i);
        const auto x = xAt (static_cast<double> (chord["startBeat"]));
        const auto chordWidth = xAt (static_cast<double> (chord["startBeat"]) + static_cast<double> (chord["beats"])) - x;
        const bool sounding = sequenceOn && i == active
            && controller.transport["selectedPattern"] == controller.transport["activePattern"];
        g.setColour (sounding ? juce::Colour (0xff234e3d) : i == selected ? juce::Colour (0xff363a42) : surface);
        g.fillRect (x, 24.0f, chordWidth - 1.0f, 30.0f);
        g.setColour (sounding ? green : foreground);
        g.setFont (uiFont (14.0f));
        g.drawText (chord["label"].toString() + (static_cast<bool> (chord["edited"]) ? " *" : ""), juce::Rectangle<float> (x + 6, 23, chordWidth - 10, 18), juce::Justification::centredLeft);
        g.setFont (uiFont (10.0f));
        g.setColour (muted);
        g.drawText (juce::String (static_cast<double> (chord["beats"]) / 4.0, 2) + " bars",
                    juce::Rectangle<float> (x + 6, 40, chordWidth - 10, 14), juce::Justification::centredLeft);
        if (const auto* notes = chord["notes"].getArray())
            for (int tone = 0; tone < notes->size(); ++tone)
            {
                const auto& note = (*notes)[tone];
                const auto rect = noteBounds (i, tone);
                g.setColour (outputPreview.empty() ? (sounding ? green : juce::Colour (0xff426f5b)) : juce::Colour (0xff426f5b).withAlpha (dragTone >= 0 ? 0.7f : 0.35f));
                g.fillRoundedRectangle (rect, 2.0f);
                if ((i == hoverChord && static_cast<int> (note) == hoverPitch)
                    || (dragTone >= 0 && i == selected && static_cast<int> (note) == draggedPitch))
                {
                    g.setColour (foreground);
                    g.drawRoundedRectangle (rect, 2.0f, 1.0f);
                }
                if (rowHeight > 12 && chordWidth > 34)
                {
                    g.setColour (sounding ? background : juce::Colour (0xffcae4d7));
                    g.drawText (noteName (static_cast<int> (note)), rect.reduced (4, 0), juce::Justification::centredLeft);
                }
            }
    }
    // Draw only visible bars, even for the maximum 1024-bar progression.
    const auto clip = g.getClipBounds();
    const int firstBar = juce::jmax (0, static_cast<int> ((static_cast<float> (clip.getX()) - keyWidth) / width * static_cast<float> (total) / 4));
    const int lastBar = juce::jmin (static_cast<int> (std::ceil (total / 4)), firstBar + static_cast<int> (static_cast<float> (clip.getWidth()) / width * static_cast<float> (total) / 4) + 2);
    for (int bar = firstBar; bar < lastBar; ++bar)
    {
        const auto x = xAt (bar * 4.0);
        g.setColour (border);
        g.drawVerticalLine (juce::roundToInt (x), top, static_cast<float> (getHeight()));
        g.setColour (muted);
        g.drawText (juce::String (bar + 1), juce::roundToInt (x) + 5, 2, 40, 20, juce::Justification::centredLeft);
    }
    // Output phrase sits above the dim source-chord bands, using note gates
    // and velocities emitted by the same native sequence/arp engines.
    for (const auto& note : outputPreview)
    {
        const float x = xAt (note.start);
        const float y = top + static_cast<float> (highest - note.note) * rowHeight;
        const float noteWidth = juce::jmax (1.5f, xAt (note.end) - x - 0.75f);
        g.setColour (amber.withAlpha (0.5f + 0.5f * static_cast<float> (note.velocity) / 127.0f));
        g.fillRoundedRectangle ({ x, y + 0.5f, noteWidth, juce::jmax (1.5f, rowHeight - 1.0f) }, 1.0f);
    }
    if (! outputPreview.empty())
    {
        g.setColour (amber);
        g.setFont (uiFont (10.0f));
        g.drawText (total > 64.0 ? "ARP / first 16 bars" : "ARP / output preview", getWidth() - 275, 2, 155, 18, juce::Justification::centredRight);
    }
    g.setColour (border);
    g.drawVerticalLine (46, 0, static_cast<float> (getHeight()));
    if (sequenceOn && static_cast<bool> (controller.transport["playing"]))
    {
        g.setColour (green);
        g.fillRect (xAt (static_cast<double> (controller.transport["beat"])), 24.0f, 2.0f, static_cast<float> (getHeight()) - 24);
    }
    if (dragTone >= 0 || hoverPitch >= 0)
    {
        const int displayPitch = dragTone >= 0 ? draggedPitch : hoverPitch;
        g.setColour (foreground);
        g.setFont (uiFont (12.0f));
        g.drawText (noteName (displayPitch) + "  /  " + juce::String (displayPitch), getWidth() - 140, 24, 130, 25, juce::Justification::centredRight);
    }
    if (selectionRevision == static_cast<int> (controller.state["revision"]))
        for (const int id : selectedNotes)
        {
            const auto* notes = controller.state["chords"][id / maxNotes]["notes"].getArray();
            if (notes == nullptr || id % maxNotes >= notes->size())
                continue;
            const auto rect = noteBounds (id / maxNotes, id % maxNotes);
            g.setColour (green.withAlpha (0.16f));
            g.fillRoundedRectangle (rect, 2.0f);
            g.setColour (green);
            g.drawRoundedRectangle (rect, 2.0f, 1.5f);
            if (rect.getWidth() > 16 && rect.getHeight() > 6)
            {
                g.fillRect (rect.getX() + 3, rect.getCentreY() - 2, 1.5f, 4.0f);
                g.fillRect (rect.getRight() - 4, rect.getCentreY() - 2, 1.5f, 4.0f);
            }
        }
    if (!marquee.isEmpty())
    {
        g.setColour (green.withAlpha (0.12f));
        g.fillRect (marquee);
        g.setColour (green);
        g.drawRect (marquee, 1.0f);
    }

}
juce::Rectangle<float> TimelineView::noteBounds (int c, int n) const
{
    const auto chord = controller.state["chords"][c];
    const double total = static_cast<double> (controller.state["totalBeats"]);
    const float scale = static_cast<float> ((getWidth() - 46) / total);
    const float row = (getHeight() - 54.0f) / static_cast<float> (gridHigh - gridLow + 1);
    return { 46.0f + static_cast<float> (static_cast<double> (chord["noteStarts"][n])) * scale,
        54.0f + static_cast<float> (gridHigh - static_cast<int> (chord["notes"][n])) * row + 1.0f,
        juce::jmax (2.0f, static_cast<float> (static_cast<double> (chord["noteDurations"][n])) * scale - 2.0f),
        juce::jmax (2.0f, row - 2.0f) };
}
int TimelineView::hitNote (juce::Point<float> point) const
{
    const auto* chords = controller.state["chords"].getArray();
    if (chords == nullptr || point.y < 54.0f)
        return -1;
    for (int c = chords->size() - 1; c >= 0; --c)
        if (const auto* notes = (*chords)[c]["notes"].getArray())
            for (int n = notes->size() - 1; n >= 0; --n)
                if (noteBounds (c, n).expanded (0, 2).contains (point))
                    return c * maxNotes + n;
    return -1;
}
void TimelineView::mouseDown (const juce::MouseEvent& event)
{
    if (!event.mods.isLeftButtonDown())
        return;
    finishDrag();
    if (controller.processor.selectedPattern() < 0 || event.x < 46 || getWidth() <= 46)
        return;
    if (selectionRevision != static_cast<int> (controller.state["revision"]))
        selectedNotes.clear();
    dragOrigin = event.position;
    dragTotal = static_cast<double> (controller.state["totalBeats"]);
    dragPixels = getWidth() - 46;
    dragRevision = static_cast<int> (controller.state["revision"]);
    selectionRevision = dragRevision;
    dragSource = event.source.getIndex();
    dragChanged = false;
    dragLow = gridLow;
    dragHigh = gridHigh;
    if (isShowing())
        grabKeyboardFocus();
    if (event.y < 24 && event.x >= getWidth() - 110)
        return;
    if (event.y < 54)
    {
        const double beat = (event.x - 46) / dragPixels * dragTotal;
        if (const auto* chords = controller.state["chords"].getArray())
            for (int c = 0; c < chords->size(); ++c)
                if (beat >= static_cast<double> ((*chords)[c]["startBeat"]) && beat < static_cast<double> ((*chords)[c]["startBeat"]) + static_cast<double> ((*chords)[c]["beats"]))
                {
                    selected = c;
                    if (!event.mods.isShiftDown() && !event.mods.isCommandDown())
                        selectedNotes.clear();
                    const auto* notes = (*chords)[c]["notes"].getArray();
                    if (notes != nullptr)
                        for (int n = 0; n < notes->size(); ++n)
                            if (std::find (selectedNotes.begin(), selectedNotes.end(), c * maxNotes + n) == selectedNotes.end())
                                selectedNotes.push_back (c * maxNotes + n);
                    break;
                }
    }
    else
    {
        const int id = hitNote (event.position);
        const bool additive = event.mods.isShiftDown() || event.mods.isCommandDown();
        if (id >= 0)
        {
            auto found = std::find (selectedNotes.begin(), selectedNotes.end(), id);
            if (additive && found != selectedNotes.end())
            {
                selectedNotes.erase (found);
                repaint();
                return;
            }
            if (found == selectedNotes.end())
            {
                if (!additive)
                    selectedNotes.clear();
                selectedNotes.push_back (id);
            }
            selected = id / maxNotes;
            dragTone = id % maxNotes;
            const auto rect = noteBounds (selected, dragTone);
            const float edge = juce::jmin (7.0f, rect.getWidth() * 0.2f);
            dragMode = event.x >= rect.getRight() - edge ? 3 : event.x <= rect.getX() + edge ? 2
                                                                                             : 1;
            dragPitch = static_cast<int> (controller.state["chords"][selected]["notes"][dragTone]);
            draggedPitch = dragPitch;
            dragStep = juce::jmax (8.0f, (getHeight() - 54.0f) / static_cast<float> (gridHigh - gridLow + 1));
            dragNotes.clear();
            for (const int note : selectedNotes)
            {
                const auto chord = controller.state["chords"][note / maxNotes];
                const int n = note % maxNotes;
                dragNotes.push_back ({ note / maxNotes, n, static_cast<int> (chord["notes"][n]), static_cast<double> (chord["noteStarts"][n]), static_cast<double> (chord["noteDurations"][n]) });
            }
        }
        else
        {
            if (!additive)
                selectedNotes.clear();
            marqueeBase = selectedNotes;
            dragMode = 5;
        }
    }
    if (dragMode > 0 && dragMode < 4)
        controller.processor.beginEditGesture();
    repaint();
}
void TimelineView::mouseMove (const juce::MouseEvent& event)
{
    const int id = hitNote (event.position);
    hoverChord = id < 0 ? -1 : id / maxNotes;
    hoverPitch = id < 0 ? -1 : static_cast<int> (controller.state["chords"][hoverChord]["notes"][id % maxNotes]);
    auto cursor = juce::MouseCursor::NormalCursor;
    if (id >= 0)
    {
        const auto rect = noteBounds (id / maxNotes, id % maxNotes);
        const float edge = juce::jmin (7.0f, rect.getWidth() * 0.2f);
        cursor = event.x < rect.getX() + edge || event.x > rect.getRight() - edge
                     ? juce::MouseCursor::LeftRightResizeCursor
                     : juce::MouseCursor::DraggingHandCursor;
    }
    setMouseCursor (cursor);
    repaint();
}
void TimelineView::mouseExit (const juce::MouseEvent&)
{
    hoverChord = hoverPitch = -1;
    setMouseCursor (juce::MouseCursor::NormalCursor); repaint();
}
void TimelineView::mouseDrag (const juce::MouseEvent& event)
{
    if (dragMode == 0 || event.source.getIndex() != dragSource)
        return;
    if (static_cast<int> (controller.processor.getTransportState()["revision"]) != dragRevision)
    {
        finishDrag();
        selectedNotes.clear();
        return;
    }
    if (dragMode == 5)
    {
        marquee = juce::Rectangle<float> (dragOrigin, event.position);
        selectedNotes = marqueeBase;
        if (const auto* chords = controller.state["chords"].getArray())
            for (int c = 0; c < chords->size(); ++c)
                if (const auto* notes = (*chords)[c]["notes"].getArray())
                    for (int n = 0; n < notes->size(); ++n)
                        if (marquee.intersects (noteBounds (c, n)) && std::find (selectedNotes.begin(), selectedNotes.end(), c * maxNotes + n) == selectedNotes.end())
                            selectedNotes.push_back (c * maxNotes + n);
        repaint();
        return;
    }
    const double snap = event.mods.isAltDown() ? 0.0 : 0.25;
    double delta = (event.position.x - dragOrigin.x) / dragPixels * dragTotal;
    if (snap > 0)
        delta = std::round (delta / snap) * snap;
    juce::Result result = juce::Result::ok();
    {
        auto edits = dragNotes;
        int pitchDelta = juce::roundToInt ((dragOrigin.y - event.position.y) / (dragStep * (event.mods.isShiftDown() ? 2.0f : 1.0f)));
        double low = -4096.0, high = 4096.0;
        int pitchLow = -127, pitchHigh = 127;
        for (const auto& note : edits)
        {
            pitchLow = juce::jmax (pitchLow, -note.pitch);
            pitchHigh = juce::jmin (pitchHigh, 127 - note.pitch);
            low = juce::jmax (low, dragMode == 3 ? 0.0625 - note.duration : -note.start);
            high = juce::jmin (high, dragMode == 2 ? note.duration - 0.0625 : dragTotal - note.start - note.duration);
        }
        delta = juce::jlimit (low, high, delta);
        pitchDelta = juce::jlimit (pitchLow, pitchHigh, pitchDelta);
        for (auto& note : edits)
        {
            if (dragMode == 1)
            {
                note.start += delta;
                note.pitch += pitchDelta;
            }
            else if (dragMode == 2)
            {
                note.start += delta;
                note.duration -= delta;
            }
            else
                note.duration += delta;
        }
        result = controller.processor.editNotes (edits, dragRevision);
        if (result.wasOk())
            draggedPitch = dragPitch + (dragMode == 1 ? pitchDelta : 0);
    }
    controller.error = result.getErrorMessage();
    if (result.wasOk())
    {
        const int next = static_cast<int> (controller.processor.getTransportState()["revision"]);
        dragChanged = dragChanged || next != dragRevision;
        dragRevision = selectionRevision = next;
    }
    controller.refresh();
    repaint();
}
void TimelineView::mouseUp (const juce::MouseEvent& event)
{
    if (event.source.getIndex() == dragSource) finishDrag();
}
bool TimelineView::keyPressed (const juce::KeyPress& key)
{
    if (dragMode != 0 && key == juce::KeyPress::escapeKey)
    {
        const bool revert = dragChanged && static_cast<int> (controller.processor.getTransportState()["revision"]) == dragRevision;
        finishDrag();
        if (revert)
            controller.undo();
        return true;
    }
    if (dragMode != 0)
        return false;
    if (key.getModifiers().isCommandDown() && key.getTextCharacter() == 'a')
    {
        selectedNotes.clear();
        if (const auto* chords = controller.state["chords"].getArray())
            for (int c = 0; c < chords->size(); ++c)
                if (const auto* notes = (*chords)[c]["notes"].getArray())
                    for (int n = 0; n < notes->size(); ++n)
                        selectedNotes.push_back (c * maxNotes + n);
        selectionRevision = static_cast<int> (controller.state["revision"]);
        repaint();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        selectedNotes.clear();
        repaint();
        return true;
    }
    if (key != juce::KeyPress::leftKey && key != juce::KeyPress::rightKey)
        return false;
    const auto* chords = controller.state["chords"].getArray();
    if (chords != nullptr)
        selected = juce::jlimit (0, chords->size() - 1, selected + (key == juce::KeyPress::rightKey ? 1 : -1));
    repaint();
    return true;
}

ChordGlossary::ChordGlossary (std::function<void (juce::String)> callback) : onInsert (std::move (callback))
{
    identify (*this, "glossary", "Chord glossary");
    for (auto* component : std::initializer_list<juce::Component*> { &title, &hint, &count, &details, &search, &family, &root, &list, &insert, &close })
        addAndMakeVisible (component);
    labelStyle (title, "Chord glossary");
    title.setFont (uiFont (22.0f));
    labelStyle (hint, "One chord = one bar. C9 is a ninth chord; C9@2 or C9x2 lasts two bars. Use spaces, - or | between chords.\n"
                      "Formulas are degrees above the root. 9 / 11 / 13 sit above the octave. C4 = MIDI 60.\n"
                      "m7 is minor; M7 is major. C6add9 / C69 = six-nine. Slash bass chords are not supported.");
    labelStyle (count, "");
    labelStyle (details, "");
    details.setJustificationType (juce::Justification::topLeft);
    identify (search, "glossary.search", "Search chords");
    search.setTextToShowWhenEmpty ("Search suffix, name, alias or formula", muted);
    search.onTextChange = [this] { filter(); };
    identify (family, "glossary.family", "Chord family");
    families.add ("All chords");
    if (const auto* vocabulary = chordVocabulary().getArray())
        for (const auto& chord : *vocabulary)
            families.addIfNotAlreadyThere (chord["family"].toString());
    family.addItemList (families, 1);
    family.setSelectedId (1, juce::dontSendNotification);
    family.onChange = [this] { filter(); };
    identify (root, "glossary.root", "Example root");
    root.addItemList ({ "C", "C#", "Db", "D", "D#", "Eb", "E", "F", "F#", "Gb", "G", "G#", "Ab", "A", "A#", "Bb", "B" }, 1);
    root.setSelectedId (1, juce::dontSendNotification);
    root.onChange = [this] { filter(); };
    identify (list, "glossary.list", "Matching chords");
    list.setModel (this);
    list.setRowHeight (58);
    list.setMultipleSelectionEnabled (false);
    identify (insert, "glossary.insert", "Add chord to draft");
    insert.onClick = [this] { insertSelected(); };
    close.onClick = [this] { if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>()) dialog->exitModalState (0); };
    filter();
    setSize (760, 580);
}
void ChordGlossary::filter()
{
    matches.clear();
    const auto query = search.getText().trim();
    if (const auto* vocabulary = chordVocabulary().getArray())
        for (int i = 0; i < vocabulary->size(); ++i)
        {
            const auto& chord = vocabulary->getReference (i);
            auto searchable = chord["name"].toString() + " " + chord["family"].toString() + " "
                + chord["suffix"].toString() + " " + root.getText() + chord["suffix"].toString()
                + " " + joined (chord["degrees"], " ") + " " + joined (chord["aliases"], " ");
            if (const auto* aliases = chord["aliases"].getArray())
                for (const auto& alias : *aliases)
                    searchable += " " + root.getText() + alias.toString();
            if ((family.getSelectedId() == 1 || family.getText() == chord["family"].toString()) && searchable.containsIgnoreCase (query))
                matches.add (i);
        }
    list.updateContent();
    list.deselectAllRows();
    if (! matches.isEmpty())
        list.selectRow (0);
    selectedRowsChanged (list.getSelectedRow());
    count.setText (juce::String (matches.size()) + " chords. Select a row, then Add to draft (or double-click).", juce::dontSendNotification);
    list.repaint();
}
int ChordGlossary::getNumRows() { return matches.size(); }
void ChordGlossary::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selectedRow)
{
    if (! juce::isPositiveAndBelow (row, matches.size()))
        return;
    const auto& chord = chordVocabulary()[matches[row]];
    g.fillAll (selectedRow ? juce::Colour (0xff254337) : surface);
    g.setColour (green);
    g.setFont (uiFont (15.0f));
    g.drawText (root.getText() + chord["suffix"].toString(), 12, 4, 150, 23, juce::Justification::centredLeft);
    g.setColour (foreground);
    g.setFont (uiFont (12.0f));
    g.drawText (chord["name"].toString(), 170, 4, width - 180, 23, juce::Justification::centredLeft);
    g.setColour (muted);
    g.drawText (chord["family"].toString(), 12, 29, 150, 20, juce::Justification::centredLeft);
    g.drawText (joined (chord["degrees"], " - "), 170, 29, width - 180, 20, juce::Justification::centredLeft);
    g.setColour (border);
    g.drawHorizontalLine (height - 1, 0, static_cast<float> (width));
}
void ChordGlossary::selectedRowsChanged (int row)
{
    const bool valid = juce::isPositiveAndBelow (row, matches.size());
    insert.setEnabled (valid);
    details.setText (valid ? "Aliases: " + (joined (chordVocabulary()[matches[row]]["aliases"], ", ").isEmpty()
        ? juce::String ("none") : joined (chordVocabulary()[matches[row]]["aliases"], ", ")) : juce::String ("No matching chords."), juce::dontSendNotification);
}
void ChordGlossary::insertSelected()
{
    const int row = list.getSelectedRow();
    if (! juce::isPositiveAndBelow (row, matches.size()))
        return;
    const auto text = root.getText() + chordVocabulary()[matches[row]]["suffix"].toString();
    if (onInsert)
        onInsert (text);
    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
        dialog->exitModalState (0);
}
void ChordGlossary::listBoxItemDoubleClicked (int, const juce::MouseEvent&) { insertSelected(); }
void ChordGlossary::returnKeyPressed (int) { insertSelected(); }
void ChordGlossary::paint (juce::Graphics& g) { g.fillAll (background); }
void ChordGlossary::resized()
{
    auto area = getLocalBounds().reduced (18);
    title.setBounds (area.removeFromTop (34));
    hint.setBounds (area.removeFromTop (66));
    area.removeFromTop (8);
    auto filters = area.removeFromTop (getWidth() < 500 ? 88 : 32);
    if (getWidth() < 500)
    {
        search.setBounds (filters.removeFromTop (40));
        filters.removeFromTop (8);
    }
    root.setBounds (filters.removeFromRight (75));
    filters.removeFromRight (8);
    family.setBounds (filters.removeFromRight (juce::jmin (180, filters.getWidth())));
    filters.removeFromRight (8);
    if (getWidth() >= 500)
        search.setBounds (filters);
    count.setBounds (area.removeFromTop (30));
    auto actions = area.removeFromBottom (32);
    close.setBounds (actions.removeFromRight (90));
    actions.removeFromRight (10);
    insert.setBounds (actions.removeFromRight (140));
    details.setBounds (area.removeFromBottom (45));
    area.removeFromBottom (8);
    list.setBounds (area);
}

NativeEditor::NumberControl::NumberControl (const juce::String& id, const juce::String& name, double min, double max, double increment, double initial, const juce::String& unit)
{
    labelStyle (label, name);
    addAndMakeVisible (label);
    addAndMakeVisible (slider);
    identify (slider, id, name);
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 78, 26);
    slider.setRange (min, max, increment);
    slider.setValue (initial, juce::dontSendNotification);
    slider.setTextValueSuffix (unit);
    slider.setDoubleClickReturnValue (true, initial);
    slider.setScrollWheelEnabled (false);
    slider.setChangeNotificationOnlyOnRelease (false);
    slider.setTooltip (name + ": drag to adjust, click the value to type, double-click to reset.");
    if (id == "tempo" || id == "velocity")
    {
        slider.setSliderStyle (juce::Slider::LinearBarVertical);
        slider.setSliderSnapsToMousePosition (false);
        slider.setMouseDragSensitivity (static_cast<int> (max - min));
        slider.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        slider.setTooltip (name + ": drag up or down to adjust, click to type, double-click to reset.");
    }
    if (id == "arp.interval")
    {
        slider.textFromValueFunction = [] (double value)
        {
            auto number = juce::String (value, 4).trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
            return number + (std::abs (value - 1) < 1.0e-8 ? " beat" : " beats");
        };
        slider.valueFromTextFunction = [] (const juce::String& text) { return juce::jlimit (1.0, 64.0, std::round (text.getDoubleValue())); };
        slider.setTextValueSuffix ({});
        slider.updateText();
        identify (decrease, "arp.interval.decrease", "Decrease restart interval by one beat");
        identify (increase, "arp.interval.increase", "Increase restart interval by one beat");
        for (auto* button : { &decrease, &increase })
        {
            addAndMakeVisible (button);
            button->setRepeatSpeed (400, 90, 30);
        }
        decrease.onClick = [this] { slider.keyPressed (juce::KeyPress (juce::KeyPress::leftKey)); };
        increase.onClick = [this] { slider.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)); };
        refreshStepper();
    }
}
bool NativeEditor::NumberControl::ValueSlider::keyPressed (const juce::KeyPress& key)
{
    if (getComponentID() == "arp.interval")
    {
        const int code = key.getKeyCode();
        if (code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey)
        {
            setValue (juce::jlimit (1.0, 64.0, std::floor (getValue() + 1.0e-8) + 1), juce::sendNotificationSync);
            return true;
        }
        if (code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey)
        {
            setValue (juce::jlimit (1.0, 64.0, std::ceil (getValue() - 1.0e-8) - 1), juce::sendNotificationSync);
            return true;
        }
    }
    return juce::Slider::keyPressed (key);
}
void NativeEditor::NumberControl::refreshStepper()
{
    decrease.setEnabled (slider.getValue() > 1);
    increase.setEnabled (slider.getValue() < 64);
}
void NativeEditor::NumberControl::resized()
{
    auto area = getLocalBounds();
    const auto* editor = findParentComponentOfClass<NativeEditor>();
    const bool inlineLabel = editor != nullptr && ! editor->phoneLayout && slider.getComponentID() == "velocity";
    label.setBounds (inlineLabel ? area.removeFromLeft (54) : area.removeFromTop (editor != nullptr && editor->phoneLayout ? 16 : 18));
    if (slider.getComponentID() == "arp.interval")
    {
        const juce::Font font (uiFont (12.0f));
        const int valueWidth = static_cast<int> (std::ceil (juce::GlyphArrangement::getStringWidth (font, "64 beats"))) + 12;
        const int preferredWidth = editor != nullptr && editor->phoneLayout ? 40 : 28;
        area.setWidth (juce::jmin (area.getWidth(), preferredWidth * 2 + valueWidth + 6));
        const int buttonWidth = juce::jmin (preferredWidth, juce::jmax (24, (area.getWidth() - 46) / 2));
        decrease.setBounds (area.removeFromLeft (buttonWidth)); area.removeFromLeft (3);
        increase.setBounds (area.removeFromRight (buttonWidth)); area.removeFromRight (3);
    }
    if (slider.getComponentID() == "tempo" || slider.getComponentID() == "velocity")
        area.setWidth (juce::jmin (area.getWidth(), slider.getComponentID() == "tempo" ? 86 : 54));
    slider.setBounds (area);
}
NativeEditor::ChoiceControl::ChoiceControl (const juce::String& id, const juce::String& name, const juce::StringArray& items)
{
    labelStyle (label, name);
    addAndMakeVisible (label);
    addAndMakeVisible (combo);
    identify (combo, id, name);
    combo.addItemList (items, 1);
}
void NativeEditor::ChoiceControl::resized()
{
    auto area = getLocalBounds();
    const auto* editor = findParentComponentOfClass<NativeEditor>();
    const bool touch = editor != nullptr && editor->phoneLayout;
    const bool inlineLabel = ! touch && combo.getComponentID() == "octave";
    label.setBounds (inlineLabel ? area.removeFromLeft (48) : area.removeFromTop (touch ? 16 : 18));
    const juce::Font font (uiFont (13.0f));
    float longest = 0.0f;
    for (int i = 0; i < combo.getNumItems(); ++i)
        longest = juce::jmax (longest, juce::GlyphArrangement::getStringWidth (font, combo.getItemText (i)));
    const int width = juce::jmax (touch ? 60 : 48, static_cast<int> (std::ceil (longest)) + 44);
    combo.setBounds (area.removeFromLeft (juce::jmin (area.getWidth(), width)).reduced (0, touch ? 1 : 2));
}
NativeEditor::NativeEditor (PluginProcessor& processor) : controller (processor), timeline (controller)
{
    look.setDefaultSansSerifTypefaceName (uiFontFamily);
    look.setColour (juce::ResizableWindow::backgroundColourId, background);
    look.setColour (juce::TextButton::buttonColourId, surface);
    look.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff345844));
    look.setColour (juce::TextButton::textColourOffId, foreground);
    look.setColour (juce::TextButton::textColourOnId, green);
    look.setColour (juce::ComboBox::backgroundColourId, surface);
    look.setColour (juce::ComboBox::outlineColourId, border);
    look.setColour (juce::ComboBox::textColourId, foreground);
    look.setColour (juce::PopupMenu::backgroundColourId, surface);
    look.setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff345844));
    look.setColour (juce::Slider::backgroundColourId, border);
    look.setColour (juce::Slider::trackColourId, amber);
    look.setColour (juce::Slider::thumbColourId, amber);
    look.setColour (juce::Slider::textBoxBackgroundColourId, background);
    look.setColour (juce::Slider::textBoxOutlineColourId, border);
    look.setColour (juce::Slider::textBoxTextColourId, foreground);
    look.setColour (juce::TextEditor::backgroundColourId, surface);
    look.setColour (juce::TextEditor::textColourId, foreground);
    look.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    look.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    look.setColour (juce::ListBox::backgroundColourId, surface);
    look.setColour (juce::GroupComponent::outlineColourId, border);
    look.setColour (juce::GroupComponent::textColourId, amber);
    setLookAndFeel (&look);
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
   #if JUCE_IOS
    viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::nonHover);
   #else
    viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
   #endif
    for (auto* component : std::initializer_list<juce::Component*> { &open, &save, &glossary, &status, &panicButton, &midiActivity, &tipsButton })
        addAndMakeVisible (component);
    for (auto* component : std::initializer_list<juce::Component*> { &sequencer, &arpeggiator, &progression, &apply, &example, &message, &play, &tempo, &clock, &octave, &velocity, &timelineViewport, &hold })
        content.addAndMakeVisible (component);
    const juce::StringArray groupNames { "PHRASE", "TIMING", "OCTAVES", "HUMANIZE", "VELOCITY ENVELOPE" };
    for (size_t i = 0; i < groups.size(); ++i)
    {
        groups[i].setText (groupNames[static_cast<int> (i)]);
        content.addAndMakeVisible (groups[i]);
    }
    for (auto* component : std::initializer_list<juce::Component*> { &style, &offset, &repeats, &rateMode, &rate, &freeRate, &gate, &swing, &swingSubdivision, &velocityHumanize, &timingHumanize, &steps, &octaveOrder, &retrigger, &interval, &strength, &target, &decay })
        content.addAndMakeVisible (component);
    timelineViewport.setViewedComponent (&timeline, false);
    timelineViewport.setScrollBarsShown (false, true);
    timelineViewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
    identify (open, "open", "Open project");
    identify (save, "save", "Save project");
    identify (glossary, "glossary.open", "Chord dictionary");
    glossary.setTooltip ("Open the chord dictionary");
    identify (apply, "apply", "Apply progression");
    identify (play, "play", "Play or stop");
    identify (panicButton, "panic", "All notes off");
    identify (midiActivity, "midi.activity", "MIDI input and output activity");
    identify (tipsButton, "tips", "Show hover tips");
    tipsButton.setClickingTogglesState (true);
    tipsButton.setToggleState (true, juce::dontSendNotification);
    tooltips = std::make_unique<juce::TooltipWindow> (this, 650);
    tipsButton.setTooltip ("Show or hide hover tips");
    tipsButton.onClick = [this]
    {
        if (tipsButton.getToggleState())
            tooltips = std::make_unique<juce::TooltipWindow> (this, 650);
        else
            tooltips.reset();
    };
    identify (progression, "progression", "Chord progression");
    progression.setMultiLine (false);
    progression.setInputRestrictions (4096);
    progression.setFont (uiFont (28.0f));
    progression.setJustification (juce::Justification::centredLeft);
    progression.setBorder (juce::BorderSize<int> (0, 16, 0, 16));
    progression.setIndents (0, 0);
    progression.setTextToShowWhenEmpty ("C - G - Am - F", muted);
    progression.onTextChange = [this]
    {
        if (! syncing)
        {
            controller.draft = progression.getText();
            controller.error.clear();
            message.setText (controller.dirty() ? "Unapplied changes" : "One chord. One bar. Fixed 4/4.", juce::dontSendNotification);
            message.setColour (juce::Label::textColourId, muted);
            message.setVisible (controller.dirty());
            resized();
        }
    };
    progression.onReturnKey = [this] { controller.applyDraft(); };
    apply.onClick = [this] { controller.applyDraft(); };
    example.onClick = [this] { progression.setText ("Dm9 - G13b9 - Cmaj9"); };
    play.onClick = [this] { controller.togglePlay(); };
    panicButton.onClick = [this] { controller.panic(); };
    open.onClick = [this] { if (chooseProject) chooseProject (false); };
    save.onClick = [this] { if ((! controller.dirty() || controller.applyDraft()) && chooseProject) chooseProject (1); };
    glossary.onClick = [this] { showGlossary(); };
    bind (tempo, "tempo");
    bind (velocity, "velocity");
    bind (octave, "octave");
    bind (style, "style", true);
    bind (offset, "offset", true);
    offset.slider.textFromValueFunction = [this] (double value) { return juce::String (static_cast<int> (value) + 1) + " / " + juce::String (startStepCount); };
    offset.slider.valueFromTextFunction = [] (const juce::String& text) { return static_cast<double> (text.getIntValue() - 1); };
    bind (repeats, "repeats", true);
    repeats.slider.onValueChange = [this]
    {
        if (! syncing) controller.set ("repeats", repeats.slider.getValue() >= 9.0 ? 0 : static_cast<int> (repeats.slider.getValue()), true);
    };
    bind (freeRate, "freeMs", true);
    bind (gate, "gate", true);
    bind (swing, "swing", true);
    bind (swingSubdivision, "swingSubdivision", true);
    bind (velocityHumanize, "velocityHumanize", true);
    bind (timingHumanize, "timingHumanize", true);
    bind (steps, "steps", true);
    bind (octaveOrder, "octaveMode", true);
    steps.slider.textFromValueFunction = [] (double value) { return juce::String (static_cast<int> (value) + 1); };
    steps.slider.valueFromTextFunction = [] (const juce::String& text) { return static_cast<double> (text.getIntValue() - 1); };
    steps.slider.updateText();
    bind (retrigger, "retrigger", true);
    bind (interval, "interval", true);
    bind (target, "target", true);
    bind (decay, "decay", true);
    freeRate.slider.setSkewFactorFromMidPoint (200);
    decay.slider.setSkewFactorFromMidPoint (1000);
    repeats.slider.textFromValueFunction = [] (double value) { return value >= 9.0 ? juce::String ("Infinite") : juce::String (static_cast<int> (value)); };
    repeats.slider.updateText();
    repeats.slider.valueFromTextFunction = [] (const juce::String& text) { return (text.containsIgnoreCase ("inf") || text.containsChar (0x221e)) ? 9.0 : text.getDoubleValue(); };
    rateMode.combo.onChange = [this] { if (! syncing) controller.set ("sync", rateMode.combo.getSelectedId() == 1, true); };
    clock.combo.onChange = [this] { if (! syncing) controller.set ("hostSync", clock.combo.getSelectedId() == 2); };
    rate.combo.onChange = [this]
    {
        const int index = rate.combo.getSelectedId() - 1;
        if (! syncing && juce::isPositiveAndBelow (index, static_cast<int> (rates.size())))
            controller.set ("rate", rates[static_cast<size_t> (index)], true);
    };
    auto toggle = [this] (juce::TextButton& button, const char* key, bool arp)
    {
        identify (button, arp ? "arp." + juce::String (key) : juce::String (key), button.getButtonText());
        button.setClickingTogglesState (true);
        button.onClick = [this, &button, key, arp] { if (! syncing) controller.set (key, button.getToggleState(), arp); };
    };
    sequencer.setFaceDiameter (20.8f);
    arpeggiator.setFaceDiameter (20.8f);
    toggle (sequencer, "sequencerEnabled", false);
    toggle (arpeggiator, "enabled", true);
    toggle (hold, "hold", true);
    hold.setTooltip ("Keep the arp playing after releasing the input keys. Play a new chord after releasing all keys to replace the latched notes.");
    bind (strength, "strength", true);
    strength.slider.onValueChange = [this]
    {
        if (syncing) return;
        auto next = controller.processor.getTimelineState();
        next["arp"].getDynamicObject()->setProperty ("strength", strength.slider.getValue());
        next["arp"].getDynamicObject()->setProperty ("velocityEnabled", strength.slider.getValue() > 0.0);
        controller.error = controller.processor.editTimeline (next, "arp.strength").getErrorMessage();
        controller.refresh();
    };
    retrigger.combo.setTooltip ("Off: keep the phrase moving. Note: restart on incoming note-ons. Beat: restart on the beat grid.");
    interval.slider.setTooltip ("Reset the arp every 1 to 64 beats in Beat mode. Use - and +, arrow keys, or type a whole number.");
    steps.slider.setTooltip ("Octave range: 1 plays the held notes; each additional octave repeats the pattern one octave higher.");
    for (auto* label : { &message, &status })
        labelStyle (*label, "");
    // Each power control belongs to the module it enables.
    content.addAndMakeVisible (sequenceModule);
    content.addAndMakeVisible (arpModule);
    for (auto* component : std::initializer_list<juce::Component*> { &sequencer, &progression, &apply, &message, &timelineViewport, &fit })
        sequenceModule.addAndMakeVisible (component);
    sequenceModule.onHeaderDoubleClick = [this] { sequenceFold = sequenceFolded ? 0 : 1; resized(); };
    arpModule.onHeaderDoubleClick = [this] { arpFold = arpFolded ? 0 : 1; phonePreview.setToggleState (false, juce::dontSendNotification); resized(); };
    arpModule.addChildComponent (arpVisualizer);
    arpModule.addChildComponent (phonePreview);
    phonePreview.setComponentID ("arp.preview.toggle");
    phonePreview.setClickingTogglesState (true);
    phonePreview.onClick = [this] { layoutPhone(); };
    for (auto* hint : { &pitchHint, &humanizeHint })
    {
        labelStyle (*hint, "");
        hint->setFont (uiFont (11.0f));
        hint->setJustificationType (juce::Justification::topLeft);
        arpModule.addAndMakeVisible (hint);
    }
    pitchHint.setText ("1 octave plays the held notes.\nEach added octave repeats the pattern higher.", juce::dontSendNotification);
    humanizeHint.setText ("Subtle variation per note.\nPreview shows one possible take.", juce::dontSendNotification);
    example.setVisible (false);
    for (auto* component : std::initializer_list<juce::Component*> { &arpeggiator, &style, &offset, &repeats, &rateMode, &rate, &freeRate, &gate, &swing, &swingSubdivision, &steps, &octaveOrder, &retrigger, &interval, &hold, &strength, &target, &decay, &velocityHumanize, &timingHumanize })
        arpModule.addAndMakeVisible (component);
    for (auto& group : groups)
    {
        arpModule.addAndMakeVisible (group);
        group.toBack();
    }
    for (auto* component : std::initializer_list<juce::Component*> { &projectTitle, &projectMenu, &undoButton, &redoButton, &mutePad, &launch, &tempo, &clock, &octave, &velocity, &play })
        addAndMakeVisible (component);
    open.setVisible (false);
    save.setVisible (false);
    identify (projectMenu, "project.menu", "Project menu");
    identify (projectTitle, "project.details", "Project details");
    identify (undoButton, "undo", "Undo last edit");
    identify (redoButton, "redo", "Redo last edit");
    undoButton.setTooltip ("Undo last edit");
    redoButton.setTooltip ("Redo last edit");
    identify (fit, "timeline.fit", "Fit progression in view");
    identify (mutePad, "pattern.mute", "Mute pattern / MIDI note 16");
    sequenceModule.addAndMakeVisible (voicingButton);
    sequenceModule.addAndMakeVisible (inversionsLabel);
    labelStyle (inversionsLabel, "Inversions");
    identify (inversionsLabel, "inversions.label", "Inversions");
    voicingButton.setComponentID ("voiceLeading");
    voicingButton.setTitle ("Inversions");
    voicingButton.onClick = [this]
    {
        juce::PopupMenu menu;
        const juce::StringArray names { "Root position", "Smooth", "Ramp Up", "Ramp Down" };
        for (int i = 0; i < names.size(); ++i)
            menu.addItem (i + 1, names[i], true, static_cast<int> (controller.state["voiceLeading"]) == i);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&voicingButton),
            [safe = juce::Component::SafePointer<NativeEditor> (this)] (int result)
            {
                if (safe != nullptr && result > 0) safe->controller.set ("voiceLeading", result - 1);
            });
    };
    projectMenu.onClick = [this] { showProjectMenu(); };
    projectTitle.onClick = [this] { showProjectDetails(); };
    undoButton.onClick = [this] { controller.undo(); };
    redoButton.onClick = [this] { controller.redo(); };
    mutePad.setClickingTogglesState (false);
    mutePad.onClick = [this] { copySource = -1; controller.selectPattern (-1); };
    launch.combo.onChange = [this]
    {
        if (! syncing)
        {
            controller.processor.setLaunchMode (launch.combo.getSelectedId() - 1);
            controller.refresh();
        }
    };
    fit.setTooltip ("Fit the whole progression in the piano roll. Turn off to scroll horizontally. Notes and timing stay the same.");
    fit.setClickingTogglesState (true);
    fit.setToggleState (true, juce::dontSendNotification);
    fit.onClick = [this] { fitNotes = fit.getToggleState(); updateTimelineSize(); };
    for (int i = 0; i < 16; ++i)
    {
        auto& pad = pads[static_cast<size_t> (i)];
        pad.slot = i;
        identify (pad, "pattern." + juce::String (i + 1), "Pattern " + juce::String (i + 1));
        pad.setTooltip ("Launch pattern " + juce::String (i + 1) + " / "
            + juce::MidiMessage::getMidiNoteName (i, true, true, 5) + " (MIDI " + juce::String (i) + ")");
        pad.setTooltip (pad.getTooltip() + " / Shift-click to arm copy, then click a destination. Escape cancels. Delete resets the focused pattern.");
        pad.onClick = [this, i]
        {
            auto& button = pads[static_cast<size_t> (i)];
            const bool shift = button.clickModifiers.isShiftDown();
            button.clickModifiers = {};
            if (controller.dirty() && ! controller.applyDraft()) return;
            if (shift)
            {
                copySource = i;
                copyRevision = static_cast<int> (controller.state["revision"]);
                controller.refresh();
                return;
            }
            if (copySource >= 0)
            {
                const int source = copySource;
                copySource = -1;
                if (static_cast<int> (controller.processor.getTransportState()["revision"]) == copyRevision && source != i)
                {
                    const auto result = controller.processor.copyPattern (source, i);
                    controller.error = result.getErrorMessage();
                    if (result.failed()) { controller.refresh(); return; }
                }
            }
            controller.selectPattern (i);
        };
        pad.onReset = [this, i]
        {
            copySource = -1;
            if (controller.dirty() && ! controller.applyDraft()) return;
            controller.processor.clearPattern (i);
            controller.refresh (true);
        };
        addAndMakeVisible (pad);
    }
    swing.slider.setTooltip ("KSH swing: delays alternate groups by up to half a subdivision.");
    timingHumanize.slider.setTooltip ("KSH timing humanize: independent early/late offsets, up to 20% of the step duration.");
    velocityHumanize.slider.setTooltip ("KSH velocity humanize: random variation relative to each note's velocity.");
    const juce::StringArray phonePages { "Perform", "Sequence", "Arp", "Groove", "Rhythm" };
    for (int i = 0; i < 5; ++i)
    {
        auto& tab = phoneTabs[static_cast<size_t> (i)];
        tab.setButtonText (phonePages[i]);
        identify (tab, "phone.page." + juce::String (i), phonePages[i] + " page");
        tab.onClick = [this, i]
        {
            phonePage = i;
            if (phoneLayout) layoutPhone();
        };
        addChildComponent (tab);
    }
    identify (viewport, "body.viewport", "Editor controls");
    identify (sequenceModule, "sequence.module", "Sequence module");
    identify (arpModule, "arp.module", "Arpeggiator module");
    performanceInfo.setFont (uiFont (14.0f));
    performanceInfo.setColour (juce::Label::textColourId, foreground);
    performanceInfo.setJustificationType (juce::Justification::centredLeft);
    addChildComponent (performanceInfo);
    arpModule.addChildComponent (rhythmPanel);
    addKeyListener (this);
    controller.changed = [this] { sync(); };
    sync();
    startTimerHz (60);
}
NativeEditor::~NativeEditor()
{
    stopTimer();
    controller.changed = nullptr;
    if (glossaryWindow != nullptr)
        delete glossaryWindow.getComponent();
    timelineViewport.setViewedComponent (nullptr, false);
    viewport.setViewedComponent (nullptr, false);
    setLookAndFeel (nullptr);
}
void NativeEditor::bind (NumberControl& control, const juce::Identifier& key, bool arp)
{
    numbers.push_back (&control);
    control.slider.onDragStart = [this] { controller.processor.beginEditGesture(); };
    control.slider.onDragEnd = [this] { controller.processor.endEditGesture(); };
    control.slider.onValueChange = [this, &control, key, arp]
    {
        if (! syncing)
            controller.set (key, control.slider.getValue(), arp);
    };
}
void NativeEditor::bind (ChoiceControl& control, const juce::Identifier& key, bool arp, int offsetValue)
{
    control.combo.onChange = [this, &control, key, arp, offsetValue]
    {
        if (! syncing)
            controller.set (key, control.combo.getSelectedId() - 1 + offsetValue, arp);
    };
}
void NativeEditor::timerCallback() { controller.poll(); }
void NativeEditor::sync()
{
    const juce::ScopedValueSetter<bool> guard (syncing, true);
    const auto& state = controller.state;
    const auto& transport = controller.transport;
    const auto arp = state["arp"];
    pitchHint.setText (static_cast<int> (arp["octaveMode"]) == 1 ? "Each note plays across the octave range before moving to the next note." : "Play the complete phrase, then repeat it one octave higher.", juce::dontSendNotification);
    voicingButton.setButtonText (juce::StringArray { "Root position", "Smooth", "Ramp Up", "Ramp Down" }[static_cast<int> (state["voiceLeading"])]);
    const bool sequenceOn = static_cast<bool> (state["sequencerEnabled"]);
    const bool arpOn = static_cast<bool> (arp["enabled"]);
    const bool standalone = controller.processor.isStandalone();
    const bool host = ! standalone && static_cast<bool> (state["hostSync"]);
    const int revision = controller.refreshSerial;
    const double previewTempo = host && static_cast<bool> (transport["hostAvailable"])
        ? static_cast<double> (transport["tempo"]) : static_cast<double> (state["tempo"]);
    arpVisualizer.showOutputPulse (static_cast<juce::uint64> (static_cast<juce::int64> (transport["arpPulseSerial"])),
        static_cast<juce::uint64> (static_cast<juce::int64> (transport["arpPulseStep"])), static_cast<int> (transport["arpPulseVelocity"]), static_cast<double> (transport["arpPulseAgeMs"]),
        static_cast<juce::uint64> (static_cast<juce::int64> (transport["arpPulseRhythmStep"])));
    const int heldNotes = static_cast<int> (transport["heldInputNotes"]);
    if (arpOn && (lastRevision != revision || heldNotes != lastArpPreviewHeld || std::abs (previewTempo - lastArpPreviewTempo) > 1.0e-6))
    {
        arpVisualizer.update (controller.processor.getArpPreviewSettings(), previewTempo, heldNotes);
        lastArpPreviewTempo = previewTempo;
        lastArpPreviewHeld = heldNotes;
    }
    if (lastRevision != revision)
    {
        lastRevision = revision;
        if (progression.getText() != controller.draft)
        {
            progression.setText (controller.draft, false);
            if (! progression.hasKeyboardFocus (true)) progression.setCaretPosition (0);
        }
        for (auto* control : numbers)
        {
            const auto id = control->slider.getComponentID();
            const auto value = id.startsWith ("arp.") ? arp[juce::Identifier (id.substring (4))] : state[juce::Identifier (id)];
            // A host restore must not move the thumb underneath a user's active drag.
            if (! control->slider.isMouseButtonDown())
                control->slider.setValue (static_cast<double> (value), juce::dontSendNotification);
        }
        octave.combo.setSelectedId (static_cast<int> (state["octave"]) + 1, juce::dontSendNotification);
        clock.combo.setSelectedId (host ? 2 : 1, juce::dontSendNotification);
        octaveOrder.combo.setSelectedId (static_cast<int> (arp["octaveMode"]) + 1, juce::dontSendNotification);
        style.combo.setSelectedId (static_cast<int> (arp["style"]) + 1, juce::dontSendNotification);
        rateMode.combo.setSelectedId (static_cast<bool> (arp["sync"]) ? 1 : 2, juce::dontSendNotification);
        retrigger.combo.setSelectedId (static_cast<int> (arp["retrigger"]) + 1, juce::dontSendNotification);
        int rateId = 0;
        for (size_t i = 0; i < rates.size(); ++i)
            if (std::abs (rates[i] - static_cast<double> (arp["rate"])) < 1.0e-10)
                rateId = static_cast<int> (i) + 1;
        rate.combo.setSelectedId (rateId, juce::dontSendNotification);
        if (rateId == 0)
            rate.combo.setText (arp["rate"].toString() + " beats", juce::dontSendNotification);
        swingSubdivision.combo.setSelectedId (static_cast<int> (arp["swingSubdivision"]) + 1, juce::dontSendNotification);
        launch.combo.setSelectedId (controller.processor.getLaunchMode() + 1, juce::dontSendNotification);
        bankState = controller.processor.getProjectState();
        projectModified = controller.processor.isDocumentModified();
        updateTimelineSize();
    }
    int availableNotes = static_cast<int> (transport["arpInputNotes"]);
    if (availableNotes <= 0 && sequenceOn)
    {
        const auto chord = state["chords"][timeline.selectedChord()];
        if (const auto* notes = chord["notes"].getArray()) availableNotes = notes->size();
    }
    if (availableNotes <= 0) availableNotes = 4;
    const int arpStyle = static_cast<int> (arp["style"]);
    const int baseLength = arpStyle == ArpeggiatorSettings::chordTrigger ? 1
        : arpStyle == ArpeggiatorSettings::upDown || arpStyle == ArpeggiatorSettings::downUp
            ? juce::jmax (1, availableNotes * 2 - 2) : availableNotes;
    startStepCount = baseLength * (static_cast<int> (arp["steps"]) + 1);
    offset.slider.setRange (0, juce::jmax (1, startStepCount - 1), 1);
    if (! offset.slider.isMouseButtonDown())
        offset.slider.setValue (static_cast<int> (arp["offset"]) % startStepCount, juce::dontSendNotification);
    offset.slider.updateText();
    offset.slider.setTooltip ("Start at this position in the current arp cycle. " + juce::String (startStepCount) + " available steps across the octave range.");
    if (! repeats.slider.isMouseButtonDown())
        repeats.slider.setValue (static_cast<int> (arp["repeats"]) == 0 ? 9 : juce::jmin (8, static_cast<int> (arp["repeats"])), juce::dontSendNotification);
    projectTitle.setButtonText (state["projectName"].toString() + (projectModified ? "  *" : ""));
    projectTitle.setTooltip (state["projectName"].toString());
    undoButton.setEnabled (controller.processor.canUndo());
    redoButton.setEnabled (controller.processor.canRedo());
    const int selectedSlot = static_cast<int> (transport["selectedPattern"]);
    sequenceModule.setEnabled (selectedSlot >= 0);
    arpModule.setEnabled (selectedSlot >= 0);
    octave.setEnabled (selectedSlot >= 0);
    velocity.setEnabled (selectedSlot >= 0);
    const int activeSlot = static_cast<int> (transport["activePattern"]);
    const int queuedSlot = static_cast<int> (transport["queuedPattern"]);
    if (copySource >= 0 && copyRevision != static_cast<int> (state["revision"])) copySource = -1;
    const double phase = static_cast<double> (juce::Time::getMillisecondCounter() % 200u) / 200.0;
    const float copyAlpha = 0.85f + 0.15f * static_cast<float> (std::cos (juce::MathConstants<double>::twoPi * phase));
    for (int i = 0; i < 16; ++i)
    {
        auto& pad = pads[static_cast<size_t> (i)];
        pad.copyArmed = i == copySource;
        pad.setAlpha (pad.copyArmed ? copyAlpha : 1.0f);
        pad.active = i == activeSlot;
        pad.queued = i == queuedSlot;
        pad.selected = i == selectedSlot;
        pad.repaint();
    }
    mutePad.setToggleState (selectedSlot < 0, juce::dontSendNotification);
    // Draft changes and validation failures do not increment the processor revision.
    if (! progression.hasKeyboardFocus (true) && progression.getText() != controller.draft)
        progression.setText (controller.draft, false);
    sequencer.setToggleState (sequenceOn, juce::dontSendNotification);
    arpeggiator.setToggleState (arpOn, juce::dontSendNotification);
    hold.setToggleState (static_cast<bool> (arp["hold"]), juce::dontSendNotification);
    strength.slider.setValue (static_cast<bool> (arp["velocityEnabled"]) ? static_cast<double> (arp["strength"]) : 0.0, juce::dontSendNotification);
    play.setEnabled (sequenceOn);
    play.setButtonText (static_cast<bool> (transport["armed"]) ? "Stop" : host ? "Arm" : "Play");
    play.setTooltip (play.getButtonText());
    tempo.setEnabled (! host);
    clock.setEnabled (! standalone);
    for (auto* component : std::initializer_list<juce::Component*> { &style, &offset, &repeats, &rateMode, &rate, &freeRate, &gate, &swing, &swingSubdivision, &velocityHumanize, &timingHumanize, &steps, &octaveOrder, &retrigger, &hold, &strength })
        component->setEnabled (arpOn);
    offset.setEnabled (arpOn && startStepCount > 1);
    interval.setEnabled (arpOn && static_cast<int> (arp["retrigger"]) == 2);
    interval.refreshStepper();
    const bool envelopeOn = arpOn && static_cast<bool> (arp["velocityEnabled"]) && static_cast<double> (arp["strength"]) > 0.0;
    target.setEnabled (envelopeOn);
    decay.setEnabled (envelopeOn);
    rate.setVisible (! arpFolded && static_cast<bool> (arp["sync"]));
    freeRate.setVisible (! arpFolded && ! static_cast<bool> (arp["sync"]));
    const bool entryStatus = controller.error.isNotEmpty() || controller.dirty();
    const bool entryStatusChanged = message.isVisible() != (entryStatus && ! sequenceFolded);
    message.setVisible (entryStatus && ! sequenceFolded);
    message.setColour (juce::Label::textColourId, controller.error.isNotEmpty() ? juce::Colour (0xffffa5aa) : muted);
    message.setText (controller.error.isNotEmpty() ? controller.error : controller.dirty() ? "Unapplied changes" : "Drag chord tones up/down to edit pitches. Apply regenerates the notes.", juce::dontSendNotification);
    if (entryStatusChanged && getWidth() > 0 && getHeight() > 0)
        resized();
    auto transportText = juce::String ("Stopped");
    if (! sequenceOn)
        transportText = arpOn ? host && ! static_cast<bool> (transport["playing"]) ? "Waiting for host transport" : "Arpeggiating MIDI input" : "MIDI thru";
    else if (static_cast<bool> (transport["playing"]))
        transportText = "Playing at " + juce::String (static_cast<double> (transport["tempo"]), 1) + " BPM";
    else if (host && static_cast<bool> (transport["armed"]))
        transportText = static_cast<bool> (transport["hostAvailable"]) ? "Waiting for host transport" : "Host transport unavailable";
    if (queuedSlot != -2)
        transportText += queuedSlot < 0 ? "  /  Mute queued" : "  /  P" + juce::String (queuedSlot + 1) + " queued";
    status.setText ((standalone ? juce::String ("Chord Runner Output") : juce::String ("MIDI to host")) + "  |  " + transportText, juce::dontSendNotification);
    performanceInfo.setText ((selectedSlot < 0 ? juce::String ("Muted") : "P" + juce::String (selectedSlot + 1).paddedLeft ('0', 2)) + "\n" + state["progression"].toString(), juce::dontSendNotification);
    if (phoneLayout)
    {
        status.setText (queuedSlot != -2 ? (queuedSlot < 0 ? juce::String ("Mute queued") : "P" + juce::String (queuedSlot + 1) + " queued")
            : (activeSlot < 0 ? juce::String ("Muted") : "P" + juce::String (activeSlot + 1)) + (static_cast<bool> (transport["playing"]) ? "  Playing" : "  Stopped"), juce::dontSendNotification);
        layoutPhone();
    }
    if (layoutModules != ((sequenceOn ? 1 : 0) | (arpOn ? 2 : 0)) && getWidth() > 0 && getHeight() > 0)
    {
        sequenceFold = arpFold = -1;
        resized();
    }
    midiActivity.update (controller.processor.lastMidiInputTime(), controller.processor.lastMidiOutputTime());
    if (arpVisualizer.isVisible()) status.setText (arpVisualizer.statusText(), juce::dontSendNotification);
    rhythmPanel.refresh();
    timeline.refreshLength();
    timeline.repaint();
}
void NativeEditor::showGlossary()
{
    if (glossaryWindow != nullptr)
    {
        glossaryWindow->toFront (true);
        return;
    }
    juce::DialogWindow::LaunchOptions options;
    auto* panel = new ChordGlossary ([safe = juce::Component::SafePointer<NativeEditor> (this)] (juce::String text)
    {
        if (safe == nullptr)
            return;
        const auto existing = safe->progression.getText().trim();
        safe->controller.draft = existing.isEmpty() ? text : existing + " - " + text;
        safe->progression.setText (safe->controller.draft);
    });
    panel->setSize (juce::jmin (760, getWidth() - 24), juce::jmin (580, getHeight() - 40));
    options.content.setOwned (panel);
    options.dialogTitle = "Chord glossary";
    options.dialogBackgroundColour = background;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.componentToCentreAround = this;
    glossaryWindow = options.launchAsync();
    glossaryWindow->setLookAndFeel (&look);
}
bool NativeEditor::keyPressed (const juce::KeyPress& key, juce::Component* origin)
{
    if (key == juce::KeyPress::escapeKey && copySource >= 0)
    {
        copySource = -1;
        controller.refresh();
        return true;
    }
    const bool command = key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown();
    if (command && (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z'))
    {
        if (auto* text = dynamic_cast<juce::TextEditor*> (origin); text != nullptr)
            return false;
        if (key.getModifiers().isShiftDown()) controller.redo(); else controller.undo();
        return true;
    }
    if (command && (key.getKeyCode() == 'S' || key.getKeyCode() == 's'))
    {
        if ((! controller.dirty() || controller.applyDraft()) && chooseProject)
            chooseProject (key.getModifiers().isShiftDown() ? 2 : 1);
        return true;
    }
    if (command && (key.getKeyCode() == 'O' || key.getKeyCode() == 'o'))
    {
        if (chooseProject) chooseProject (0);
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::returnKey && (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()))
    {
        controller.applyDraft();
        return true;
    }
    if (key == juce::KeyPress::spaceKey)
    {
        for (auto* component = origin; component != nullptr && component != this; component = component->getParentComponent())
            if (dynamic_cast<juce::TextEditor*> (component) != nullptr || dynamic_cast<juce::Button*> (component) != nullptr
                || dynamic_cast<juce::ComboBox*> (component) != nullptr || dynamic_cast<juce::Slider*> (component) != nullptr)
                return false;
        controller.togglePlay();
        return true;
    }
    return false;
}
void NativeEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);
    if (! phoneLayout)
    {
        const juce::Rectangle<float> header (0.0f, 0.0f, static_cast<float> (getWidth()), static_cast<float> (juce::jmax (0, viewport.getY() - 12)));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff242c31), header.getTopLeft(), juce::Colour (0xff1b2227), header.getBottomLeft(), false));
        g.fillRect (header);
    }
    g.setColour (foreground);
    g.setFont (uiFont (phoneLayout ? 12.0f : 16.0f).withStyle ("Bold"));
    if (! phoneLayout || getWidth() <= getHeight())
        g.drawText ("CHORD RUNNER", phoneLayout ? 12 : 18, phoneLayout ? 3 : getWidth() >= 1108 ? 24 : 5, 150, phoneLayout ? 18 : 24, juce::Justification::centredLeft);
    g.setFont (uiFont (10.0f));
    g.setColour (muted);
    if (! phoneLayout)
        g.drawText ("ofsound", 19, getWidth() >= 1108 ? 46 : 27, 100, 14, juce::Justification::centredLeft);
}

void NativeEditor::updateTimelineSize()
{
    const auto* chords = controller.state["chords"].getArray();
    const int count = chords != nullptr ? chords->size() : 1;
    const int width = fitNotes ? timelineViewport.getWidth() : juce::jmax (timelineViewport.getWidth(), 46 + count * 95);
    timeline.setSize (juce::jmax (60, width), juce::jmax (phoneLayout ? 60 : 80, timelineViewport.getHeight() - (width > timelineViewport.getWidth() ? 14 : 0)));
}

void NativeEditor::resized()
{
    // Reset the fitted desktop bodies before laying out another size or phone page.
    for (auto* module : { &sequenceModule, &arpModule })
        for (auto* child : module->getChildren())
            child->setTransform ({});
    arpOnlyLayout = ! static_cast<bool> (controller.state["sequencerEnabled"]) && static_cast<bool> (controller.state["arp"]["enabled"]);
    layoutModules = (static_cast<bool> (controller.state["sequencerEnabled"]) ? 1 : 0) | (static_cast<bool> (controller.state["arp"]["enabled"]) ? 2 : 0);
    sequenceFolded = sequenceFold >= 0 ? sequenceFold != 0 : layoutModules == 2;
    arpFolded = arpFold >= 0 ? arpFold != 0 : layoutModules == 1;
    const bool compact = getWidth() < 700 || (getHeight() <= 500 && getWidth() <= 1100);
    if (compact)
    {
        phoneLayout = true;
        layoutPhone();
        return;
    }
    restoreDesktopParents();
    phoneLayout = false;
    const int w = getWidth();
    const int margin = 16;
    const bool narrow = w < 1108;
    constexpr int padSide = 32, padColumns = 8, bankWidth = 320, bankTop = 8;
    const int bankX = w - margin - bankWidth;
    const int controlsWidth = bankX - margin - 12;
    if (controlsWidth >= 744)
    {
        auto header = juce::Rectangle<int> (margin + 156, 24, controlsWidth - 156, 38);
        play.setBounds (header.removeFromLeft (32).withSizeKeepingCentre (32, 32)); header.removeFromLeft (8);
        tempo.setBounds (header.removeFromLeft (80)); header.removeFromLeft (8);
        clock.setBounds (header.removeFromLeft (88)); header.removeFromLeft (8);
        launch.setBounds (header.removeFromLeft (96)); header.removeFromLeft (12);
        projectTitle.setBounds (header.removeFromLeft (120).withSizeKeepingCentre (120, 28)); header.removeFromLeft (6);
        projectMenu.setBounds (header.removeFromLeft (64).withSizeKeepingCentre (64, 28)); header.removeFromLeft (6);
        undoButton.setBounds (header.removeFromLeft (28).withSizeKeepingCentre (28, 28)); header.removeFromLeft (4);
        redoButton.setBounds (header.removeFromLeft (28).withSizeKeepingCentre (28, 28));
    }
    else
    {
        auto transport = juce::Rectangle<int> (margin + 156, 6, controlsWidth - 156, 38);
        play.setBounds (transport.removeFromLeft (32).withSizeKeepingCentre (32, 32)); transport.removeFromLeft (8);
        tempo.setBounds (transport.removeFromLeft (90));
        transport = juce::Rectangle<int> (margin, 80, controlsWidth, 38);
        clock.setBounds (transport.removeFromLeft (110)); transport.removeFromLeft (10);
        launch.setBounds (transport.removeFromLeft (110));
        auto projects = juce::Rectangle<int> (margin, 46, controlsWidth, 28);
        projectTitle.setBounds (projects.removeFromLeft (juce::jmin (180, controlsWidth - 142))); projects.removeFromLeft (6);
        projectMenu.setBounds (projects.removeFromLeft (64)); projects.removeFromLeft (8);
        undoButton.setBounds (projects.removeFromLeft (28)); projects.removeFromLeft (4);
        redoButton.setBounds (projects.removeFromLeft (28));
    }
    for (int i = 0; i < 16; ++i)
        pads[static_cast<size_t> (i)].setBounds (bankX + (i % padColumns) * (padSide + 4),
            bankTop + (i / padColumns) * (padSide + 4), padSide, padSide);
    mutePad.setBounds (w - margin - padSide, bankTop, padSide, padSide);
    const int bodyTop = narrow ? 126 : 88;
    viewport.setBounds (margin, bodyTop, w - margin * 2, juce::jmax (40, getHeight() - bodyTop - 50));
    auto footer = getLocalBounds().removeFromBottom (42).reduced (margin, 3);
    tipsButton.setBounds (footer.removeFromRight (36)); footer.removeFromRight (8);
    glossary.setBounds (footer.removeFromRight (36)); footer.removeFromRight (8);
    panicButton.setBounds (footer.removeFromRight (120)); footer.removeFromRight (10);
    midiActivity.setBounds (footer.removeFromRight (146)); footer.removeFromRight (10);
    status.setBounds (footer);
    const bool splitModules = ! sequenceFolded && ! arpFolded;
    const int columns = splitModules || w >= 1000 ? 5 : w >= 700 ? 3 : w >= 500 ? 2 : 1;
    const int groupRows = (5 + columns - 1) / columns;
    const int rhythmHeight = splitModules || w >= 1000 ? 96 : 128;
    const int rhythmTop = ModuleCard::headerHeight + groupRows * 240;
    const int arpHeight = rhythmTop + rhythmHeight + 8;
    const bool arpOnly = ! static_cast<bool> (controller.state["sequencerEnabled"]) && static_cast<bool> (controller.state["arp"]["enabled"]);
    arpOnlyLayout = arpOnly;
    const int minimumSequence = sequenceFolded ? ModuleCard::headerHeight : 252;
    const int minimumHeight = minimumSequence + 12 + (arpFolded ? ModuleCard::headerHeight : arpHeight) + (sequenceFolded && ! arpFolded ? 180 : 0);
    const int contentHeight = splitModules ? viewport.getHeight() : juce::jmax (minimumHeight, viewport.getHeight());
    content.setSize (viewport.getWidth() - (contentHeight > viewport.getHeight() ? 14 : 0), contentHeight);
    if (splitModules)
        viewport.setViewPosition (0, 0);
    const int actualSequenceHeight = sequenceFolded ? ModuleCard::headerHeight : splitModules ? (contentHeight - 12) / 2
        : contentHeight - (arpFolded ? ModuleCard::headerHeight : arpHeight) - 12;
    const int actualArpHeight = arpFolded ? ModuleCard::headerHeight : contentHeight - actualSequenceHeight - 12;
    // Keep the same control proportions as space contracts. Only the bodies scale;
    // module headers and power buttons retain their normal size and hit targets.
    const float bodyScale = splitModules ? juce::jmin (1.0f,
        static_cast<float> (juce::jmin (actualSequenceHeight, actualArpHeight) - ModuleCard::headerHeight) / static_cast<float> (arpHeight - ModuleCard::headerHeight)) : 1.0f;
    const float sequenceScale = splitModules ? juce::jmin (1.0f,
        static_cast<float> (actualSequenceHeight - ModuleCard::headerHeight) / 216.0f) : 1.0f;
    int cw = juce::roundToInt (static_cast<float> (content.getWidth()) / sequenceScale);
    const int arpCanvasWidth = juce::roundToInt (static_cast<float> (content.getWidth()) / bodyScale);
    const int sequenceHeight = ModuleCard::headerHeight + juce::roundToInt (static_cast<float> (actualSequenceHeight - ModuleCard::headerHeight) / sequenceScale);
    const int expandedArpHeight = ModuleCard::headerHeight + juce::roundToInt (static_cast<float> (actualArpHeight - ModuleCard::headerHeight) / bodyScale);
    const int previewHeight = sequenceFolded && ! arpFolded ? expandedArpHeight - arpHeight : 0;
    sequenceModule.setBounds (0, 0, content.getWidth(), actualSequenceHeight);
    arpModule.setBounds (0, actualSequenceHeight + 12, content.getWidth(), actualArpHeight);
    arpVisualizer.setVisible (sequenceFolded && ! arpFolded);
    rhythmPanel.setHorizontal (true);
    rhythmPanel.setBounds (4, rhythmTop + previewHeight, arpCanvasWidth - 8, rhythmHeight);
    rhythmPanel.setVisible (! arpFolded);
    rhythmPanel.setEnabled (static_cast<bool> (controller.state["arp"]["enabled"]));
    arpVisualizer.setBounds (12, ModuleCard::headerHeight, arpCanvasWidth - 24, juce::jmax (0, previewHeight - 10));
    for (auto* component : std::initializer_list<juce::Component*> { &progression, &apply, &timelineViewport, &message, &fit, &voicingButton, &inversionsLabel, &octave, &velocity })
        if (sequenceFolded) component->setVisible (false);
    sequencer.setBounds (10, 2, 32, 32);
    progression.setFont (uiFont (28.0f));
    progression.setBorder (juce::BorderSize<int> (0, 16, 0, 16));
    progression.setBounds (14, 39, cw - 128, 64);
    apply.setBounds (cw - 102, 39, 88, 64);
    for (auto* component : std::initializer_list<juce::Component*> { &velocity, &octave })
    {
        if (component->getParentComponent() != &sequenceModule) sequenceModule.addChildComponent (component);
        component->setVisible (! sequenceFolded);
    }
    const bool compactActions = cw < 900;
    const int settingsWidth = compactActions ? 456 : 484;
    const int settingsTop = sequenceHeight - 40 - (message.isVisible() ? 26 : 0);
    auto sequenceSettings = juce::Rectangle<int> (cw - 14 - settingsWidth, settingsTop, settingsWidth, 28);
    velocity.setBounds (sequenceSettings.removeFromLeft (112));
    sequenceSettings.removeFromLeft (12);
    octave.setBounds (sequenceSettings.removeFromLeft (112));
    sequenceSettings.removeFromLeft (12);
    inversionsLabel.setBounds (sequenceSettings.removeFromLeft (68));
    voicingButton.setBounds (sequenceSettings.removeFromLeft (compactActions ? 100 : 120));
    sequenceSettings.removeFromLeft (6);
    fit.setBounds (sequenceSettings.removeFromLeft (compactActions ? 34 : 42));
    timelineViewport.setBounds (14, 111, cw - 28, settingsTop - 119);
    message.setBounds (14, sequenceHeight - 27, cw - 28, 22);
    cw = arpCanvasWidth;
    arpeggiator.setBounds (10, 2, 32, 32);
    for (size_t i = 0; i < groups.size(); ++i)
    {
        constexpr std::array<int, 5> columnOrder { 2, 0, 3, 1, 4 };
        const int position = columnOrder[i];
        const int col = position % columns;
        const int x = 10 + col * (cw - 20) / columns;
        const int right = 10 + (col + 1) * (cw - 20) / columns;
        groups[i].setBounds (x, ModuleCard::headerHeight + previewHeight + position / columns * 240, right - x - 5, 236);
    }
    auto groupArea = [this] (size_t i) { return groups[i].getBounds().reduced (10, 0).withTrimmedTop (35); };
    auto a = groupArea (0);
    style.setBounds (a.removeFromTop (48)); a.removeFromTop (6);
    auto phrase = a.removeFromTop (48);
    offset.setBounds (phrase.removeFromLeft (phrase.getWidth() / 2).withTrimmedRight (5));
    repeats.setBounds (phrase); a.removeFromTop (6);
    phrase = a.removeFromTop (48);
    retrigger.setBounds (phrase.removeFromLeft (65)); phrase.removeFromLeft (6);
    interval.setBounds (phrase);
    a.removeFromTop (8);
    hold.setBounds (a.removeFromTop (28));
    a = groupArea (1);
    auto timing = a.removeFromTop (48);
    rateMode.setBounds (timing.removeFromLeft (65)); timing.removeFromLeft (6);
    rate.setBounds (timing); freeRate.setBounds (timing);
    a.removeFromTop (6); gate.setBounds (a.removeFromTop (48));
    a.removeFromTop (6);
    auto groove = a.removeFromTop (48);
    swing.setBounds (groove.removeFromLeft (juce::jmax (90, groove.getWidth() - 65)).withTrimmedRight (5));
    swingSubdivision.setBounds (groove);
    a = groupArea (2);
    steps.setBounds (a.removeFromTop (48)); a.removeFromTop (6);
    octaveOrder.setBounds (a.removeFromTop (48)); a.removeFromTop (6);

    pitchHint.setBounds (a);
    a = groupArea (3);

    velocityHumanize.setBounds (a.removeFromTop (48)); a.removeFromTop (6);
    timingHumanize.setBounds (a.removeFromTop (48)); a.removeFromTop (6);
    humanizeHint.setBounds (a);
    a = groupArea (4);
    strength.setBounds (a.removeFromTop (40)); a.removeFromTop (4);
    target.setBounds (a.removeFromTop (40)); a.removeFromTop (4);
    decay.setBounds (a.removeFromTop (40)); a.removeFromTop (6);
    freeRate.slider.setSliderStyle (juce::Slider::LinearHorizontal);
    interval.slider.setSliderStyle (juce::Slider::LinearHorizontal);
    freeRate.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 30);
    interval.slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 90, 30);
    offset.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 26);
    repeats.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 54, 26);
    swing.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 26);
    for (auto* component : std::initializer_list<juce::Component*> { &style, &offset, &repeats, &rateMode, &gate, &swing, &swingSubdivision, &steps, &octaveOrder, &retrigger, &interval, &hold, &strength, &target, &decay, &velocityHumanize, &timingHumanize, &pitchHint, &humanizeHint })
        component->setVisible (! arpFolded);
    rate.setVisible (! arpFolded && static_cast<bool> (controller.state["arp"]["sync"]));
    freeRate.setVisible (! arpFolded && ! static_cast<bool> (controller.state["arp"]["sync"]));
    for (auto& group : groups) group.setVisible (! arpFolded);
    if (splitModules)
    {
        for (auto* module : { &sequenceModule, &arpModule })
        {
            const float scale = module == &sequenceModule ? sequenceScale : bodyScale;
            const auto fitBody = juce::AffineTransform::scale (scale).translated (0.0f, static_cast<float> (ModuleCard::headerHeight) * (1.0f - scale));
            for (auto* child : module->getChildren())
                if (child != &sequencer && child != &arpeggiator)
                    child->setTransform (fitBody);
        }
    }
    updateTimelineSize();
}
}
