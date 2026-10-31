#include "NativeEditor.h"
#include "UiPalette.h"

namespace chordrunner::ui
{
namespace
{
const juce::Colour background { 0xff101114 }, surface { 0xff1b1d22 }, border { 0xff393d45 },
    foreground { 0xffe3e5e8 }, muted { 0xffc0c7d0 }, green { accentGreen }, amber { accentYellow };
constexpr float timelineRulerHeight = 16.0f, timelineHeaderHeight = 38.0f, timelineKeyWidth = 24.0f;
constexpr std::array<double, 12> rates { 4, 2, 1, 0.5, 0.25, 0.125, 0.0625, 2.0 / 3, 1.0 / 3, 1.0 / 6, 0.75, 0.375 };
// Clockwise moves from longer to shorter notes, including dotted and triplet divisions.
constexpr std::array<int, 12> rateOrder { 0, 1, 2, 10, 7, 3, 11, 8, 4, 9, 5, 6 };

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
    label.setMinimumHorizontalScale (1.0f);
}
void arpLabelStyle (juce::Label& label, const juce::String& text)
{
    labelStyle (label, text);
    label.setColour (juce::Label::textColourId, arpLabelColour);
    label.setFont (arpLabelFont());
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
    juce::Path shell;
    shell.addRoundedRectangle (area, 5.0f);

    // A broad, low-contrast reflection keeps the body diffuse rather than glossy.
    juce::ColourGradient body (juce::Colour (0xff414e5a), area.getTopLeft(),
        juce::Colour (0xff2b3742), area.getBottomRight(), false);
    body.addColour (0.42, juce::Colour (0xff394752));
    g.setGradientFill (body);
    g.fillPath (shell);

    {
        juce::Graphics::ScopedSaveState saved (g);
        g.reduceClipRegion (shell);
        const auto headerArea = area.withHeight (juce::jmin (area.getHeight(), static_cast<float> (headerHeight)));
        juce::Path plate;
        const bool folded = getHeight() == headerHeight;
        plate.addRoundedRectangle (headerArea.getX(), headerArea.getY(), headerArea.getWidth(), headerArea.getHeight(),
            5.0f, 5.0f, true, true, folded, folded);
        if (! folded) headerLift.render (g, plate);

        // The tighter reflection across the header reads as a machined metal lip.
        juce::ColourGradient metal (juce::Colour (0xff7b8e9f), headerArea.getTopLeft(),
            juce::Colour (0xff455969), headerArea.getBottomLeft(), false);
        metal.addColour (0.12, juce::Colour (0xff6c8193));
        metal.addColour (0.48, juce::Colour (0xff5a7083));
        metal.addColour (0.58, juce::Colour (0xff4e6375));
        g.setGradientFill (metal);
        g.fillPath (plate);
        rimLight.render (g, shell);
        rimShade.render (g, shell);
    }

    g.setColour (juce::Colour (0xff526573));
    g.strokePath (shell, juce::PathStrokeType (1.0f));
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawLine (6.0f, 1.5f, static_cast<float> (getWidth()) - 6.0f, 1.5f);
    if (getHeight() > headerHeight)
    {
        g.setColour (juce::Colours::black.withAlpha (0.38f));
        g.drawHorizontalLine (headerHeight - 1, 1.0f, static_cast<float> (getWidth()) - 1.0f);
    }
    juce::GlyphArrangement lettering;
    lettering.addFittedText (juce::Font (uiFont (18.0f).withStyle ("Bold")), title,
        46.0f, 0.0f, 260.0f, static_cast<float> (headerHeight), juce::Justification::centredLeft, 1, 1.0f);
    juce::Path titlePath;
    lettering.createPath (titlePath);
    // Cached blur stays behind a separate, sharp white face.
    if (! titlePath.isEmpty())
    {
        titleShade.render (g, titlePath);
        titleEdge.render (g, titlePath);
        g.setColour (juce::Colours::white);
        g.fillPath (titlePath);
    }
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
    if (getComponentID() == "apply")
    {
        juce::Path arrow;
        arrow.startNewSubPath (centre.x + 8, centre.y - 7);
        arrow.lineTo (centre.x + 8, centre.y + 2);
        arrow.quadraticTo (centre.x + 8, centre.y + 5, centre.x + 5, centre.y + 5);
        arrow.lineTo (centre.x - 8, centre.y + 5);
        arrow.startNewSubPath (centre.x - 3, centre.y);
        arrow.lineTo (centre.x - 8, centre.y + 5);
        arrow.lineTo (centre.x - 3, centre.y + 10);
        g.strokePath (arrow, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }
    if (getComponentID() == "play")
    {
        g.setColour ((getButtonText() == "Stop" ? foreground : green).withMultipliedAlpha (isEnabled() ? 1.0f : 0.35f));
        if (getButtonText() == "Stop")
            g.fillRoundedRectangle (centre.x - 6, centre.y - 6, 12, 12, 1.5f);
        else
        {
            juce::Path triangle;
            triangle.addTriangle (centre.x - 5, centre.y - 8, centre.x - 5, centre.y + 8, centre.x + 8, centre.y);
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
    auto iconArea = getLocalBounds().toFloat();
    if (heading.isNotEmpty())
    {
        iconArea.setWidth (juce::jmax (36.0f, faceDiameter + 12.0f));
        g.setColour (arpLabelColour.withAlpha (isEnabled() ? 1.0f : 0.4f));
        g.setFont (arpLabelFont (headingSize));
        g.drawText (heading, juce::roundToInt (iconArea.getRight()) + 4, 0,
            getWidth() - juce::roundToInt (iconArea.getWidth()) - 8, getHeight(), juce::Justification::centredLeft);
    }
    auto face = iconArea.withSizeKeepingCentre (diameter, diameter);
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

VoicingSelector::VoicingSelector()
{
    labelStyle (caption, "INVERSIONS");
    caption.setComponentID ("voiceLeading.label");
    addAndMakeVisible (caption);
    const juce::StringArray names { "Roots", "Smooth", "Ramp Up", "Ramp Down" };
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        auto& button = buttons[i];
        button.setButtonText (names[static_cast<int> (i)]);
        identify (button, "voiceLeading." + juce::String (static_cast<int> (i)), names[static_cast<int> (i)]);
        button.setTooltip ("Inversions: " + names[static_cast<int> (i)]);
        button.setClickingTogglesState (true);
        button.setRadioGroupId (1);
        button.onClick = [this, i]
        {
            if (onChange) onChange (static_cast<int> (i));
        };
        addAndMakeVisible (button);
    }
    setMode (0);
}

void VoicingSelector::resized()
{
    constexpr int gap = 6;
    const bool showCaption = getWidth() >= 440;
    caption.setVisible (showCaption);
    const int captionWidth = showCaption ? static_cast<int> (std::ceil (juce::GlyphArrangement::getStringWidth (caption.getFont(), caption.getText()))) + 12 : 0;
    caption.setBounds (0, 0, captionWidth, getHeight());
    const int unit = juce::jmax (0, getWidth() - captionWidth - 3 * gap) / 9;
    int left = captionWidth;
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        const int right = i + 1 == buttons.size() ? getWidth() : left + unit * (i == 0 ? 3 : 2);
        buttons[i].setBounds (left, 0, right - left, getHeight());
        left = right + gap;
    }
}

void VoicingSelector::setMode (int mode)
{
    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i].setToggleState (static_cast<int> (i) == mode, juce::dontSendNotification);
}

void NativeEditor::LatchButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const bool on = getToggleState();
    const float alpha = isEnabled() ? 1.0f : 0.35f;
    const auto area = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (amber.withAlpha (alpha * (down ? 0.16f : over ? 0.11f : on ? 0.12f : 0.065f)));
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (amber.withAlpha (alpha * (hasKeyboardFocus (true) ? 0.9f : on ? 0.65f : 0.36f)));
    g.drawRoundedRectangle (area, 5.0f, 0.8f);

    const float y = static_cast<float> (getHeight()) * 0.5f;
    juce::Path icon;
    icon.addCentredArc (10.0f, y, 4.3f, 4.3f, 0.0f, 0.65f, juce::MathConstants<float>::twoPi - 0.65f, true);
    icon.startNewSubPath (10.0f, y - 5.0f);
    icon.lineTo (10.0f, y - 0.5f);
    if (on && isEnabled())
    {
        juce::Path halo;
        halo.addEllipse (5.0f, y - 5.0f, 10.0f, 10.0f);
        glow.render (g, halo);
    }
    g.setColour (amber.withAlpha (alpha * (on ? 1.0f : 0.8f)));
    g.strokePath (icon, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (arpLabelColour.withAlpha (alpha));
    g.setFont (arpLabelFont (9.0f));
    g.drawText (getButtonText(), 20, 0, getWidth() - 23, getHeight(), juce::Justification::centredLeft);
}

juce::Font PerformanceLook::getTextButtonFont (juce::TextButton& button, int height)
{
    if (button.getComponentID() == "panic")
        return uiFont (10.0f);
    if (button.getComponentID().startsWith ("arp.offset."))
        return uiFont (juce::jmin (40.0f, static_cast<float> (height) * 0.65f));
    if (button.getComponentID().endsWith (".mode"))
        return arpLabelFont (controlLabelSize);
    if (button.getComponentID().startsWith ("arp.style."))
        return arpLabelFont (juce::jmin (controlLabelSize, static_cast<float> (height) * 0.6f));
    if (button.getComponentID().startsWith ("arp."))
        return uiFont (juce::jmin (22.0f, static_cast<float> (height) * 0.6f));
    if (button.getComponentID().startsWith ("voiceLeading."))
        return uiFont (12.0f);
    return uiFont (juce::jmin (15.0f, static_cast<float> (height) * 0.6f));
}
void PerformanceLook::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    const bool modeLabel = button.getComponentID().endsWith (".mode");
    const auto bounds = button.getLocalBounds().reduced (modeLabel ? 14 : 6, modeLabel ? 5 : 3);
    g.setFont (fitUiFont (getTextButtonFont (button, button.getHeight()), button.getButtonText(),
        static_cast<float> (bounds.getWidth()), static_cast<float> (bounds.getHeight())));
    g.setColour ((modeLabel ? arpLabelColour : button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId))
        .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred);
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
    if (button.getComponentID().endsWith (".mode"))
    {
        const auto area = button.getLocalBounds().toFloat().reduced (0.5f);
        const float alpha = button.isEnabled() ? 1.0f : 0.4f;
        g.setColour (juce::Colours::black.withAlpha (0.45f * alpha));
        g.fillRoundedRectangle (area, 5.0f);
        const auto face = area.withTrimmedBottom (down ? 1.0f : 3.0f).translated (0.0f, down ? 1.0f : 0.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (over ? 0xff52606b : 0xff43515c).withAlpha (alpha), face.getTopLeft(),
            juce::Colour (0xff27323c).withAlpha (alpha), face.getBottomLeft(), false));
        g.fillRoundedRectangle (face, 4.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.3f * alpha), face.getTopLeft(),
            juce::Colours::black.withAlpha (0.3f * alpha), face.getBottomLeft(), false));
        g.drawRoundedRectangle (face, 4.0f, 1.0f);
        if (over || button.hasKeyboardFocus (true))
        {
            g.setColour (amber.withAlpha (alpha * (button.hasKeyboardFocus (true) ? 0.65f : 0.3f)));
            g.drawRoundedRectangle (area, 4.0f, 0.8f);
        }
        return;
    }
    if (button.getComponentID().startsWith ("arp.style."))
    {
        const float alpha = button.isEnabled() ? 1.0f : 0.4f;
        const bool selected = button.getToggleState();
        const auto well = button.getLocalBounds().toFloat().reduced (0.5f);
        const auto face = well.withTrimmedBottom (2.5f).translated (0.0f, down ? 1.5f : 0.0f);
        constexpr float corner = 5.0f;

        // A dark socket and lower lip lift the phrase keys off the panel.
        g.setColour (juce::Colours::black.withAlpha (0.65f * alpha));
        g.fillRoundedRectangle (well, corner);
        auto top = selected ? juce::Colour (0xffffe99a) : juce::Colour (0xff3b454e);
        auto bottom = selected ? juce::Colour (0xffc89a32) : juce::Colour (0xff1a222a);
        if (over || down)
        {
            top = top.brighter (down ? 0.02f : 0.10f);
            bottom = bottom.brighter (down ? 0.02f : 0.10f);
        }
        juce::ColourGradient finish (top.withAlpha (alpha), face.getTopLeft(),
            bottom.withAlpha (alpha), face.getBottomLeft(), false);
        finish.addColour (0.45, (selected ? accentYellow : juce::Colour (0xff2b343d)).withAlpha (alpha));
        g.setGradientFill (finish);
        g.fillRoundedRectangle (face, corner);
        g.setColour ((selected ? juce::Colour (0xffffe9a0) : over ? accentYellow : juce::Colour (0xff56616b)).withAlpha (alpha * 0.8f));
        g.drawRoundedRectangle (face.reduced (0.5f), corner, 1.0f);

        const auto inset = face.reduced (2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (alpha * (selected ? 0.5f : 0.14f)),
            inset.getTopLeft(), juce::Colours::transparentWhite, inset.getBottomLeft(), false));
        g.drawRoundedRectangle (inset, 3.0f, 1.0f);
        if (button.hasKeyboardFocus (true))
        {
            g.setColour (accentYellow.withAlpha (alpha));
            g.drawRoundedRectangle (well.reduced (1.0f), corner, 2.0f);
        }
        return;
    }
    const auto area = button.getLocalBounds().toFloat().reduced (0.5f);
    if (button.getComponentID() == "play")
    {
        const bool playing = button.getButtonText() == "Stop";
        const auto ink = playing ? muted : green;
        const float alpha = button.isEnabled() ? 1.0f : 0.35f;
        g.setColour (juce::Colour (0xff172027).interpolatedWith (ink, down ? 0.35f : over ? 0.16f : 0.07f).withAlpha (alpha));
        g.fillRoundedRectangle (area, 7.0f);
        g.setColour ((button.hasKeyboardFocus (true) ? amber : juce::Colour (0xff39515a)).withAlpha (alpha));
        g.drawRoundedRectangle (area, 7.0f, button.hasKeyboardFocus (true) ? 2.0f : 1.0f);
        return;
    }
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
    if (slider.getComponentID() == "arp.interval" || slider.getComponentID() == "tempo" || slider.getComponentID() == "velocity" || static_cast<bool> (slider.getProperties()["valueInput"])) return;
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
void PerformanceLook::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
    float position, float startAngle, float endAngle, juce::Slider& slider)
{
    const float alpha = slider.isEnabled() ? 1.0f : 0.3f;
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
        static_cast<float> (width), static_cast<float> (height)).reduced (5.0f);
    const auto centre = bounds.getCentre();
    const float radius = juce::jmax (1.0f, juce::jmin (47.0f, juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f));
    const float angle = startAngle + position * (endAngle - startAngle);
    const auto stroke = juce::PathStrokeType (5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
    g.setColour (juce::Colour (0xff354047).withAlpha (alpha));
    g.strokePath (track, stroke);
    g.setColour (amber.withAlpha (alpha));
    g.strokePath (value, stroke);
    const float faceRadius = juce::jmax (1.0f, radius - 8.0f);
    const juce::Rectangle<float> face (centre.x - faceRadius, centre.y - faceRadius, faceRadius * 2, faceRadius * 2);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff34424a).withAlpha (alpha), face.getTopLeft(),
        juce::Colour (0xff182126).withAlpha (alpha), face.getBottomRight(), false));
    g.fillEllipse (face);
    g.setColour (juce::Colour (0xff4a5860).withAlpha (alpha * 0.7f));
    g.drawEllipse (face, 1.0f);
    const juce::Point<float> direction (std::sin (angle), -std::cos (angle));
    g.setColour (amber.withAlpha (alpha));
    g.drawLine ({ centre + direction * (faceRadius * 0.48f), centre + direction * (faceRadius * 0.86f) }, 4.0f);
}

void PerformanceLook::drawComboBox (juce::Graphics& g, int width, int height, bool down, int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0, 0, static_cast<float> (width), static_cast<float> (height)).reduced (0.5f);
    const float alpha = box.isEnabled() ? 1.0f : 0.3f;
    g.setColour (juce::Colour (down || box.isMouseOver() ? 0xff2a383e : 0xff182128).withAlpha (alpha));
    g.fillRoundedRectangle (bounds, 5);
    g.setColour (juce::Colour (0xff3b4c53).withAlpha (alpha));
    g.drawLine (5, bounds.getBottom(), bounds.getRight() - 5, bounds.getBottom(), 1);
    juce::Path arrow;
    const float cx = static_cast<float> (width) - 14, cy = static_cast<float> (height) * 0.5f;
    arrow.startNewSubPath (cx - 3, cy - 1); arrow.lineTo (cx, cy + 2); arrow.lineTo (cx + 3, cy - 1);
    g.setColour (green.withAlpha (alpha)); g.strokePath (arrow, juce::PathStrokeType (1.4f));
}
juce::Font PerformanceLook::getComboBoxFont (juce::ComboBox& box)
{
    return uiFont (static_cast<bool> (box.getProperties()["rotaryReadout"]) ? controlValueSize : 13.0f);
}
void PerformanceLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 0, box.getWidth() - 28, box.getHeight());
    label.setFont (getComboBoxFont (box));
}
void PerformanceLook::drawGroupComponentOutline (juce::Graphics& g, int w, int h, const juce::String& text,
    const juce::Justification&, juce::GroupComponent&)
{
    const auto area = juce::Rectangle<float> (0, 0, static_cast<float> (w), static_cast<float> (h));
    g.setColour (juce::Colour (0xff27313a)); g.fillRoundedRectangle (area, 6);
    g.setColour (juce::Colour (0xff64717c).withAlpha (0.45f));
    g.drawRoundedRectangle (area.reduced (0.5f), 6, 1);
    g.setColour (arpLabelColour); g.setFont (arpLabelFont());
    g.setFont (fitUiFont (g.getCurrentFont(), text, static_cast<float> (w - 20), 36.0f));
    g.drawText (text, 10, 7, w - 20, 36, juce::Justification::centredLeft);
}
void PerformanceLook::drawLabel (juce::Graphics& g, juce::Label& label)
{
    const auto* slider = dynamic_cast<juce::Slider*> (label.getParentComponent());
    if (slider == nullptr)
    {
        g.fillAll (label.findColour (juce::Label::backgroundColourId));
        if (! label.isBeingEdited())
        {
            auto font = getLabelFont (label);
            const auto textArea = label.getBorderSize().subtractedFrom (label.getLocalBounds());
            const int lines = juce::jmax (1, static_cast<int> (static_cast<float> (textArea.getHeight()) / font.getHeight()));
            g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (label.isEnabled() ? 1.0f : 0.5f));
            if (lines == 1 && ! label.getText().containsChar ('\n'))
            {
                g.setFont (fitUiFont (font, label.getText(), static_cast<float> (textArea.getWidth()), static_cast<float> (textArea.getHeight())));
                g.drawText (label.getText(), textArea, label.getJustificationType());
            }
            else
            {
                g.setFont (font);
                g.drawFittedText (label.getText(), textArea, label.getJustificationType(), lines, 1.0f);
            }
        }
        g.setColour (label.findColour (juce::Label::outlineColourId));
        g.drawRect (label.getLocalBounds());
        return;
    }
    const auto area = label.getLocalBounds().toFloat().reduced (0.5f);
    const float alpha = label.isEnabled() ? 1.0f : 0.3f;
    if (slider->getComponentID() == "tempo")
    {
        const bool active = slider->isMouseButtonDown() || label.isBeingEdited() || slider->hasKeyboardFocus (true);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff24333a).withAlpha (alpha), area.getTopLeft(),
            juce::Colour (0xff10171b).withAlpha (alpha), area.getBottomLeft(), false));
        g.fillRoundedRectangle (area, 5.0f);
        g.setColour ((active ? amber : juce::Colour (0xff39515a)).withAlpha (alpha * (active ? 1.0f : 0.65f)));
        g.drawRoundedRectangle (area.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (foreground.withAlpha (0.04f * alpha));
        g.drawLine (area.getX() + 5, area.getY() + 1, area.getRight() - 5, area.getY() + 1);
        if (! label.isBeingEdited())
        {
            g.setColour (foreground.withAlpha (alpha));
            g.setFont (uiFont (13.0f, juce::Font::bold));
            g.drawText (label.getText(), label.getLocalBounds().reduced (5, 0), juce::Justification::centred);
        }
        return;
    }
    g.setColour (juce::Colour (0xff172027).withAlpha (alpha)); g.fillRoundedRectangle (area, 4);
    if (! label.isBeingEdited())
    {
        g.setColour (foreground.withAlpha (alpha)); g.setFont (uiFont (static_cast<bool> (slider->getProperties()["compactReadout"]) ? controlValueSize : slider->isRotary() ? controlValueSize : slider->getComponentID() == "arp.interval" ? controlValueSize : 12.0f));
        g.drawText (label.getText(), label.getLocalBounds().reduced (3, 0), juce::Justification::centred);
    }
}

juce::Label* PerformanceLook::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (uiFont (static_cast<bool> (slider.getProperties()["compactReadout"]) ? controlValueSize : slider.isRotary() ? controlValueSize : slider.getComponentID() == "arp.interval" ? controlValueSize : 12.0f));
    return label;
}

juce::Slider::SliderLayout PerformanceLook::getSliderLayout (juce::Slider& slider)
{
    if (slider.getComponentID() == "arp.interval" || slider.getComponentID() == "tempo" || slider.getComponentID() == "velocity" || static_cast<bool> (slider.getProperties()["valueInput"]))
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
    setTooltip ("Drag notes to move. Drag edges to resize. Shift-click or drag empty space to select. Click a chord heading to select its tones. Snap: sixteenth notes; Alt bypasses snap.");
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
    constexpr float keyWidth = timelineKeyWidth, top = timelineHeaderHeight;
    const auto width = static_cast<float> (getWidth()) - keyWidth;
    const auto rowHeight = (static_cast<float> (getHeight()) - top) / static_cast<float> (highest - lowest + 1);
    const auto xAt = [total, width] (double beat) { return keyWidth + static_cast<float> (beat / total) * width; };
    for (int note = highest; note >= lowest; --note)
    {
        const auto y = top + static_cast<float> (highest - note) * rowHeight;
        const bool black = juce::MidiMessage::isMidiNoteBlack (note);
        g.setColour (black ? juce::Colour (0xff131519) : juce::Colour (0xff1d2026));
        g.fillRect (0.0f, y, static_cast<float> (getWidth()), rowHeight);
    }
    // Octave markers remain readable even when individual pitch rows are tiny.
    g.setFont (uiFont (8.0f));
    g.setColour (muted);
    for (int note = highest; note >= lowest; --note)
    {
        if (note % 12 != 0)
            continue;
        const float centre = top + (static_cast<float> (highest - note) + 0.5f) * rowHeight;
        const float y = juce::jlimit (top, juce::jmax (top, static_cast<float> (getHeight()) - 12.0f), centre - 6.0f);
        g.drawText (noteName (note), juce::Rectangle<float> (1, y, timelineKeyWidth - 5, 12), juce::Justification::centredRight);
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
        g.fillRect (x, timelineRulerHeight, chordWidth - 1.0f, timelineHeaderHeight - timelineRulerHeight);
        g.setColour (sounding ? green : foreground);
        g.setFont (uiFont (12.0f));
        g.drawText (chord["label"].toString() + (static_cast<bool> (chord["edited"]) ? " *" : ""), juce::Rectangle<float> (x + 6, timelineRulerHeight, chordWidth - 10, timelineHeaderHeight - timelineRulerHeight), juce::Justification::centredLeft);
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
                if (! static_cast<bool> (controller.state["arp"]["enabled"]) && rowHeight > 12 && chordWidth > 34)
                {
                    g.setColour (sounding ? background : juce::Colour (0xffcae4d7));
                    g.drawText (noteName (static_cast<int> (note)), rect.reduced (4, 0), juce::Justification::centredLeft);
                }
            }
    }
    g.setFont (uiFont (9.0f));
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
        g.drawText (juce::String (bar + 1), juce::roundToInt (x) + 5, 0, 40, static_cast<int> (timelineRulerHeight), juce::Justification::centredLeft);
    }
    // Output phrase sits above the dim source-chord bands, using note gates
    // and velocities emitted by the same native sequence/arp engines.
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double beat = static_cast<double> (controller.transport["beat"]);
    std::array<float, 128> attackLevels {};
    std::array<int, 128> attackNotes;
    attackNotes.fill (-1);
    std::array<double, 128> distances;
    distances.fill (total + 1.0);
    const bool liveSequence = sequenceOn && static_cast<bool> (controller.transport["playing"])
        && controller.transport["selectedPattern"] == controller.transport["activePattern"];
    if (liveSequence)
        for (size_t i = 0; i < outputPreview.size(); ++i)
        {
            const auto& note = outputPreview[i];
            const auto pitch = static_cast<size_t> (juce::jlimit (0, 127, note.note));
            const double due = controller.processor.lastNoteAttackTime (note.note);
            const double age = now - due;
            if (due <= 0 || age < 0 || age >= 180) continue;
            double distance = beat - note.start;
            // Allow a small lead for the currently submitted audio block.
            if (distance < -0.1) distance += total;
            distance = std::abs (distance);
            if (distance < distances[pitch])
            {
                distances[pitch] = distance;
                attackNotes[pitch] = static_cast<int> (i);
                const float level = static_cast<float> (1.0 - age / 180.0);
                attackLevels[pitch] = level * level;
            }
        }
    juce::Path attacks;
    float strongestAttack = 0;
    for (size_t pitch = 0; pitch < attackNotes.size(); ++pitch)
        if (attackNotes[pitch] >= 0)
        {
            const auto& note = outputPreview[static_cast<size_t> (attackNotes[pitch])];
            const juce::Rectangle<float> r { xAt (note.start), top + static_cast<float> (highest - note.note) * rowHeight + 0.5f,
                juce::jmax (1.5f, xAt (note.end) - xAt (note.start) - 0.75f), juce::jmax (1.5f, rowHeight - 1.0f) };
            attacks.addRoundedRectangle (r, 1.0f);
            strongestAttack = juce::jmax (strongestAttack, attackLevels[pitch]);
        }
    if (! attacks.isEmpty())
    {
        attackGlow.setColor (accentYellow.withAlpha (0.6f * strongestAttack), 0);
        attackGlow.setColor (accentYellow.withAlpha (0.8f * strongestAttack), 1);
        attackGlow.render (g, attacks);
    }
    for (const auto& note : outputPreview)
    {
        const float x = xAt (note.start);
        const float y = top + static_cast<float> (highest - note.note) * rowHeight;
        const float noteWidth = juce::jmax (1.5f, xAt (note.end) - x - 0.75f);
        g.setColour (velocityColour (note.velocity));
        g.fillRoundedRectangle ({ x, y + 0.5f, noteWidth, juce::jmax (1.5f, rowHeight - 1.0f) }, 1.0f);
    }
    for (size_t pitch = 0; pitch < attackNotes.size(); ++pitch)
        if (attackNotes[pitch] >= 0)
        {
            const auto& note = outputPreview[static_cast<size_t> (attackNotes[pitch])];
            g.setColour (velocityColour (note.velocity).brighter (0.5f).withAlpha (attackLevels[pitch] * 0.9f));
            g.fillRoundedRectangle ({ xAt (note.start), top + static_cast<float> (highest - note.note) * rowHeight + 0.5f,
                juce::jmax (1.5f, xAt (note.end) - xAt (note.start) - 0.75f), juce::jmax (1.5f, rowHeight - 1.0f) }, 1.0f);
        }
    if (static_cast<bool> (controller.state["loopEnabled"]))
    {
        const float left = xAt (static_cast<double> (controller.state["loopStart"]));
        const double end = static_cast<double> (controller.state["loopEnd"]);
        const float right = xAt (end > 0 ? end : total);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (timelineKeyWidth, top, left - timelineKeyWidth, static_cast<float> (getHeight()) - top);
        g.fillRect (right, top, static_cast<float> (getWidth()) - right, static_cast<float> (getHeight()) - top);
    }
    g.setColour (border);
    g.drawVerticalLine (static_cast<int> (timelineKeyWidth), 0, static_cast<float> (getHeight()));
    if (sequenceOn && static_cast<bool> (controller.transport["playing"]))
    {
        g.setColour (green);
        g.fillRect (xAt (static_cast<double> (controller.transport["beat"])), timelineRulerHeight, 2.0f, static_cast<float> (getHeight()) - timelineRulerHeight);
    }
    if (! static_cast<bool> (controller.state["arp"]["enabled"]) && (dragTone >= 0 || hoverPitch >= 0))
    {
        const int displayPitch = dragTone >= 0 ? draggedPitch : hoverPitch;
        g.setColour (foreground);
        g.setFont (uiFont (12.0f));
        g.drawText (noteName (displayPitch) + "  /  " + juce::String (displayPitch), getWidth() - 140, static_cast<int> (timelineRulerHeight), 130, static_cast<int> (timelineHeaderHeight - timelineRulerHeight), juce::Justification::centredRight);
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
    const float scale = static_cast<float> ((getWidth() - timelineKeyWidth) / total);
    const float row = (getHeight() - timelineHeaderHeight) / static_cast<float> (gridHigh - gridLow + 1);
    return { timelineKeyWidth + static_cast<float> (static_cast<double> (chord["noteStarts"][n])) * scale,
        timelineHeaderHeight + static_cast<float> (gridHigh - static_cast<int> (chord["notes"][n])) * row + 1.0f,
        juce::jmax (2.0f, static_cast<float> (static_cast<double> (chord["noteDurations"][n])) * scale - 2.0f),
        juce::jmax (2.0f, row - 2.0f) };
}
int TimelineView::hitNote (juce::Point<float> point) const
{
    const auto* chords = controller.state["chords"].getArray();
    if (chords == nullptr || point.y < timelineHeaderHeight)
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
    if (controller.processor.selectedPattern() < 0 || event.x < timelineKeyWidth || getWidth() <= timelineKeyWidth)
        return;
    if (selectionRevision != static_cast<int> (controller.state["revision"]))
        selectedNotes.clear();
    dragOrigin = event.position;
    dragTotal = static_cast<double> (controller.state["totalBeats"]);
    dragPixels = getWidth() - timelineKeyWidth;
    dragRevision = static_cast<int> (controller.state["revision"]);
    selectionRevision = dragRevision;
    dragSource = event.source.getIndex();
    dragChanged = false;
    dragLow = gridLow;
    dragHigh = gridHigh;
    if (isShowing())
        grabKeyboardFocus();
    if (event.y < timelineHeaderHeight)
    {
        const double beat = (event.x - timelineKeyWidth) / dragPixels * dragTotal;
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
            dragStep = juce::jmax (8.0f, (getHeight() - timelineHeaderHeight) / static_cast<float> (gridHigh - gridLow + 1));
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
    if (id.startsWith ("arp.")) arpLabelStyle (label, name);
    else labelStyle (label, name.toUpperCase());
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
    if (id == "arp.velocityHumanize" || id == "arp.timingHumanize")
        slider.getProperties().set ("valueInput", true);
    if (id == "tempo" || id == "velocity" || static_cast<bool> (slider.getProperties()["valueInput"]))
    {
        slider.setSliderStyle (juce::Slider::LinearBarVertical);
        slider.setSliderSnapsToMousePosition (false);
        slider.setMouseDragSensitivity (static_cast<int> (max - min) * (id == "tempo" || static_cast<bool> (slider.getProperties()["valueInput"]) ? 4 : 1));
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
    const bool touch = editor != nullptr && editor->phoneLayout;
    if (slider.getComponentID() == "tempo")
    {
        label.setVisible (false);
        slider.setBounds (area.withSizeKeepingCentre (area.getWidth(), juce::jmin (32, area.getHeight())));
        return;
    }
    if (static_cast<bool> (slider.getProperties()["valueInput"]))
    {
        label.setFont (arpLabelFont (touch ? 16.0f : controlLabelSize));
        if (touch)
        {
            label.setJustificationType (juce::Justification::centred);
            label.setBounds (area.removeFromTop (20));
            area.removeFromTop (2);
            slider.setBounds (area.withSizeKeepingCentre (juce::jmin (140, area.getWidth()), juce::jmin (48, area.getHeight())));
        }
        else
        {
            label.setJustificationType (juce::Justification::centredLeft);
            const int labelWidth = static_cast<int> (std::ceil (juce::GlyphArrangement::getStringWidth (label.getFont(), label.getText()))) + 10;
            area = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), labelWidth + 140), juce::jmin (56, area.getHeight()));
            label.setBounds (area.removeFromLeft (labelWidth));
            slider.setBounds (area);
        }
        return;
    }
    const bool restart = slider.getComponentID() == "arp.interval";
    const bool centred = slider.isRotary() || (restart && ! touch);
    label.setJustificationType (centred ? juce::Justification::centred : juce::Justification::centredLeft);
    const float labelSize = compact && ! touch ? 18.0f : centred ? controlLabelSize : 12.0f;
    label.setFont (slider.getComponentID().startsWith ("arp.") ? arpLabelFont (labelSize) : juce::Font (uiFont (labelSize)));
    const bool inlineLabel = ! touch && slider.getComponentID() == "velocity";
    label.setBounds (inlineLabel ? area.removeFromLeft (static_cast<int> (std::ceil (juce::GlyphArrangement::getStringWidth (label.getFont(), label.getText()))) + 10) : area.removeFromTop (compact && ! touch ? 26 : centred ? controlLabelHeight : touch ? 16 : 18));
    if (slider.getComponentID() == "arp.interval")
    {
        if (! touch) area.removeFromTop (12);
        const juce::Font font (uiFont (controlValueSize));
        const int valueWidth = static_cast<int> (std::ceil (juce::GlyphArrangement::getStringWidth (font, "64 beats"))) + 12;
        const int preferredWidth = 40;
        const int gap = touch ? 3 : 6;
        area = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), preferredWidth * 2 + valueWidth + gap * 2), area.getHeight());
        const int buttonWidth = juce::jmin (preferredWidth, juce::jmax (24, (area.getWidth() - (touch ? 46 : 110)) / 2));
        decrease.setBounds (area.removeFromLeft (buttonWidth)); area.removeFromLeft (gap);
        increase.setBounds (area.removeFromRight (buttonWidth)); area.removeFromRight (gap);
    }
    if (slider.getComponentID() == "tempo" || slider.getComponentID() == "velocity" || static_cast<bool> (slider.getProperties()["valueInput"]))
        area.setWidth (juce::jmin (area.getWidth(), slider.getComponentID() == "tempo" ? 86 : 54));
    slider.setBounds (area);
}
NativeEditor::PhraseControl::PhraseControl()
{
    setComponentID ("arp.style");
    const juce::StringArray names { "Up", "Down", "Up & Down", "Down & Up", "Converge", "Diverge", "Play Order", "Chord Trigger", "Random", "Random Other", "Random Once" };
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        auto& button = buttons[i];
        button.owner = this;
        button.setButtonText (names[modes[i]]);
        button.setComponentID ("arp.style." + juce::String (modes[i]));
        button.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff282312));
        button.setColour (juce::TextButton::textColourOffId, arpLabelColour);
        button.setClickingTogglesState (true);
        button.setRadioGroupId (2);
        button.setTooltip ("Click or hold and drag across the grid to switch arp modes. Automatable as Arp Mode in your DAW.");
        button.onClick = [this, i] { choose (i); };
        addAndMakeVisible (button);
    }
}

NativeEditor::PhraseControl::~PhraseControl() { finishDrag(); }
void NativeEditor::PhraseControl::beginDrag (int source)
{
    if (dragging) return;
    dragging = true;
    dragSource = source;
    if (onGestureStart) onGestureStart();
}
void NativeEditor::PhraseControl::finishDrag()
{
    if (! dragging) return;
    dragging = false;
    dragSource = -1;
    if (onGestureEnd) onGestureEnd();
}
void NativeEditor::PhraseControl::choose (size_t index)
{
    if (! isEnabled() || modes[index] == selectedMode) return;
    const bool single = ! dragging;
    if (single) beginDrag (-1);
    setSelected (modes[index]);
    if (onChange) onChange (modes[index]);
    if (single) finishDrag();
}
void NativeEditor::PhraseControl::selectAt (juce::Point<float> point)
{
    if (! dragging || ! isEnabled()) return;
    for (size_t i = 0; i < buttons.size(); ++i)
        if (buttons[i].getBounds().toFloat().contains (point)) { choose (i); return; }
}
void NativeEditor::PhraseControl::ModeButton::mouseDown (const juce::MouseEvent& event)
{
    if (! isEnabled() || ! event.mods.isLeftButtonDown() || owner->dragging) return;
    owner->beginDrag (event.source.getIndex());
    owner->selectAt (event.getEventRelativeTo (owner).position);
    setState (juce::Button::buttonDown);
}
void NativeEditor::PhraseControl::ModeButton::mouseDrag (const juce::MouseEvent& event)
{
    if (owner->dragSource == event.source.getIndex()) owner->selectAt (event.getEventRelativeTo (owner).position);
}
void NativeEditor::PhraseControl::ModeButton::mouseUp (const juce::MouseEvent& event)
{
    setState (juce::Button::buttonNormal);
    if (owner->dragSource == event.source.getIndex()) owner->finishDrag();
}

void NativeEditor::PhraseControl::resized()
{
    const int columns = getWidth() > 450 && getHeight() < 100 ? 6 : 3;
    const int rows = (static_cast<int> (buttons.size()) + columns - 1) / columns;
    const int gap = getHeight() < 80 ? 2 : 4;
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        const int col = static_cast<int> (i) % columns, row = static_cast<int> (i) / columns;
        const int rowColumns = juce::jmin (columns, static_cast<int> (buttons.size()) - row * columns);
        const int x = col * getWidth() / rowColumns, right = (col + 1) * getWidth() / rowColumns;
        const int y = row * getHeight() / rows, bottom = (row + 1) * getHeight() / rows;
        buttons[i].setBounds (x, y, right - x - gap, bottom - y - gap);
    }
}

void NativeEditor::PhraseControl::setSelected (int selected)
{
    selectedMode = selected;
    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i].setToggleState (modes[i] == selected, juce::dontSendNotification);
}

NativeEditor::RotateControl::RotateControl()
{
    identify (*this, "arp.offset", "Rotate");
    arpLabelStyle (label, "Rotate");
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
    previous.setButtonText (juce::String::fromUTF8 ("←"));
    next.setButtonText (juce::String::fromUTF8 ("→"));
    identify (previous, "arp.offset.previous", "Rotate backward");
    identify (next, "arp.offset.next", "Rotate forward");
    previous.setTooltip ("Rotate the phrase start backward by one step, wrapping around the cycle.");
    next.setTooltip ("Rotate the phrase start forward by one step, wrapping around the cycle.");
    addAndMakeVisible (previous);
    addAndMakeVisible (next);
}

void NativeEditor::RotateControl::resized()
{
    const auto* editor = findParentComponentOfClass<NativeEditor>();
    const bool touch = editor != nullptr && editor->phoneLayout;
    auto area = getLocalBounds();
    label.setFont (arpLabelFont (touch ? 12.0f : controlLabelSize));
    label.setBounds (area.removeFromTop (touch ? 16 : controlLabelHeight));
    if (! touch) area.removeFromTop (18);
    const int side = juce::jmax (1, juce::jmin (touch ? 44 : 84, (area.getWidth() - 10) / 2, area.getHeight()));
    area = area.withSizeKeepingCentre (side * 2 + 10, side);
    previous.setBounds (area.removeFromLeft (side));
    area.removeFromLeft (10);
    next.setBounds (area);
}

NativeEditor::ChoiceControl::ChoiceControl (const juce::String& id, const juce::String& name, const juce::StringArray& items)
{
    if (id.startsWith ("arp.")) arpLabelStyle (label, name);
    else labelStyle (label, name.toUpperCase());
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
    const bool centred = ! touch && combo.getComponentID() == "arp.sync";
    combo.getProperties().set ("rotaryReadout", centred);
    label.setJustificationType (centred ? juce::Justification::centred : juce::Justification::centredLeft);
    label.setFont (combo.getComponentID().startsWith ("arp.") ? arpLabelFont (touch ? 12.0f : controlLabelSize) : juce::Font (uiFont (12.0f)));
    label.setBounds (inlineLabel ? area.removeFromLeft (static_cast<int> (std::ceil (juce::GlyphArrangement::getStringWidth (label.getFont(), label.getText()))) + 10) : area.removeFromTop (centred ? controlLabelHeight : touch ? 16 : combo.getComponentID().startsWith ("arp.") ? 24 : 18));
    const auto font = getLookAndFeel().getComboBoxFont (combo);
    float longest = 0.0f;
    for (int i = 0; i < combo.getNumItems(); ++i)
        longest = juce::jmax (longest, juce::GlyphArrangement::getStringWidth (font, combo.getItemText (i)));
    const int width = juce::jmax (touch ? 60 : 48, static_cast<int> (std::ceil (longest)) + 44);
    const int actualWidth = juce::jmin (area.getWidth(), width);
    combo.setBounds ((centred ? area.withSizeKeepingCentre (actualWidth, area.getHeight())
                             : area.removeFromLeft (actualWidth)).reduced (0, touch ? 1 : 2));
}
NativeEditor::RetriggerControl::RetriggerControl()
{
    setComponentID ("arp.retrigger");
    arpLabelStyle (label, "Retrigger");
    label.setFont (arpLabelFont (controlLabelSize));
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
    const juce::StringArray names { "Off", "Note On", "Beat" };
    const juce::StringArray tips { "Keep the phrase moving.", "Restart on incoming note-ons.", "Restart on the beat grid." };
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        auto& button = buttons[i];
        const auto index = static_cast<int> (i);
        button.setButtonText (names[index]);
        identify (button, "arp.retrigger." + juce::String (index), "Retrigger " + names[index]);
        button.setTooltip (tips[index]);
        button.setClickingTogglesState (true);
        button.setRadioGroupId (3);
        button.onClick = [this, index] { if (onChange) onChange (index); };
        addAndMakeVisible (button);
    }
    setSelected (0);
}

void NativeEditor::RetriggerControl::resized()
{
    auto area = getLocalBounds();
    const auto* editor = findParentComponentOfClass<NativeEditor>();
    const bool touch = editor != nullptr && editor->phoneLayout;
    label.setBounds (area.removeFromTop (touch ? 16 : controlLabelHeight));
    if (! touch) area.removeFromTop (12);
    area = area.removeFromTop (controlValueHeight);
    constexpr int gap = 6;
    const int width = juce::jmax (0, area.getWidth() - gap * 2) / 3;
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        buttons[i].setBounds (area.removeFromLeft (i + 1 == buttons.size() ? area.getWidth() : width));
        area.removeFromLeft (gap);
    }
}

void NativeEditor::RetriggerControl::setSelected (int selected)
{
    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i].setToggleState (static_cast<int> (i) == selected, juce::dontSendNotification);
}

NativeEditor::ButtonPairControl::ButtonPairControl (const juce::String& id, const juce::String& name,
    const juce::StringArray& items, int width) : preferredWidth (width)
{
    identify (*this, id, name);
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        auto& button = buttons[i];
        const auto index = static_cast<int> (i);
        button.setButtonText (items[index]);
        identify (button, id + "." + juce::String (index), name + ": " + items[index]);
        button.setClickingTogglesState (true);
        button.setRadioGroupId (4);
        button.onClick = [this, index] { if (onChange) onChange (index); };
        addAndMakeVisible (button);
    }
    identify (header, id + ".mode", name + ": toggle mode");
    header.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    header.setTooltip ("Click to switch " + name.toLowerCase() + ".");
    header.onClick = [this] { if (onChange) onChange (selectedIndex == 0 ? 1 : 0); };
    addChildComponent (header);
    setSelected (0);
}

void NativeEditor::ButtonPairControl::setHeaderMode (bool value, const juce::StringArray& texts)
{
    headerMode = value;
    if (! texts.isEmpty()) headerTexts = texts;
    header.setVisible (value);
    for (auto& button : buttons) button.setVisible (! value);
    setSelected (selectedIndex);
    resized();
}

void NativeEditor::ButtonPairControl::resized()
{
    if (headerMode)
    {
        const auto font = arpLabelFont (controlLabelSize);
        const int width = juce::jmin (getWidth(), static_cast<int> (std::ceil (juce::GlyphArrangement::getStringWidth (font, header.getButtonText()))) + 32);
        header.setBounds (getLocalBounds().withSizeKeepingCentre (width, getHeight()));
        return;
    }
    auto area = getLocalBounds().withSizeKeepingCentre (juce::jmin (preferredWidth, getWidth()), juce::jmin (42, getHeight()));
    buttons[0].setBounds (area.removeFromLeft ((area.getWidth() - 6) / 2));
    area.removeFromLeft (6);
    buttons[1].setBounds (area);
}

void NativeEditor::ButtonPairControl::setSelected (int value)
{
    selectedIndex = value;
    if (juce::isPositiveAndBelow (value, headerTexts.size())) header.setButtonText (headerTexts[value]);
    buttons[0].setToggleState (value == 0, juce::dontSendNotification);
    buttons[1].setToggleState (value == 1, juce::dontSendNotification);
    if (headerMode) resized();
}

void NativeEditor::RateControl::resized()
{
    combo.getProperties().set ("rotaryReadout", rotary);
    if (! rotary)
    {
        ChoiceControl::resized();
        return;
    }
    auto area = getLocalBounds();
    label.setJustificationType (juce::Justification::centred);
    label.setFont (arpLabelFont (controlLabelSize));
    label.setBounds (area.removeFromTop (controlLabelHeight));
    combo.setBounds (area.removeFromBottom (controlValueHeight).withSizeKeepingCentre (juce::jmin (240, getWidth()), controlValueHeight));
    area.removeFromBottom (2);
    knob.setBounds (area);
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
    for (auto* component : std::initializer_list<juce::Component*> { &open, &save, &glossary, &panicButton, &midiActivity, &tipsButton, &scaleControl })
        addAndMakeVisible (component);
    scaleControl.onChange = [this] (int percent)
    {
        controller.processor.setUiScalePercent (percent);
        showUiScalePercent (percent);
        if (preferredSizeChanged) preferredSizeChanged();
    };
    for (auto* component : std::initializer_list<juce::Component*> { &sequencer, &arpeggiator, &progression, &apply, &example, &message, &play, &tempo, &clock, &octave, &velocity, &timelineViewport, &hold })
        content.addAndMakeVisible (component);
    const juce::StringArray groupNames { "", "", "Velocity Envelope" };
    for (size_t i = 0; i < groups.size(); ++i)
    {
        groups[i].setText (groupNames[static_cast<int> (i)]);
        content.addAndMakeVisible (groups[i]);
    }
    for (auto* component : std::initializer_list<juce::Component*> { &style, &offset, &repeats, &rateMode, &rate, &freeRate, &gate, &swing, &swingSubdivision, &velocityHumanize, &timingHumanize, &steps, &octaveOrder, &retrigger, &interval, &envelopePower })
        content.addAndMakeVisible (component);
    timelineViewport.setViewedComponent (&timeline, false);
    timelineViewport.setScrollBarsShown (false, true);
    timelineViewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
    identify (open, "open", "Open project");
    identify (save, "save", "Save project");
    identify (glossary, "glossary.open", "Chord dictionary");
    glossary.setTooltip ("Open the chord dictionary");
    identify (apply, "apply", "Apply progression");
    apply.setTooltip ("Apply progression (Return)");
    identify (play, "play", "Play or stop");
    identify (panicButton, "panic", "MIDI PANIC");
    identify (midiActivity, "midi.activity", "MIDI input and output activity");
    identify (tipsButton, "tips", "Show hover tips");
    tipsButton.setClickingTogglesState (true);
    tipsButton.setToggleState (true, juce::dontSendNotification);
    footerTip.setComponentID ("footer.tip");
    footerTip.setFont (uiFont (14.0f));
    footerTip.setColour (juce::Label::textColourId, muted);
    footerTip.setJustificationType (juce::Justification::centredLeft);
    footerTip.setMinimumHorizontalScale (1.0f);
    footerTip.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (footerTip);
    tipsButton.setTooltip ("Show or hide hover tips");
    tipsButton.onClick = [this]
    {
        if (! tipsButton.getToggleState()) footerTip.setText ({}, juce::dontSendNotification);
    };
    addChildComponent (completion);
    progression.completionKey = [this] (const juce::KeyPress& key) { return completion.handleKey (key); };
    progression.caretMoved = [this] (bool reopen) { if (progression.hasKeyboardFocus (true)) completion.refresh (reopen); };
    progression.onFocusLost = [this] { completion.dismiss(); };
    identify (progression, "progression", "Chord progression");
    progression.setMultiLine (false);
    progression.setInputRestrictions (4096);
    progression.setFont (juce::FontOptions ("PT Mono", 24.0f, juce::Font::plain));
    progression.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff17191e));
    progression.setColour (juce::TextEditor::outlineColourId, border);
    progression.setColour (juce::TextEditor::focusedOutlineColourId, green.withAlpha (0.6f));
    progression.setJustification (juce::Justification::centredLeft);
    progression.setBorder (juce::BorderSize<int> (0, 9, 0, 9));
    progression.setIndents (0, 0);
    progression.setTextToShowWhenEmpty ("C - G - Am - F", muted);
    progression.setTooltip ("Type a chord or click inside one for suggestions. Arrows browse; Tab/Enter insert; Escape closes. Ctrl+Space reopens suggestions. Return with the menu closed applies the phrase.");
    progression.onTextChange = [this]
    {
        if (! syncing)
        {
            controller.draft = progression.getText();
            controller.error.clear();
            message.setText ({}, juce::dontSendNotification);
            message.setVisible (false);
            resized();
            if (progression.hasKeyboardFocus (true)) completion.refresh();
        }
    };
    progression.onReturnKey = [this] { controller.draft = progression.getText(); controller.applyDraft(); };
    apply.onClick = [this] { completion.dismiss(); controller.draft = progression.getText(); controller.applyDraft(); };
    example.onClick = [this] { progression.setText ("Dm9 - G13b9 - Cmaj9"); };
    play.onClick = [this] { controller.togglePlay(); };
    panicButton.onClick = [this] { controller.panic(); };
    open.onClick = [this] { if (chooseProject) chooseProject (false); };
    save.onClick = [this] { if ((! controller.dirty() || controller.applyDraft()) && chooseProject) chooseProject (1); };
    glossary.onClick = [this] { showGlossary(); };
    bind (tempo, "tempo");
    bind (velocity, "velocity");
    bind (octave, "octave");
    style.onChange = [this] (int value) { if (! syncing) controller.set ("style", value, true); };
    style.onGestureStart = [this] { controller.processor.beginArpModeGesture(); };
    style.onGestureEnd = [this] { controller.processor.endArpModeGesture(); };
    const auto rotateStart = [this] (int direction)
    {
        if (syncing || startStepCount <= 1) return;
        const int current = static_cast<int> (controller.state["arp"]["offset"]) % startStepCount;
        controller.set ("offset", (current + direction + startStepCount) % startStepCount, true);
    };
    offset.previous.onClick = [rotateStart] { rotateStart (-1); };
    offset.next.onClick = [rotateStart] { rotateStart (1); };
    bind (repeats, "repeats", true);
    repeats.slider.onValueChange = [this]
    {
        if (! syncing) controller.set ("repeats", repeats.slider.getValue() >= 8.1 ? 0.0 : repeats.slider.getValue(), true);
    };
    bind (freeRate, "freeMs", true);
    bind (gate, "gate", true);
    bind (swing, "swing", true);
    swingSubdivision.onChange = [this] (int value) { if (! syncing) controller.set ("swingSubdivision", value, true); };
    bind (velocityHumanize, "velocityHumanize", true);
    bind (timingHumanize, "timingHumanize", true);
    bind (steps, "steps", true);
    octaveOrder.onChange = [this] (int value) { if (! syncing) controller.set ("octaveMode", 1 - value, true); };
    octaveOrder.buttons[1].setTooltip ("Play the whole phrase before moving to the next added octave.");
    octaveOrder.buttons[0].setTooltip ("Play each note through the added octaves before advancing the phrase.");
    retrigger.onChange = [this] (int mode) { if (! syncing) controller.set ("retrigger", mode, true); };
    bind (interval, "interval", true);
    freeRate.slider.setSkewFactorFromMidPoint (200);
    repeats.slider.textFromValueFunction = [] (double value) { return value >= 8.1 ? juce::String ("Infinite") : juce::String (value, 1); };
    repeats.slider.updateText();
    repeats.slider.valueFromTextFunction = [] (const juce::String& text) { return (text.containsIgnoreCase ("inf") || text.containsChar (0x221e)) ? 8.1 : text.getDoubleValue(); };
    rateMode.onChange = [this] (int value) { if (! syncing) controller.set ("sync", value == 0, true); };
    clock.combo.onChange = [this] { if (! syncing) controller.set ("hostSync", clock.combo.getSelectedId() == 2); };
    rate.combo.onChange = [this]
    {
        const int index = rate.combo.getSelectedId() - 1;
        if (! syncing && juce::isPositiveAndBelow (index, static_cast<int> (rates.size())))
            controller.set ("rate", rates[static_cast<size_t> (index)], true);
    };
    rate.addChildComponent (rate.knob);
    identify (rate.knob, "arp.rate.knob", "Rate");
    rate.knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    rate.knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    rate.knob.setRange (0, static_cast<double> (rateOrder.size() - 1), 1);
    rate.knob.setTooltip ("Rate: drag to choose a note division; use the readout for all divisions.");
    rate.knob.onDragStart = [this] { controller.processor.beginEditGesture(); };
    rate.knob.onDragEnd = [this] { controller.processor.endEditGesture(); };
    rate.knob.onValueChange = [this]
    {
        if (! syncing)
        {
            const auto index = static_cast<size_t> (juce::roundToInt (rate.knob.getValue()));
            controller.set ("rate", rates[static_cast<size_t> (rateOrder[index])], true);
        }
    };
    auto toggle = [this] (juce::TextButton& button, const char* key, bool arp)
    {
        identify (button, arp ? "arp." + juce::String (key) : juce::String (key), button.getButtonText());
        button.setClickingTogglesState (true);
        button.onClick = [this, &button, key, arp] { if (! syncing) controller.set (key, button.getToggleState(), arp); };
    };
    sequencer.setFaceDiameter (22.0f);
    arpeggiator.setFaceDiameter (22.0f);
    toggle (sequencer, "sequencerEnabled", false);
    toggle (arpeggiator, "enabled", true);
    toggle (hold, "hold", true);
    hold.setTooltip ("Keep the arp playing after releasing the input keys. Play a new chord after releasing all keys to replace the latched notes.");
    identify (envelopePower, "arp.velocityEnabled", "Velocity envelope power");
    envelopePower.setClickingTogglesState (true);
    envelopePower.setHeading ("Velocity Envelope");
    envelopePower.setFaceDiameter (44.0f);
    envelopePower.onClick = [this]
    {
        if (syncing) return;
        auto next = controller.processor.getTimelineState();
        next["arp"].getDynamicObject()->setProperty ("velocityEnabled", envelopePower.getToggleState());
        next["arp"].getDynamicObject()->setProperty ("strength", 100.0);
        controller.error = controller.processor.editTimeline (next, "arp.velocityEnabled").getErrorMessage();
        controller.refresh();
    };
    envelopeDisplay.onGestureStart = [this] { controller.processor.beginEditGesture(); };
    envelopeDisplay.onGestureEnd = [this] { controller.processor.endEditGesture(); };
    envelopeDisplay.onEdit = [this] (int start, int end, double ratio)
    {
        auto next = controller.processor.getTimelineState();
        auto* arp = next["arp"].getDynamicObject();
        arp->setProperty ("startVelocity", start);
        arp->setProperty ("target", end);
        arp->setProperty ("durationRatio", ratio);
        arp->setProperty ("strength", 100.0);
        controller.error = controller.processor.editTimeline (next, "arp.envelope").getErrorMessage();
        controller.refresh();
    };
    envelopePower.setTooltip ("Enable or bypass the velocity envelope. Humanize remains independent.");
    interval.slider.setTooltip ("Reset the arp every 1 to 64 beats in Beat mode. Use - and +, arrow keys, or type a whole number.");
    steps.slider.setTooltip ("Added octaves: 0 plays the held notes; each added octave repeats the pattern one octave higher.");
    labelStyle (message, "");
    // Each power control belongs to the module it enables.
    content.addAndMakeVisible (sequenceModule);
    content.addAndMakeVisible (arpModule);
    for (auto* component : std::initializer_list<juce::Component*> { &sequencer, &progression, &apply, &message, &timelineViewport })
        sequenceModule.addAndMakeVisible (component);
    sequenceModule.addAndMakeVisible (loopBar);
    sequenceModule.onHeaderDoubleClick = [this] { sequenceFold = sequenceFolded ? 0 : 1; resized(); };
    arpModule.onHeaderDoubleClick = [this] { arpFold = arpFolded ? 0 : 1; phonePreview.setToggleState (false, juce::dontSendNotification); resized(); };
    arpModule.addChildComponent (arpVisualizer);
    arpModule.addChildComponent (phonePreview);
    phonePreview.setComponentID ("arp.preview.toggle");
    phonePreview.setClickingTogglesState (true);
    phonePreview.onClick = [this] { layoutPhone(); };
    example.setVisible (false);
    for (auto* component : std::initializer_list<juce::Component*> { &arpeggiator, &style, &offset, &repeats, &rateMode, &rate, &freeRate, &gate, &swing, &swingSubdivision, &steps, &octaveOrder, &retrigger, &interval, &hold, &envelopePower, &velocityHumanize, &timingHumanize })
        arpModule.addAndMakeVisible (component);
    humanizeGroup.setText ("Humanize");
    humanizeGroup.setComponentID ("arp.humanize.panel");
    arpModule.addAndMakeVisible (humanizeGroup);
    humanizeGroup.toBack();
    arpModule.addAndMakeVisible (envelopeDisplay);
    arpModule.addAndMakeVisible (retriggerGroup);
    retriggerGroup.setComponentID ("arp.retrigger.panel");
    retriggerGroup.toBack();
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
    identify (mutePad, "pattern.mute", "Mute pattern / MIDI note 16");
    sequenceModule.addAndMakeVisible (voicingSelector);
    identify (voicingSelector, "voiceLeading", "Inversions");
    voicingSelector.onChange = [this] (int mode) { controller.set ("voiceLeading", mode); };
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
void NativeEditor::timerCallback()
{
    controller.poll();
    if (completion.isVisible())
    {
        if (! progression.hasKeyboardFocus (true) || ! progression.isShowing()) completion.dismiss();
        else completion.position();
    }
    juce::String tip;
    if (tipsButton.getToggleState())
    {
        auto* hovered = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
        if (hovered != nullptr && (hovered == this || isParentOf (hovered)))
        {
            for (auto* component = hovered; component != nullptr; component = component->getParentComponent())
            {
                if (auto* client = dynamic_cast<juce::TooltipClient*> (component))
                    tip = client->getTooltip();
                if (tip.isNotEmpty() || component == this) break;
            }
        }
    }
    footerTip.setText (tip, juce::dontSendNotification);
}
void NativeEditor::sync()
{
    const juce::ScopedValueSetter<bool> guard (syncing, true);
    const int scalePercent = controller.processor.getUiScalePercent();
    if (scalePercent != lastUiScalePercent)
    {
        showUiScalePercent (scalePercent);
        if (preferredSizeChanged) preferredSizeChanged();
    }
    const auto& state = controller.state;
    const auto& transport = controller.transport;
    const auto arp = state["arp"];
    voicingSelector.setMode (static_cast<int> (state["voiceLeading"]));
    const bool sequenceOn = static_cast<bool> (state["sequencerEnabled"]);
    const bool arpOn = static_cast<bool> (arp["enabled"]);
    const bool standalone = controller.processor.isStandalone();
    const bool host = controller.processor.isHosted() || (! standalone && static_cast<bool> (state["hostSync"]));
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
        octaveOrder.setSelected (1 - static_cast<int> (arp["octaveMode"]));
        style.setSelected (static_cast<int> (arp["style"]));
        rateMode.setSelected (static_cast<bool> (arp["sync"]) ? 0 : 1);
        retrigger.setSelected (static_cast<int> (arp["retrigger"]));
        int rateId = 0;
        for (size_t i = 0; i < rates.size(); ++i)
            if (std::abs (rates[i] - static_cast<double> (arp["rate"])) < 1.0e-10)
                rateId = static_cast<int> (i) + 1;
        rate.combo.setSelectedId (rateId, juce::dontSendNotification);
        if (rateId == 0)
            rate.combo.setText (arp["rate"].toString() + " beats", juce::dontSendNotification);
        if (! rate.knob.isMouseButtonDown())
        {
            size_t nearest = 0;
            for (size_t i = 1; i < rateOrder.size(); ++i)
                if (std::abs (rates[static_cast<size_t> (rateOrder[i])] - static_cast<double> (arp["rate"]))
                    < std::abs (rates[static_cast<size_t> (rateOrder[nearest])] - static_cast<double> (arp["rate"]))) nearest = i;
            rate.knob.setValue (static_cast<double> (nearest), juce::dontSendNotification);
        }
        swingSubdivision.setSelected (static_cast<int> (arp["swingSubdivision"]));
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

    if (! repeats.slider.isMouseButtonDown())
        repeats.slider.setValue (static_cast<double> (arp["repeats"]) <= 0.0 ? 8.1 : juce::jmin (8.0, static_cast<double> (arp["repeats"])), juce::dontSendNotification);
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
    envelopePower.setToggleState (static_cast<bool> (arp["velocityEnabled"]), juce::dontSendNotification);
    const bool liveVelocity = arpOn && (heldNotes > 0 || (sequenceOn && static_cast<bool> (transport["playing"]))) && static_cast<juce::int64> (transport["arpPulseSerial"]) > 0;
    envelopeDisplay.update (controller.processor.getArpPreviewSettings(),
        liveVelocity ? static_cast<int> (transport["arpSourceVelocity"]) : static_cast<int> (state["velocity"]),
        liveVelocity, static_cast<int> (transport["arpPulseVelocity"]), static_cast<double> (transport["arpEnvelopeMs"]), previewTempo, juce::jmax (1, availableNotes));
    play.setEnabled (sequenceOn);
    play.setButtonText (static_cast<bool> (transport["armed"]) ? "Stop" : host ? "Arm" : "Play");
    play.setTooltip (play.getButtonText());
    tempo.setEnabled (! host);
    clock.setEnabled (! standalone);
    for (auto* component : std::initializer_list<juce::Component*> { &style, &offset, &repeats, &rateMode, &rate, &freeRate, &gate, &swing, &swingSubdivision, &velocityHumanize, &timingHumanize, &steps, &octaveOrder, &retrigger, &hold, &envelopePower })
        component->setEnabled (arpOn);
    offset.setEnabled (arpOn && startStepCount > 1);
    interval.setEnabled (arpOn && static_cast<int> (arp["retrigger"]) == 2);
    interval.refreshStepper();
    rate.setVisible (! arpFolded && static_cast<bool> (arp["sync"]));
    freeRate.setVisible (! arpFolded && ! static_cast<bool> (arp["sync"]));
    const bool entryStatus = controller.error.isNotEmpty();
    const bool entryStatusChanged = message.isVisible() != (entryStatus && ! sequenceFolded);
    message.setVisible (entryStatus && ! sequenceFolded);
    message.setColour (juce::Label::textColourId, controller.error.isNotEmpty() ? juce::Colour (0xffffa5aa) : muted);
    message.setText (controller.error, juce::dontSendNotification);
    if (entryStatusChanged && getWidth() > 0 && getHeight() > 0)
        resized();
    performanceInfo.setText ((selectedSlot < 0 ? juce::String ("Muted") : "P" + juce::String (selectedSlot + 1).paddedLeft ('0', 2)) + "\n" + state["progression"].toString(), juce::dontSendNotification);
    if (phoneLayout)
    {
        layoutPhone();
    }
    if (layoutModules != ((sequenceOn ? 1 : 0) | (arpOn ? 2 : 0)) && getWidth() > 0 && getHeight() > 0)
    {
        resized();
    }
    midiActivity.update (controller.processor.lastMidiInputTime(), controller.processor.lastMidiOutputTime());
    rhythmPanel.refresh();
    loopBar.repaint();
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
        completion.dismiss();
        controller.draft = progression.getText();
        controller.applyDraft();
        return true;
    }
    if (key == juce::KeyPress::spaceKey)
    {
        if (controller.processor.isHosted()) return false;
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
        backdrop.paint (g, juce::Rectangle<int> (0, viewport.getY(), getWidth(), getHeight() - viewport.getY() - 42));
    if (! phoneLayout)
    {
        const std::array<juce::Rectangle<float>, 2> bands {
            juce::Rectangle<float> (0, 0, static_cast<float> (getWidth()), static_cast<float> (viewport.getY())),
            getLocalBounds().removeFromBottom (42).toFloat()
        };
        for (size_t i = 0; i < bands.size(); ++i)
        {
            const auto area = bands[i];
            juce::Path plate; plate.addRectangle (area);
            g.setColour (juce::Colour (0xff0b0d10));
            g.fillPath (plate);
            juce::ColourGradient reflection (juce::Colour (0xff090c0f), area.getTopLeft(), juce::Colour (0xff090c0f), area.getTopRight(), false);
            reflection.addColour (0.025, juce::Colour (0xff171d22));
            reflection.addColour (0.09, juce::Colour (0xff0b0d10));
            reflection.addColour (0.91, juce::Colour (0xff0b0d10));
            reflection.addColour (0.975, juce::Colour (0xff171d22));
            g.setGradientFill (reflection); g.fillPath (plate);
            chromeLight[i].render (g, plate);
            chromeShade[i].render (g, plate);
        }
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
    timeline.setSize (juce::jmax (60, timelineViewport.getWidth()),
                      juce::jmax (phoneLayout ? 60 : 80, timelineViewport.getHeight()));
}

void NativeEditor::resized()
{
    backdrop.resize (getWidth(), getHeight());
    // Reset the fitted desktop bodies before laying out another size or phone page.
    for (auto* module : { &sequenceModule, &arpModule })
        for (auto* child : module->getChildren())
            child->setTransform ({});
    arpOnlyLayout = ! static_cast<bool> (controller.state["sequencerEnabled"]) && static_cast<bool> (controller.state["arp"]["enabled"]);
    layoutModules = (static_cast<bool> (controller.state["sequencerEnabled"]) ? 1 : 0) | (static_cast<bool> (controller.state["arp"]["enabled"]) ? 2 : 0);
    sequenceFolded = sequenceFold != 0;
    arpFolded = arpFold != 0;
    const bool compact = getWidth() < 700 || (getHeight() <= 500 && getWidth() <= 1100);
    if (compact)
    {
        phoneLayout = true;
        layoutPhone();
        return;
    }
    restoreDesktopParents();
    phoneLayout = false;
    rateMode.setHeaderMode (true, { "Rate (BPM)", "Rate (ms)" });
    octaveOrder.setHeaderMode (true, { "Octaves (Note)", "Octaves (Phrase)" });
    swingSubdivision.setHeaderMode (true, { "Swing (1x)", "Swing (2x)" });
    rate.label.setVisible (false);
    freeRate.label.setVisible (false);
    steps.label.setVisible (false);
    swing.label.setVisible (false);
    const bool localTransport = ! controller.processor.isHosted();
    play.setVisible (localTransport);
    tempo.setVisible (localTransport);
    clock.setVisible (localTransport);
    const int w = getWidth();
    const int margin = 16;
    const bool narrow = w < 1264;
    constexpr int padSide = 32, padColumns = 8, bankWidth = 320, bankTop = 24;
    const int bankX = w - margin - bankWidth;
    const int controlsWidth = bankX - margin - 12;
    if (controlsWidth >= 900)
    {
        auto header = juce::Rectangle<int> (margin + 156, 24, controlsWidth - 156, 38);
        if (localTransport)
        {
            play.setBounds (header.removeFromLeft (32).withSizeKeepingCentre (32, 32)); header.removeFromLeft (8);
            tempo.setBounds (header.removeFromLeft (100)); header.removeFromLeft (8);
            clock.setBounds (header.removeFromLeft (88)); header.removeFromLeft (8);
        }
        launch.setBounds (header.removeFromLeft (96)); header.removeFromLeft (12);
        projectTitle.setBounds (header.removeFromLeft (header.getWidth() - 152).withHeight (32).withY (27)); header.removeFromLeft (6);
        projectMenu.setBounds (header.removeFromLeft (80).withSizeKeepingCentre (80, 32)); header.removeFromLeft (6);
        undoButton.setBounds (header.removeFromLeft (28).withSizeKeepingCentre (28, 28)); header.removeFromLeft (4);
        redoButton.setBounds (header.removeFromLeft (28).withSizeKeepingCentre (28, 28));
    }
    else
    {
        auto transport = juce::Rectangle<int> (margin + 156, 6, controlsWidth - 156, 38);
        if (localTransport)
        {
            play.setBounds (transport.removeFromLeft (32).withSizeKeepingCentre (32, 32)); transport.removeFromLeft (8);
            tempo.setBounds (transport.removeFromLeft (100));
        }
        transport = juce::Rectangle<int> (margin, 80, controlsWidth, 38);
        if (localTransport)
        {
            clock.setBounds (transport.removeFromLeft (110)); transport.removeFromLeft (10);
        }
        launch.setBounds (transport.removeFromLeft (110));
        auto projects = juce::Rectangle<int> (margin, 46, controlsWidth, 28);
        projectTitle.setBounds (projects.removeFromLeft (controlsWidth - 142)); projects.removeFromLeft (6);
        projectMenu.setBounds (projects.removeFromLeft (64)); projects.removeFromLeft (8);
        undoButton.setBounds (projects.removeFromLeft (28)); projects.removeFromLeft (4);
        redoButton.setBounds (projects.removeFromLeft (28));
    }
    for (int i = 0; i < 16; ++i)
    {
        auto& pad = pads[static_cast<size_t> (i)];
        pad.setVisible (i < padColumns);
        if (i < padColumns)
            pad.setBounds (bankX + i * (padSide + 4), bankTop, padSide, padSide);
    }
    mutePad.setBounds (w - margin - padSide, bankTop, padSide, padSide);
    const int bodyTop = narrow ? 122 : 66;
    viewport.setBounds (margin, bodyTop, w - margin * 2, juce::jmax (40, getHeight() - bodyTop - 42));
    auto footer = getLocalBounds().removeFromBottom (42).reduced (margin, 3);
   #if JUCE_IOS
    scaleControl.setVisible (false);
   #else
    scaleControl.setVisible (true);
    scaleControl.setBounds (footer.removeFromRight (66).withSizeKeepingCentre (66, 32));
    footer.removeFromRight (8);
   #endif
    tipsButton.setBounds (footer.removeFromRight (36)); footer.removeFromRight (8);
    glossary.setBounds (footer.removeFromRight (36)); footer.removeFromRight (8);
    panicButton.setBounds (footer.removeFromRight (120)); footer.removeFromRight (10);
    midiActivity.setBounds (footer.removeFromRight (146)); footer.removeFromRight (10);
    footerTip.setBounds (footer);
    const bool splitModules = ! sequenceFolded && ! arpFolded;
    constexpr int columns = 2;
    const int groupRows = (static_cast<int> (groups.size()) - 1 + columns - 1) / columns;
    constexpr int rhythmHeight = 144;
    constexpr int mainRowHeight = 300, modeBankGap = 12;
    constexpr int modeBankHeight = mainRowHeight + modeBankGap + 4;
    constexpr int groupStride = mainRowHeight + modeBankGap;
    constexpr int knobGroupHeight = controlLabelHeight + 112 + controlValueHeight;
    const int controlsTop = ModuleCard::headerHeight + modeBankHeight + modeBankGap;
    const int rhythmTop = controlsTop + groupRows * groupStride;
    const int arpHeight = rhythmTop + rhythmHeight + 8;
    const bool arpOnly = ! static_cast<bool> (controller.state["sequencerEnabled"]) && static_cast<bool> (controller.state["arp"]["enabled"]);
    arpOnlyLayout = arpOnly;
    const int minimumSequence = sequenceFolded ? ModuleCard::headerHeight : ModuleCard::headerHeight + 216;
    const int minimumHeight = minimumSequence + ModuleCard::moduleGap + ModuleCard::verticalMargin * 2 + (arpFolded ? ModuleCard::headerHeight : arpHeight);
    const bool previewLayout = sequenceFolded && ! arpFolded;
    const int contentHeight = splitModules || previewLayout ? viewport.getHeight() : juce::jmax (minimumHeight, viewport.getHeight());
    content.setSize (viewport.getWidth() - (contentHeight > viewport.getHeight() ? 14 : 0), contentHeight);
    if (splitModules || previewLayout)
        viewport.setViewPosition (0, 0);
    const int moduleHeight = contentHeight - ModuleCard::verticalMargin * 2 - ModuleCard::moduleGap;
    const int actualSequenceHeight = sequenceFolded ? ModuleCard::headerHeight : splitModules ? moduleHeight / 2
        : moduleHeight - (arpFolded ? ModuleCard::headerHeight : arpHeight);
    const int actualArpHeight = arpFolded ? ModuleCard::headerHeight : moduleHeight - actualSequenceHeight;
    // Fit ARP controls against the same half-height budget in both routing modes.
    // Collapsing SEQ gives the preview more space, never a different control scale.
    const int sharedBodyHeight = juce::jmax (1, (viewport.getHeight() - ModuleCard::verticalMargin * 2 - ModuleCard::moduleGap) / 2 - ModuleCard::headerHeight);
    const float bodyScale = juce::jmin (1.0f,
        static_cast<float> (sharedBodyHeight) / static_cast<float> (arpHeight - ModuleCard::headerHeight));
    const float sequenceScale = splitModules ? juce::jmin (1.0f,
        static_cast<float> (actualSequenceHeight - ModuleCard::headerHeight) / 216.0f) : 1.0f;
    int cw = juce::roundToInt (static_cast<float> (content.getWidth()) / sequenceScale);
    const int arpCanvasWidth = juce::roundToInt (static_cast<float> (content.getWidth()) / bodyScale);
    const int sequenceHeight = ModuleCard::headerHeight + juce::roundToInt (static_cast<float> (actualSequenceHeight - ModuleCard::headerHeight) / sequenceScale);
    const int expandedArpHeight = ModuleCard::headerHeight + juce::roundToInt (static_cast<float> (actualArpHeight - ModuleCard::headerHeight) / bodyScale);
    const int previewHeight = sequenceFolded && ! arpFolded ? expandedArpHeight - arpHeight : 0;
    sequenceModule.setBounds (0, ModuleCard::verticalMargin, content.getWidth(), actualSequenceHeight);
    arpModule.setBounds (0, ModuleCard::verticalMargin + actualSequenceHeight + ModuleCard::moduleGap, content.getWidth(), actualArpHeight);
    arpVisualizer.setVisible (sequenceFolded && ! arpFolded);
    rhythmPanel.setHorizontal (true);
    const int rhythmWidth = (arpCanvasWidth - 8) * 3 / 4;
    rhythmPanel.setBounds (4, rhythmTop + previewHeight, rhythmWidth, rhythmHeight);
    humanizeGroup.setBounds (rhythmWidth + 8, rhythmTop + previewHeight + 8, arpCanvasWidth - rhythmWidth - 12, rhythmHeight - 16);
    auto humanizeArea = humanizeGroup.getBounds().reduced (10, 0).withTrimmedTop (40);
    const std::array<NumberControl*, 2> humanizeControls { &timingHumanize, &velocityHumanize };
    for (size_t i = 0; i < humanizeControls.size(); ++i)
    {
        auto& knob = *humanizeControls[i];
        knob.compact = true;
        knob.label.setText (i == 0 ? "Timing" : "Velocity", juce::dontSendNotification);
        knob.slider.getProperties().set ("compactReadout", true);
        knob.slider.setSliderStyle (juce::Slider::LinearBarVertical);
        knob.slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 140, 56);
        const int left = humanizeArea.getX() + static_cast<int> (i) * humanizeArea.getWidth() / 2;
        knob.setBounds (left + 4, humanizeArea.getY(), humanizeArea.getWidth() / 2 - 8, humanizeArea.getHeight());
    }
    rhythmPanel.setVisible (! arpFolded);
    rhythmPanel.setEnabled (static_cast<bool> (controller.state["arp"]["enabled"]));
    arpVisualizer.setBounds (12, ModuleCard::headerHeight, content.getWidth() - 24,
        juce::jmax (0, juce::roundToInt (static_cast<float> (previewHeight) * bodyScale) - 10));
    for (auto* component : std::initializer_list<juce::Component*> { &progression, &apply, &timelineViewport, &message, &voicingSelector, &octave, &velocity })
        if (sequenceFolded) component->setVisible (false);
    sequencer.setBounds (8, 0, 32, 32);
    loopBar.setVisible (! sequenceFolded);
    progression.setBounds (14, ModuleCard::headerHeight + 12, cw - 80, 40);
    apply.setBounds (cw - 54, ModuleCard::headerHeight + 12, 40, 40);
    for (auto* component : std::initializer_list<juce::Component*> { &velocity, &octave })
    {
        if (component->getParentComponent() != &sequenceModule) sequenceModule.addChildComponent (component);
        component->setVisible (! sequenceFolded);
    }
    const int settingsTop = sequenceHeight - 40 - (message.isVisible() ? 26 : 0);
    voicingSelector.setBounds (14, settingsTop, juce::jmin (520, cw - 330), 28);
    auto sequenceSettings = juce::Rectangle<int> (cw - 310, settingsTop, 296, 28);
    velocity.setBounds (sequenceSettings.removeFromLeft (150));
    sequenceSettings.removeFromLeft (12);
    octave.setBounds (sequenceSettings);
    loopBar.setBounds (14, ModuleCard::headerHeight + 60, cw - 28, 24);
    timelineViewport.setBounds (14, ModuleCard::headerHeight + 84, cw - 28, settingsTop - ModuleCard::headerHeight - 92);
    message.setBounds (14, sequenceHeight - 27, cw - 28, 22);
    cw = arpCanvasWidth;
    arpeggiator.setBounds (8, 0, 32, 32);
    auto modeRow = juce::Rectangle<int> (20, ModuleCard::headerHeight + previewHeight, cw - 40, modeBankHeight)
        .withTrimmedTop (modeBankGap + 4);
    style.setBounds (modeRow.removeFromLeft ((modeRow.getWidth() - 12) / 2));
    modeRow.removeFromLeft (12);
    const int retriggerWidth = (modeRow.getWidth() - 12) / 4;
    retriggerGroup.setBounds (modeRow.removeFromRight (retriggerWidth));
    modeRow.removeFromRight (12);
    groups[0].setBounds (modeRow);
    for (size_t i = 0; i < groups.size(); ++i)
    {
        if (i == 0) continue;
        const int position = static_cast<int> (i) - 1;
        const int col = position % columns;
        const int x = 10 + col * (cw - 20) / columns;
        const int right = 10 + (col + 1) * (cw - 20) / columns;
        groups[i].setBounds (x, controlsTop + previewHeight + position / columns * groupStride, right - x - 5, mainRowHeight);
    }
    auto groupArea = [this] (size_t i)
    {
        return i < 2 ? groups[i].getBounds().reduced (10, 6)
                      : groups[i].getBounds().reduced (10, 0).withTrimmedTop (47);
    };
    auto a = groupArea (0);
    const auto phraseKnobs = a.withSizeKeepingCentre (a.getWidth(), knobGroupHeight);
    const int phraseColumnWidth = phraseKnobs.getWidth() / 3;
    const int retriggerControlHeight = controlLabelHeight + 12 + controlValueHeight;
    constexpr int retriggerGap = 16;
    const auto retriggerArea = retriggerGroup.getBounds().reduced (10, 6)
        .withSizeKeepingCentre (retriggerGroup.getWidth() - 20, retriggerControlHeight * 2 + retriggerGap);
    retrigger.setBounds (retriggerArea.getX() + 4, retriggerArea.getY(), retriggerArea.getWidth() - 8, controlLabelHeight + 12 + controlValueHeight);
    interval.setBounds (retriggerArea.getX() + 4, retriggerArea.getBottom() - (controlLabelHeight + 12 + controlValueHeight), retriggerArea.getWidth() - 8, controlLabelHeight + 12 + controlValueHeight);
    offset.setBounds (phraseKnobs.getX() + 4, phraseKnobs.getY(), phraseColumnWidth - 8, phraseKnobs.getHeight());
    const std::array<NumberControl*, 2> phraseControls { &repeats, &steps };
    for (size_t i = 0; i < phraseControls.size(); ++i)
    {
        auto& knob = *phraseControls[i];
        knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 164, controlValueHeight);
        const int left = phraseKnobs.getX() + (static_cast<int> (i) + 1) * phraseKnobs.getWidth() / 3;
        const int right = phraseKnobs.getX() + (static_cast<int> (i) + 2) * phraseKnobs.getWidth() / 3;
        knob.setBounds (left + 4, phraseKnobs.getY(), right - left - 8, phraseKnobs.getHeight());
    }
    octaveOrder.setBounds (steps.getX(), steps.getY(), steps.getWidth(), controlLabelHeight);
    const juce::Font headerFont (uiFont (18.0f).withStyle ("Bold"));
    const int latchX = 46 + static_cast<int> (std::ceil (juce::GlyphArrangement::getStringWidth (headerFont, "ARPEGGIATOR"))) + 12;
    hold.setBounds (latchX, 5, 62, 21);
    hold.setVisible (true);
    a = groupArea (1);
    const auto knobs = a.withSizeKeepingCentre (a.getWidth(), knobGroupHeight);
    rate.setRotary (true);
    const std::array<NumberControl*, 3> timingKnobs { &freeRate, &gate, &swing };
    for (size_t i = 0; i < timingKnobs.size(); ++i)
    {
        auto& knob = *timingKnobs[i];
        knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, i == 0 ? 200 : 164, controlValueHeight);
        const int left = knobs.getX() + static_cast<int> (i) * knobs.getWidth() / 3;
        const int right = knobs.getX() + (static_cast<int> (i) + 1) * knobs.getWidth() / 3;
        knob.setBounds (left + 4, knobs.getY(), right - left - 8, knobs.getHeight());
    }
    rate.setBounds (freeRate.getBounds());
    rateMode.setBounds (freeRate.getX(), freeRate.getY(), freeRate.getWidth(), controlLabelHeight);
    swingSubdivision.setBounds (swing.getX(), swing.getY(), swing.getWidth(), controlLabelHeight);
    a = groupArea (2);
    groups[2].setText ({});
    envelopePower.setBounds (groups[2].getX() + 4, groups[2].getY() + 4, groups[2].getWidth() - 8, 60);
    envelopeDisplay.setBounds (a.withTrimmedTop (20).withTrimmedBottom (8));
    interval.slider.setSliderStyle (juce::Slider::LinearHorizontal);
    interval.slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 90, 30);
    for (auto* component : std::initializer_list<juce::Component*> { &style, &offset, &repeats, &rateMode, &gate, &swing, &swingSubdivision, &steps, &octaveOrder, &retrigger, &interval, &envelopePower, &velocityHumanize, &timingHumanize })
        component->setVisible (! arpFolded);
    rate.setVisible (! arpFolded && static_cast<bool> (controller.state["arp"]["sync"]));
    freeRate.setVisible (! arpFolded && ! static_cast<bool> (controller.state["arp"]["sync"]));
    for (auto& group : groups) group.setVisible (! arpFolded);
    retriggerGroup.setVisible (! arpFolded);
    envelopeDisplay.setVisible (! arpFolded);
    humanizeGroup.setVisible (! arpFolded);
    for (auto* mode : { &rateMode, &octaveOrder, &swingSubdivision }) mode->toFront (false);
    if (splitModules || previewLayout)
    {
        for (auto* module : { &sequenceModule, &arpModule })
        {
            const float scale = module == &sequenceModule ? sequenceScale : bodyScale;
            const auto fitBody = juce::AffineTransform::scale (scale).translated (0.0f, static_cast<float> (ModuleCard::headerHeight) * (1.0f - scale));
            for (auto* child : module->getChildren())
                if (child != &sequencer && child != &arpeggiator && child != &hold && child != &arpVisualizer)
                    child->setTransform (fitBody);
        }
    }
    updateTimelineSize();
}
}
