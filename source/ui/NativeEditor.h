#pragma once
#include "EditorController.h"
#include "RhythmEditor.h"
#include "ArpVisualizer.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace chordrunner::ui
{
class TimelineView final : public juce::Component
{
public:
    explicit TimelineView (EditorController&);
    ~TimelineView() override;
    void resized() override { finishDrag(); }
    void visibilityChanged() override { if (! isShowing()) finishDrag(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    int selectedChord() const { return selected; }

private:
    EditorController& controller;
    int selected = 0, previewRevision = -1;
    std::vector<chordrunner::PreviewNote> outputPreview;
    int gridLow = 48, gridHigh = 84, dragLow = 48, dragHigh = 84;
    int hoverChord = -1, hoverPitch = -1;
    int dragSource = -1;
    int dragTone = -1, dragPitch = 60, dragRevision = -1, draggedPitch = 60;
    float dragY = 0.0f, dragStep = 8.0f;
    bool dragChanged = false;
    void finishDrag();
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

class ModuleCard final : public juce::Component
{
public:
    explicit ModuleCard (juce::String name) : title (std::move (name)) {}
    void paint (juce::Graphics&) override;
    void setTitleText (juce::String value) { title = std::move (value); repaint(); }
private:
    juce::String title;
};

class PerformanceLook final : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawGroupComponentOutline (juce::Graphics&, int, int, const juce::String&, const juce::Justification&, juce::GroupComponent&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
};

class PatternPad final : public juce::TextButton
{
public:
    int slot = 0;
    bool active = false, queued = false, selected = false;
    void paintButton (juce::Graphics&, bool, bool) override;
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
    std::function<void (int)> chooseProject;
    void sync();

private:
    struct NumberControl final : juce::Component
    {
        NumberControl (const juce::String&, const juce::String&, double, double, double, double, const juce::String&);
        void resized() override;
        struct ValueSlider final : juce::Slider
        {
            bool keyPressed (const juce::KeyPress&) override;
        };
        void refreshStepper();
        juce::Label label;
        ValueSlider slider;
        juce::TextButton decrease { "-" }, increase { "+" };
    };
    struct ChoiceControl final : juce::Component
    {
        ChoiceControl (const juce::String&, const juce::String&, const juce::StringArray&);
        void resized() override;
        juce::Label label;
        juce::ComboBox combo;
    };
    PerformanceLook look;
    juce::TooltipWindow tooltips { this, 650 };
    juce::Component content;
    std::array<juce::TextButton, 5> phoneTabs;
    RhythmEditor rhythmPanel { controller };
    juce::TextButton rhythmButton { "Gate / Tie" }, phonePreview { "Show preview" };
    juce::Component::SafePointer<RhythmEditor> rhythmPopup;
    juce::Label performanceInfo, pitchHint, humanizeHint;
    int phonePage = 0;
    bool phoneLayout = false;
    void layoutPhone();
    void restoreDesktopParents();
    juce::Viewport viewport, timelineViewport;
    TimelineView timeline;
    ArpVisualizer arpVisualizer;
    bool arpOnlyLayout = false;
    double lastArpPreviewTempo = 0;
    int lastArpPreviewHeld = -1;
    ModuleCard sequenceModule { "SEQUENCE" }, arpModule { "ARPEGGIATOR" };
    std::array<PatternPad, 16> pads;
    juce::TextButton projectTitle { "Untitled Project" }, projectMenu { "Project" }, undoButton { "Undo" }, redoButton { "Redo" };
    juce::TextButton voicingButton { "Root position" };
    juce::TextButton mutePad { "M" }, fit { "Fit" }, patternMenu { "Pattern" };
    juce::Label patternName;
    ChoiceControl launch { "launch", "Launch", { "Immediate", "Next 1/16", "Next beat", "Next bar", "Pattern end" } };
    juce::TextButton open { "Open" }, save { "Save" }, glossary { "Chords" };
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
    NumberControl swing { "arp.swing", "Swing", 0, 100, 1, 0, " %" };
    ChoiceControl swingSubdivision { "arp.swingSubdivision", "Swing grid", { "1x", "2x", "4x" } };
    NumberControl velocityHumanize { "arp.velocityHumanize", "Velocity humanize", 0, 100, 1, 0, " %" };
    NumberControl timingHumanize { "arp.timingHumanize", "Timing humanize", 0, 100, 1, 0, " %" };
    NumberControl steps { "arp.steps", "Extra passes", 0, 8, 1, 0, "" };
    NumberControl distance { "arp.distance", "Transpose by", -36, 36, 1, 12, " st" };
    NumberControl offset { "arp.offset", "Offset", 0, 128, 1, 0, "" };
    NumberControl repeats { "arp.repeats", "Repeats", 0, 64, 1, 0, "" };
    ChoiceControl retrigger { "arp.retrigger", "Retrigger", { "Off", "Note", "Beat" } };
    NumberControl interval { "arp.interval", "Restart every", 0.0625, 64, 0.0625, 4, " beats" };
    NumberControl target { "arp.target", "Target", 0, 127, 1, 64, "" };
    NumberControl decay { "arp.decay", "Decay", 1, 60000, 1, 1000, " ms" };
    juce::TextButton hold { "Hold notes" }, velocityEnabled { "Velocity envelope" }, velocityRetrigger { "Retrigger velocity" };
    std::array<juce::GroupComponent, 5> groups;
    std::vector<NumberControl*> numbers;
    juce::Component::SafePointer<juce::DialogWindow> glossaryWindow;
    int lastRevision = -1;
    bool syncing = false, fitNotes = true, projectModified = false;
    juce::var bankState;
    void showProjectMenu();
    void showPatternMenu();
    void showProjectDetails();
    void timerCallback() override;
    void bind (NumberControl&, const juce::Identifier&, bool arp = false);
    void bind (ChoiceControl&, const juce::Identifier&, bool arp = false, int offsetValue = 0);
    void showGlossary();
    void updateTimelineSize();
};
}
