#include "ScaleControl.h"
#include "UiPalette.h"

namespace chordrunner::ui
{
ScaleControl::ScaleControl()
{
    setComponentID ("ui.scale");
    setName ("UI scale percent");
    setTitle (getName());
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTooltip ("UI scale: drag vertically or scroll; double-click to type; right-click to reset to 100%.");
    input.setComponentID ("ui.scale.input");
    input.setJustification (juce::Justification::centred);
    input.setInputRestrictions (3, "0123456789");
    input.setFont (uiFont (12.0f));
    input.onReturnKey = [this] { acceptInput(); };
    input.onFocusLost = [this] { if (input.isVisible()) acceptInput(); };
    input.onEscapeKey = [this] { input.setVisible (false); if (isShowing()) grabKeyboardFocus(); };
    addChildComponent (input);
}

void ScaleControl::setValue (int value)
{
    percent = juce::jlimit (UiScale::minimumPercent, UiScale::maximumPercent, value);
    repaint();
}

void ScaleControl::update (int value)
{
    value = juce::jlimit (UiScale::minimumPercent, UiScale::maximumPercent, value);
    if (percent == value) return;
    setValue (value);
    if (onChange) onChange (percent);
}

void ScaleControl::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colour (dragging || input.isVisible() ? 0xff29363d : 0xff1b2227));
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (hasKeyboardFocus (true) ? accentGreen : juce::Colour (isMouseOver() ? 0xff59636b : 0xff393d45));
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
    g.setColour (juce::Colour (0xffe3e5e8));
    g.setFont (uiFont (12.0f).withStyle ("Medium"));
    g.drawText (juce::String (percent) + "%", getLocalBounds(), juce::Justification::centred);
}

void ScaleControl::resized() { input.setBounds (getLocalBounds().reduced (1)); }

void ScaleControl::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu()) { update (UiScale::defaultPercent); return; }
    if (isShowing()) grabKeyboardFocus();
    dragStartPercent = percent;
    dragStartY = event.getScreenY();
    dragging = true;
    repaint();
}

void ScaleControl::mouseDrag (const juce::MouseEvent& event)
{
    if (dragging) update (dragStartPercent + (dragStartY - event.getScreenY()) / 4);
}

void ScaleControl::mouseUp (const juce::MouseEvent&) { dragging = false; repaint(); }

void ScaleControl::mouseDoubleClick (const juce::MouseEvent&)
{
    dragging = false;
    input.setText (juce::String (percent), false);
    input.setVisible (true);
    if (isShowing()) input.grabKeyboardFocus();
    input.selectAll();
}

void ScaleControl::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (! dragging && std::abs (wheel.deltaY) > 0.00001f) update (percent + (wheel.deltaY > 0 ? 1 : -1));
}

bool ScaleControl::keyPressed (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    const int step = key.getModifiers().isShiftDown() ? 10 : 1;
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey) { update (percent + step); return true; }
    if (code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey) { update (percent - step); return true; }
    return false;
}

void ScaleControl::acceptInput()
{
    const auto text = input.getText();
    input.setVisible (false);
    if (text.isNotEmpty()) update (text.getIntValue());
}
}
