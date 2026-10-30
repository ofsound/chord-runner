#pragma once
#include "EditorController.h"
#include "UiPalette.h"
#include "ModulePower.h"
#include <melatonin_blur/melatonin_blur.h>

namespace chordrunner::ui
{
    class CycleLane final : public juce::Component
    {
    public:
        CycleLane (EditorController&, bool ties);
        ~CycleLane() override;
        void refresh();
        void resized() override;
        void paint (juce::Graphics&) override;
        void setInline (bool value)
        {
            inlineLane = value;
            resized();
        }

    private:
        bool inlineLane = false;
        int columns() const { return inlineLane ? (getWidth() >= 450 ? 16 : 8) : getWidth() < 380 ? 4
                                                                             : getWidth() < 700   ? 8
                                                                                                  : 16; }
        EditorController& controller;
        bool tieLane;
        int length = 4, mask = 15;
        ModulePower power { "Sequence power" };
        struct StepButton final : juce::TextButton
        {
            CycleLane* owner = nullptr;
            int index = 0;
            bool inCycle = true, flashing = false, suppressClick = false, armedMove = false;
            void mouseDown (const juce::MouseEvent&) override;
            void mouseDrag (const juce::MouseEvent&) override;
            void mouseUp (const juce::MouseEvent&) override;
            bool keyPressed (const juce::KeyPress&) override;
            void paintButton (juce::Graphics&, bool, bool) override;
        };
        std::array<StepButton, 16> cells;
        struct Handle final : juce::Component, juce::SettableTooltipClient
        {
            explicit Handle (CycleLane& lane) : owner (lane)
            {
                setWantsKeyboardFocus (true);
                setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
                setTooltip ("Drag the range edge to set length (1-16); arrow keys adjust; Escape cancels");
            }
            void paint (juce::Graphics&) override;
            void enablementChanged() override
            {
                const bool active = isEnabled();
                setMouseCursor (active ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
                setWantsKeyboardFocus (active);
                setInterceptsMouseClicks (active, false);
                if (!active) owner.cancelGesture (false);
                repaint();
            }
            void mouseEnter (const juce::MouseEvent&) override { repaint(); }
            void mouseExit (const juce::MouseEvent&) override { repaint(); }
            void mouseDown (const juce::MouseEvent&) override;
            void mouseDrag (const juce::MouseEvent&) override;
            void mouseUp (const juce::MouseEvent&) override;
            bool keyPressed (const juce::KeyPress&) override;
            CycleLane& owner;
            bool dragging = false;
        } handle { *this };
        melatonin::DropShadow cellGlow { { accentYellow.withAlpha (0.35f), 6 },
            { accentYellow.withAlpha (0.3f), 2 } };
        int cellStride = 1, cellTop = 27, rowStride = 30;
        int dragFrom = -1, dragTo = -1, dragRevision = -1, dragPattern = -1;
        bool bitDragging = false, gestureChanged = false;
        int gestureRevision = -1, gesturePattern = -1;
        void startBitDrag (int);
        void updateBitDrag (const juce::MouseEvent&);
        void finishBitDrag();
        void cancelBitDrag();
        void beginGesture();
        void cancelGesture (bool restore = true);
        void endGesture();
        void commit();
    };
    class RhythmEditor final : public juce::Component
    {
    public:
        explicit RhythmEditor (EditorController& c) : gate (c, false), tie (c, true)
        {
            addAndMakeVisible (gate);
            addAndMakeVisible (tie);
            addChildComponent (gateTab);
            addChildComponent (tieTab);
            gateTab.onClick = [this] { showTie = false; resized(); };
            tieTab.onClick = [this] { showTie = true; resized(); };
            setSize (720, 260);
        }
        void setHorizontal (bool value)
        {
            horizontal = value;
            gate.setInline (value);
            tie.setInline (value);
            resized();
        }
        void resized() override
        {
            auto area = getLocalBounds().reduced (8);
            const bool compact = !horizontal && getWidth() < 700 && getHeight() < 240;
            gateTab.setVisible (compact);
            tieTab.setVisible (compact);
            if (compact)
            {
                auto tabs = area.removeFromTop (36);
                gateTab.setBounds (tabs.removeFromLeft (tabs.getWidth() / 2).withTrimmedRight (4));
                tieTab.setBounds (tabs);
                gateTab.setToggleState (!showTie, juce::dontSendNotification);
                tieTab.setToggleState (showTie, juce::dontSendNotification);
                gate.setBounds (area);
                tie.setBounds (area);
                gate.setVisible (!showTie);
                tie.setVisible (showTie);
            }
            else
            {
                gate.setVisible (true);
                tie.setVisible (true);
                if (horizontal)
                {
                    gate.setBounds (area.removeFromLeft (area.getWidth() / 2).withTrimmedRight (6));
                    tie.setBounds (area.withTrimmedLeft (6));
                }
                else
                {
                    gate.setBounds (area.removeFromTop (area.getHeight() / 2));
                    tie.setBounds (area);
                }
            }
        }
        void refresh()
        {
            gate.refresh();
            tie.refresh();
        }

    private:
        CycleLane gate, tie;
        juce::TextButton gateTab { "Gate" }, tieTab { "Tie" };
        bool showTie = false, horizontal = false;
    };
}
