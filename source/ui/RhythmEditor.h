#pragma once
#include "EditorController.h"

namespace chordrunner::ui
{
class CycleLane final : public juce::Component
{
public:
    CycleLane (EditorController&, bool ties);
    void refresh();
    void resized() override;
private:
    EditorController& controller;
    bool tieLane;
    int length = 16, mask = 0;
    juce::ToggleButton power;
    std::array<juce::TextButton, 16> cells;
    struct Handle final : juce::Component
    {
        explicit Handle (CycleLane& lane) : owner (lane) { setWantsKeyboardFocus (true); setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); }
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        bool keyPressed (const juce::KeyPress&) override;
        CycleLane& owner;
        juce::Point<float> origin;
        int originalLength = 16;
    } handle { *this };
    void commit();
};
class RhythmEditor final : public juce::Component
{
public:
    explicit RhythmEditor (EditorController& c) : gate (c, false), tie (c, true)
    {
        addAndMakeVisible (gate); addAndMakeVisible (tie);
        addChildComponent (gateTab); addChildComponent (tieTab);
        gateTab.onClick = [this] { showTie = false; resized(); };
        tieTab.onClick = [this] { showTie = true; resized(); };
        setSize (720, 260);
    }
    void resized() override
    {
        auto area = getLocalBounds().reduced (8);
        const bool compact = getWidth() < 700 && getHeight() < 240;
        gateTab.setVisible (compact); tieTab.setVisible (compact);
        if (compact)
        {
            auto tabs = area.removeFromTop (36);
            gateTab.setBounds (tabs.removeFromLeft (tabs.getWidth() / 2).withTrimmedRight (4));
            tieTab.setBounds (tabs);
            gateTab.setToggleState (! showTie, juce::dontSendNotification);
            tieTab.setToggleState (showTie, juce::dontSendNotification);
            gate.setBounds (area); tie.setBounds (area);
            gate.setVisible (! showTie); tie.setVisible (showTie);
        }
        else
        {
            gate.setVisible (true); tie.setVisible (true);
            gate.setBounds (area.removeFromTop (area.getHeight() / 2));
            tie.setBounds (area);
        }
    }
    void refresh() { gate.refresh(); tie.refresh(); }
private:
    CycleLane gate, tie;
    juce::TextButton gateTab { "Gate" }, tieTab { "Tie" };
    bool showTie = false;
};
}
