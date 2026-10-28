#pragma once
#include "EditorController.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace chordrunner::ui
{
class TimelineView final : public juce::Component
{
public:
    explicit TimelineView (EditorController&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    int selectedChord() const { return selected; }

private:
    EditorController& controller;
    int selected = 0;
};

class ChordGlossary final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit ChordGlossary (std::function<void (juce::String)>);
    void resized() override;
    void paint (juce::Graphics&) override;
    int getNumRows() override;
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override;
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override;
    void returnKeyPressed (int) override;
    void selectedRowsChanged (int) override;

private:
    juce::Label title, hint, count, details;
    juce::TextEditor search;
    juce::ComboBox family, root;
    juce::ListBox list;
    juce::TextButton insert { "Add to draft" }, close { "Close" };
    juce::StringArray families;
    juce::Array<int> matches;
    std::function<void (juce::String)> onInsert;
    void filter();
    void insertSelected();
};

class NativeEditor final : public juce::Component, private juce::Timer, private juce::KeyListener
{
public:
    explicit NativeEditor (PluginProcessor&);
    ~NativeEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    using juce::Component::keyPressed;
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;
    EditorController controller;
    std::function<void (bool)> chooseProject;
    void sync();

private:
    struct NumberControl final : juce::Component
    {
        NumberControl (const juce::String&, const juce::String&, double, double, double, double, const juce::String&);
        void resized() override;
        juce::Label label;
        juce::Slider slider;
    };
    struct ChoiceControl final : juce::Component
    {
        ChoiceControl (const juce::String&, const juce::String&, const juce::StringArray&);
        void resized() override;
        juce::Label label;
        juce::ComboBox combo;
    };
    juce::LookAndFeel_V4 look;
    juce::TooltipWindow tooltips { this, 650 };
    juce::Component content;
    juce::Viewport viewport, timelineViewport;
    TimelineView timeline;
    juce::TextButton open { "Open" }, save { "Save" }, glossary { "Chord glossary" };
    juce::TextButton sequencer { "Sequencer" }, arpeggiator { "Arpeggiator" };
    juce::TextEditor progression;
    juce::TextButton apply { "Apply" }, example { "Try Dm9 - G13b9 - Cmaj9" };
    juce::Label message, status, summary;
    juce::TextButton play { "Play" }, panicButton { "All notes off" };
    NumberControl tempo { "tempo", "Tempo", 20, 300, 1, 120, " BPM" };
    ChoiceControl clock { "clock", "Clock", { "Internal", "Host sync" } };
    ChoiceControl octave { "octave", "Octave", { "0", "1", "2", "3", "4", "5", "6", "7", "8" } };
    NumberControl velocity { "velocity", "Velocity", 1, 127, 1, 100, "" };
    ChoiceControl channel { "channel", "Channel", {} };
    ChoiceControl style { "arp.style", "Style", { "Up", "Down", "Up & Down", "Down & Up", "Converge", "Diverge", "Play Order", "Chord Trigger", "Random", "Random Other", "Random Once" } };
    ChoiceControl rateMode { "arp.sync", "Rate mode", { "Sync", "Free" } };
    ChoiceControl rate { "arp.rate", "Rate", { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/4 triplet", "1/8 triplet", "1/16 triplet", "1/8 dotted", "1/16 dotted" } };
    NumberControl freeRate { "arp.freeMs", "Rate", 10, 2000, 0.1, 100, " ms" };
    NumberControl gate { "arp.gate", "Gate", 1, 200, 0.1, 100, " %" };
    NumberControl swing { "arp.swing", "Swing", 0, 50, 0.1, 0, " %" };
    NumberControl steps { "arp.steps", "Steps", 0, 8, 1, 0, "" };
    NumberControl distance { "arp.distance", "Distance", -36, 36, 1, 12, " st" };
    NumberControl offset { "arp.offset", "Offset", 0, 128, 1, 0, "" };
    NumberControl repeats { "arp.repeats", "Repeats", 0, 64, 1, 0, "" };
    ChoiceControl retrigger { "arp.retrigger", "Retrigger", { "Off", "Note", "Beat" } };
    NumberControl interval { "arp.interval", "Interval", 0.0625, 64, 0.0625, 4, " beats" };
    NumberControl target { "arp.target", "Target", 0, 127, 1, 64, "" };
    NumberControl decay { "arp.decay", "Decay", 1, 60000, 1, 1000, " ms" };
    juce::TextButton hold { "Hold notes" }, velocityEnabled { "Velocity envelope" }, velocityRetrigger { "Retrigger velocity" };
    std::array<juce::GroupComponent, 4> groups;
    std::vector<NumberControl*> numbers;
    juce::Component::SafePointer<juce::DialogWindow> glossaryWindow;
    int lastRevision = -1;
    bool syncing = false;
    void timerCallback() override;
    void bind (NumberControl&, const juce::Identifier&, bool arp = false);
    void bind (ChoiceControl&, const juce::Identifier&, bool arp = false, int offsetValue = 0);
    void showGlossary();
    void updateTimelineSize();
};
}
