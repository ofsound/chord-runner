#include "RhythmEditor.h"

namespace chordrunner::ui
{
CycleLane::CycleLane (EditorController& c, bool ties) : controller (c), tieLane (ties)
{
    setComponentID (ties ? "rhythm.tie" : "rhythm.gate");
    power.setComponentID (getComponentID() + ".enabled");
    power.setButtonText (ties ? "TIE SEQUENCE" : "GATE SEQUENCE");
    addAndMakeVisible (power);
    addAndMakeVisible (handle);
    handle.setComponentID (getComponentID() + ".length");
    power.onClick = [this] { controller.processor.endEditGesture(); commit(); };
    for (int i = 0; i < 16; ++i)
    {
        auto& cell = cells[static_cast<size_t> (i)];
        cell.setButtonText (juce::String (i + 1));
        cell.setComponentID (getComponentID() + "." + juce::String (i + 1));
        cell.setClickingTogglesState (true);
        cell.onClick = [this, i]
        {
            mask ^= 1 << i;
            controller.processor.endEditGesture();
            commit();
        };
        addAndMakeVisible (cell);
    }
    refresh();
}
void CycleLane::refresh()
{
    const auto state = controller.state["arp"];
    length = static_cast<int> (state[tieLane ? "tieLength" : "gateLength"]);
    mask = static_cast<int> (state[tieLane ? "tieMask" : "gateMask"]);
    power.setToggleState (static_cast<bool> (state[tieLane ? "tieSequenceEnabled" : "gateSequenceEnabled"]), juce::dontSendNotification);
    for (int i = 0; i < 16; ++i)
    {
        auto& cell = cells[static_cast<size_t> (i)];
        cell.setToggleState ((mask & (1 << i)) != 0, juce::dontSendNotification);
        cell.setEnabled (i < length);
        cell.setAlpha (power.getToggleState() ? 1.0f : 0.55f);
    }
    resized(); handle.repaint();
}
void CycleLane::commit()
{
    auto next = controller.processor.getTimelineState();
    auto* arp = next["arp"].getDynamicObject();
    arp->setProperty (tieLane ? "tieLength" : "gateLength", length);
    arp->setProperty (tieLane ? "tieMask" : "gateMask", mask);
    arp->setProperty (tieLane ? "tieSequenceEnabled" : "gateSequenceEnabled", power.getToggleState());
    controller.error = controller.processor.editTimeline (next, tieLane ? "tie cycle" : "gate cycle").getErrorMessage();
    controller.refresh();
    refresh();
}
void CycleLane::resized()
{
    const int columns = getWidth() < 380 ? 4 : getWidth() < 700 ? 8 : 16;
    const int rows = 16 / columns;
    const int stride = juce::jmax (1, (getWidth() - 20) / columns);
    const int rowH = juce::jmax (20, juce::jmin (48, (getHeight() - 58) / rows));
    power.setBounds (4, 0, getWidth() - 8, 30);
    for (int i = 0; i < 16; ++i)
        cells[static_cast<size_t> (i)].setBounds (10 + i % columns * stride, 32 + i / columns * rowH, stride - 3, rowH - 3);
    const auto end = cells[static_cast<size_t> (juce::jlimit (1, 16, length) - 1)].getBounds();
    handle.setBounds (juce::jlimit (0, juce::jmax (0, getWidth() - 38), end.getRight() - 19), 32 + rows * rowH - 3, 38, 26);
}
void CycleLane::Handle::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xffe9bb7c));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("< " + juce::String (owner.length) + " >", getLocalBounds(), juce::Justification::centred);
}
void CycleLane::Handle::mouseDown (const juce::MouseEvent& event)
{
    origin = event.getEventRelativeTo (&owner).position;
    originalLength = owner.length;
    owner.controller.processor.endEditGesture();
}
void CycleLane::Handle::mouseDrag (const juce::MouseEvent& event)
{
    const int columns = owner.getWidth() < 380 ? 4 : owner.getWidth() < 700 ? 8 : 16;
    const auto delta = event.getEventRelativeTo (&owner).position - origin;
    const int stride = juce::jmax (1, (owner.getWidth() - 20) / columns);
    const int newLength = juce::jlimit (1, 16, originalLength + juce::roundToInt (delta.x / static_cast<float> (stride)));
    if (newLength != owner.length) { owner.length = newLength; owner.commit(); }
}
void CycleLane::Handle::mouseUp (const juce::MouseEvent&) { owner.controller.processor.endEditGesture(); }
bool CycleLane::Handle::keyPressed (const juce::KeyPress& key)
{
    if (key != juce::KeyPress::leftKey && key != juce::KeyPress::rightKey) return false;
    owner.length = juce::jlimit (1, 16, owner.length + (key == juce::KeyPress::leftKey ? -1 : 1));
    owner.controller.processor.endEditGesture(); owner.commit();
    return true;
}
}
