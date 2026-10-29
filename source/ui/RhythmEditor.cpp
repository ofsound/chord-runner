#include "RhythmEditor.h"

namespace chordrunner::ui
{
    namespace
    {
        const juce::Colour accent (0xff83dabb), ink (0xffe9ecee), muted (0xff94a4ae);
    }
    void CycleLane::StepButton::paintButton (juce::Graphics& g, bool hover, bool down)
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool preview = owner->bitDragging && owner->dragTo >= 0;
        const bool on = preview && index == owner->dragTo ? true : preview && index == owner->dragFrom ? false
                                                                                                       : getToggleState();
        auto fill = on ? accent : inCycle ? juce::Colour (0xff233138)
                                          : juce::Colour (0xff10171b);
        if (flashing)
            fill = ink;
        if (isEnabled() && (hover || down))
            fill = fill.brighter (0.12f);
        g.setColour (fill.withMultipliedAlpha (isEnabled() ? 1.0f : 0.4f));
        g.fillRoundedRectangle (r, 3);
        g.setColour ((on ? accent.brighter (0.2f) : juce::Colour (0xff41525b)).withAlpha (inCycle ? 0.5f : 0.2f));
        g.drawRoundedRectangle (r, 3, 0.8f);
        auto font = juce::Font (juce::FontOptions (13.0f, juce::Font::bold));
        const float available = static_cast<float> (juce::jmax (1, getWidth() - 4));
        const float width = juce::GlyphArrangement::getStringWidth (font, getButtonText());
        if (width > available)
            font.setHeight (font.getHeight() * available / width);
        g.setFont (font);
        g.setColour ((on || flashing ? juce::Colour (0xff112322) : inCycle ? muted
                                                                           : muted.withAlpha (0.35f))
                .withMultipliedAlpha (isEnabled() ? 1.0f : 0.4f));
        g.drawText (getButtonText(), getLocalBounds().reduced (2, 1), juce::Justification::centred, false);
    }
    void CycleLane::StepButton::mouseDown (const juce::MouseEvent& event)
    {
        suppressClick = armedMove = false;
        juce::TextButton::mouseDown (event);
        if (isEnabled() && event.mods.isLeftButtonDown())
        {
            owner->startBitDrag (index);
            armedMove = owner->dragFrom >= 0;
        }
    }
    void CycleLane::StepButton::mouseDrag (const juce::MouseEvent& event)
    {
        juce::TextButton::mouseDrag (event);
        owner->updateBitDrag (event);
        suppressClick = suppressClick || owner->bitDragging;
    }
    void CycleLane::StepButton::mouseUp (const juce::MouseEvent& event)
    {
        if (suppressClick || (armedMove && owner->dragFrom < 0))
        {
            if (owner->bitDragging)
                owner->updateBitDrag (event);
            setState (juce::Button::buttonNormal);
            owner->finishBitDrag();
        }
        else
        {
            owner->cancelBitDrag();
            juce::TextButton::mouseUp (event);
        }
    }
    bool CycleLane::StepButton::keyPressed (const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::escapeKey && owner->dragFrom >= 0)
        {
            suppressClick = true;
            owner->cancelBitDrag();
            setState (juce::Button::buttonNormal);
            return true;
        }
        return juce::TextButton::keyPressed (key);
    }
    CycleLane::CycleLane (EditorController& c, bool ties) : controller (c), tieLane (ties)
    {
        setComponentID (ties ? "rhythm.tie" : "rhythm.gate");
        power.setComponentID (getComponentID() + ".enabled");
        power.setButtonText (ties ? "TIE SEQUENCE" : "GATE SEQUENCE");
        addAndMakeVisible (power);
        addAndMakeVisible (handle);
        handle.setComponentID (getComponentID() + ".length");
        power.onClick = [this] { cancelGesture (false); cancelBitDrag(); controller.processor.endEditGesture(); commit(); };
        for (int i = 0; i < 16; ++i)
        {
            auto& cell = cells[static_cast<size_t> (i)];
            cell.owner = this;
            cell.index = i;
            cell.setButtonText (juce::String (i + 1));
            cell.setComponentID (getComponentID() + "." + juce::String (i + 1));
            cell.setClickingTogglesState (true);
            cell.setTooltip ("Toggle a step; drag a lit step to move it on release; click beyond the range to extend");
            cell.onClick = [this, i] {
                if (static_cast<int> (controller.processor.getTransportState()["revision"]) != static_cast<int> (controller.state["revision"]))
                {
                    controller.refresh();
                    refresh();
                    return;
                }
                if (i >= length)
                    length = i + 1;
                else
                    mask ^= 1 << i;
                controller.processor.endEditGesture();
                commit();
            };
            addAndMakeVisible (cell);
        }
        refresh();
    }
    CycleLane::~CycleLane() { cancelGesture(); }
    void CycleLane::refresh()
    {
        const int revision = static_cast<int> (controller.state["revision"]);
        const int pattern = static_cast<int> (controller.state["selectedPattern"]);
        if (handle.dragging && (revision != gestureRevision || pattern != gesturePattern))
            cancelGesture (false);
        if (dragFrom >= 0 && (revision != dragRevision || pattern != dragPattern))
            cancelBitDrag();
        const auto state = controller.state["arp"];
        length = static_cast<int> (state[tieLane ? "tieLength" : "gateLength"]);
        mask = static_cast<int> (state[tieLane ? "tieMask" : "gateMask"]);
        power.setToggleState (static_cast<bool> (state[tieLane ? "tieSequenceEnabled" : "gateSequenceEnabled"]), juce::dontSendNotification);
        const auto rhythmStep = static_cast<juce::uint64> (static_cast<juce::int64> (controller.transport["arpPulseRhythmStep"]));
        const double age = static_cast<double> (controller.transport["arpPulseAgeMs"]);
        const bool flash = static_cast<juce::int64> (controller.transport["arpPulseSerial"]) > 0 && age >= 0 && age < 100 && power.getToggleState();
        for (int i = 0; i < 16; ++i)
        {
            auto& cell = cells[static_cast<size_t> (i)];
            cell.inCycle = i < length;
            cell.flashing = flash && rhythmStep % static_cast<juce::uint64> (length) == static_cast<juce::uint64> (i);
            cell.setToggleState (i < length && (mask & (1 << i)) != 0, juce::dontSendNotification);
            cell.setAlpha (power.getToggleState() ? 1.0f : 0.6f);
            cell.repaint();
        }
        resized();
        repaint();
    }
    void CycleLane::commit()
    {
        if (handle.dragging && (gestureRevision != static_cast<int> (controller.processor.getTransportState()["revision"]) || gesturePattern != controller.processor.selectedPattern()))
        {
            cancelGesture (false);
            controller.refresh();
            refresh();
            return;
        }
        auto next = controller.processor.getTimelineState();
        auto* arp = next["arp"].getDynamicObject();
        arp->setProperty (tieLane ? "tieLength" : "gateLength", length);
        arp->setProperty (tieLane ? "tieMask" : "gateMask", mask);
        arp->setProperty (tieLane ? "tieSequenceEnabled" : "gateSequenceEnabled", power.getToggleState());
        const auto result = controller.processor.editTimeline (next, tieLane ? "tie cycle" : "gate cycle");
        controller.error = result.getErrorMessage();
        if (handle.dragging && result.wasOk())
        {
            gestureRevision = static_cast<int> (controller.processor.getTransportState()["revision"]);
            gestureChanged = true;
        }
        controller.refresh();
        refresh();
    }
    void CycleLane::beginGesture()
    {
        controller.processor.beginEditGesture();
        gestureChanged = false;
        gestureRevision = static_cast<int> (controller.state["revision"]);
        gesturePattern = static_cast<int> (controller.state["selectedPattern"]);
        handle.dragging = true;
    }
    void CycleLane::endGesture()
    {
        handle.dragging = false;
        gestureChanged = false;
        controller.processor.endEditGesture();
        handle.repaint();
    }
    void CycleLane::cancelGesture (bool restore)
    {
        if (!handle.dragging)
            return;
        const bool undo = restore && gestureChanged
                          && gestureRevision == static_cast<int> (controller.processor.getTransportState()["revision"])
                          && gesturePattern == controller.processor.selectedPattern();
        endGesture();
        if (undo)
        {
            controller.processor.undo();
            controller.refresh();
        }
    }
    void CycleLane::startBitDrag (int index)
    {
        cancelBitDrag();
        if (!isEnabled() || index >= length || (mask & (1 << index)) == 0)
            return;
        dragFrom = dragTo = index;
        dragRevision = static_cast<int> (controller.state["revision"]);
        dragPattern = static_cast<int> (controller.state["selectedPattern"]);
    }
    void CycleLane::updateBitDrag (const juce::MouseEvent& event)
    {
        if (dragFrom < 0 || event.getDistanceFromDragStart() < 4)
            return;
        bitDragging = true;
        dragTo = -1;
        const auto point = event.getEventRelativeTo (this).position;
        for (int i = 0; i < length; ++i)
            if (cells[static_cast<size_t> (i)].getBounds().toFloat().contains (point))
                dragTo = i;
        for (auto& cell : cells)
            cell.repaint();
        repaint();
    }
    void CycleLane::finishBitDrag()
    {
        const bool valid = isEnabled() && dragFrom >= 0 && dragTo >= 0 && dragFrom != dragTo
                           && dragRevision == static_cast<int> (controller.processor.getTransportState()["revision"])
                           && dragPattern == controller.processor.selectedPattern();
        if (valid)
        {
            mask = (mask & ~(1 << dragFrom)) | (1 << dragTo);
            controller.processor.beginEditGesture();
            // Clear the preview before refresh can invalidate the drag's revision.
            cancelBitDrag();
            commit();
            controller.processor.endEditGesture();
        }
        else
            cancelBitDrag();
    }
    void CycleLane::cancelBitDrag()
    {
        dragFrom = dragTo = -1;
        bitDragging = false;
        for (auto& cell : cells)
            cell.repaint();
        repaint();
    }
    void CycleLane::paint (juce::Graphics& g)
    {
        if (inlineLane)
        {
            g.setColour (juce::Colour (0xff151e23));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 6);
        }
        const float alpha = (isEnabled() ? 1.0f : 0.35f) * (power.getToggleState() ? 1.0f : 0.5f);
        juce::Path lit;
        for (int row = 0; row < 16 / columns(); ++row)
        {
            const int first = row * columns(), last = juce::jmin (length, first + columns()) - 1;
            if (last < first)
                continue;
            const auto range = cells[static_cast<size_t> (first)].getBounds().getUnion (cells[static_cast<size_t> (last)].getBounds()).toFloat().expanded (3);
            g.setColour (accent.withAlpha (0.08f * alpha));
            g.fillRoundedRectangle (range, 6);
            g.setColour (accent.withAlpha (0.42f * alpha));
            g.drawRoundedRectangle (range, 6, 1);
        }
        for (int i = 0; i < length; ++i)
        {
            const bool preview = bitDragging && dragTo >= 0;
            const bool on = preview && i == dragTo ? true : preview && i == dragFrom ? false
                                                                                     : (mask & (1 << i)) != 0;
            if (on)
                lit.addRoundedRectangle (cells[static_cast<size_t> (i)].getBounds().toFloat().reduced (1), 3);
        }
        if (power.getToggleState() && isEnabled())
            cellGlow.render (g, lit);
    }
    void CycleLane::resized()
    {
        const int count = columns();
        const int rows = 16 / count;
        cellStride = juce::jmax (1, (getWidth() - 20) / count);
        cellTop = inlineLane ? 27 : 32;
        rowStride = inlineLane ? juce::jmax (20, juce::jmin (32, (getHeight() - 30) / rows))
                               : juce::jmax (20, juce::jmin (48, (getHeight() - 58) / rows));
        power.setBounds (4, 0, getWidth() - 8, inlineLane ? 24 : 30);
        for (int i = 0; i < 16; ++i)
            cells[static_cast<size_t> (i)].setBounds (10 + i % count * cellStride, cellTop + i / count * rowStride, cellStride - 4, rowStride - 3);
        const auto end = cells[static_cast<size_t> (length - 1)].getBounds();
        handle.setBounds (end.getRight() - 4, end.getY(), 9, end.getHeight());
        handle.toFront (false);
    }
    void CycleLane::Handle::paint (juce::Graphics& g)
    {
        const auto centre = getLocalBounds().toFloat().getCentre();
        g.setColour ((dragging || isMouseOver() ? accent : ink).withMultipliedAlpha (isEnabled() ? 1.0f : 0.35f));
        for (int dot = -2; dot <= 2; ++dot)
            g.fillEllipse (centre.x - 0.6f, centre.y + static_cast<float> (dot) * 4, 1.2f, 1.2f);
    }
    void CycleLane::Handle::mouseDown (const juce::MouseEvent& event)
    {
        if (!isEnabled() || !event.mods.isLeftButtonDown())
            return;
        if (isShowing())
            grabKeyboardFocus();
        owner.beginGesture();
        repaint();
    }
    void CycleLane::Handle::mouseDrag (const juce::MouseEvent& event)
    {
        if (!dragging)
            return;
        if (!isEnabled() || owner.gestureRevision != static_cast<int> (owner.controller.processor.getTransportState()["revision"])
            || owner.gesturePattern != owner.controller.processor.selectedPattern())
        {
            owner.cancelGesture (false);
            return;
        }
        const auto point = event.getEventRelativeTo (&owner).position;
        const int row = juce::jlimit (0, 16 / owner.columns() - 1, static_cast<int> (std::floor ((point.y - owner.cellTop) / owner.rowStride)));
        const int count = juce::jlimit (1, 16, row * owner.columns() + static_cast<int> (std::ceil ((point.x - 8) / owner.cellStride)));
        if (count != owner.length)
        {
            owner.length = count;
            owner.commit();
        }
    }
    void CycleLane::Handle::mouseUp (const juce::MouseEvent&)
    {
        if (dragging)
            owner.endGesture();
    }
    bool CycleLane::Handle::keyPressed (const juce::KeyPress& key)
    {
        if (!isEnabled())
            return false;
        if (key == juce::KeyPress::escapeKey && dragging)
        {
            owner.cancelGesture();
            return true;
        }
        const int delta = key == juce::KeyPress::leftKey ? -1 : key == juce::KeyPress::rightKey ? 1
                                                                                                : 0;
        if (delta == 0 || dragging)
            return false;
        owner.beginGesture();
        owner.length = juce::jlimit (1, 16, owner.length + delta);
        owner.commit();
        owner.endGesture();
        return true;
    }
}
