#pragma once
#include "ScaleControl.h"
#include "ChordCompletion.h"
#include "SequenceLoopBar.h"
#include "TechnicalBackdrop.h"
#include "VelocityEnvelopeView.h"
#include "EditorController.h"
#include "ModulePower.h"
#include "RhythmEditor.h"
#include "ArpVisualizer.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace chordrunner::ui
{
class TimelineView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit TimelineView (EditorController&);
    ~TimelineView() override;
    void resized() override
    {
        finishDrag();
    }
    void visibilityChanged() override
    {
        if (!isShowing())
            finishDrag();
    }
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
    melatonin::DropShadow attackGlow { { accentYellow.withAlpha (0.6f), 12 }, { accentYellow.withAlpha (0.8f), 4 } };
    int gridLow = 48, gridHigh = 84, dragLow = 48, dragHigh = 84;
    int hoverChord = -1, hoverPitch = -1;
    int dragSource = -1;
    int dragTone = -1, dragPitch = 60, dragRevision = -1, draggedPitch = 60;
    float dragStep = 8.0f;
    bool dragChanged = false;
    int dragMode = 0, selectionRevision = -1;
    double dragTotal = 16.0, dragPixels = 800.0;
    juce::Point<float> dragOrigin;
    juce::Rectangle<float> marquee;
    std::vector<int> selectedNotes, marqueeBase;
    std::vector<chordrunner::NoteEdit> dragNotes;
    juce::Rectangle<float> noteBounds (int chord, int tone) const;
    int hitNote (juce::Point<float>) const;
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
    static constexpr int headerHeight = 32;
    static constexpr int verticalMargin = 16, moduleGap = 16;
    explicit ModuleCard (juce::String name) : title (std::move (name)) {}
    void paint (juce::Graphics&) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (event.y < headerHeight && onHeaderDoubleClick) onHeaderDoubleClick();
    }
    std::function<void()> onHeaderDoubleClick;
    void setTitleText (juce::String value) { title = std::move (value); repaint(); }
private:
    juce::String title;
    // Keep the blur caches across paints; only geometry/scale changes rebuild them.
    melatonin::InnerShadow rimLight { juce::Colours::white.withAlpha (0.18f), 4, { 0, 1 } };
    melatonin::InnerShadow rimShade { juce::Colours::black.withAlpha (0.28f), 7, { 0, -2 } };
    melatonin::DropShadow titleShade { juce::Colours::black.withAlpha (0.75f), 3, { 0, 1 } };
    melatonin::DropShadow titleEdge { juce::Colours::white.withAlpha (0.3f), 2 };
    melatonin::DropShadow headerLift { juce::Colours::black.withAlpha (0.32f), 5, { 0, 2 } };
};

class PerformanceLook final : public juce::LookAndFeel_V4
{
public:
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawGroupComponentOutline (juce::Graphics&, int, int, const juce::String&, const juce::Justification&, juce::GroupComponent&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
};

class PatternPad final : public juce::TextButton
{
public:
    int slot = 0;
    bool active = false, queued = false, selected = false, copyArmed = false;
    juce::ModifierKeys clickModifiers;
    void clicked (const juce::ModifierKeys& mods) override { clickModifiers = mods; }
    bool keyPressed (const juce::KeyPress& key) override;
    std::function<void()> onReset;
    void paintButton (juce::Graphics&, bool, bool) override;
};

class VoicingSelector final : public juce::Component
{
public:
    VoicingSelector();
    void resized() override;
    void setMode (int);
    std::function<void (int)> onChange;

private:
    juce::Label caption;
    std::array<juce::TextButton, 4> buttons;
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
    std::function<void()> preferredSizeChanged;
    void showUiScalePercent (int value) { lastUiScalePercent = value; scaleControl.setValue (value); }
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
        bool compact = false;
        juce::Label label;
        ValueSlider slider;
        juce::TextButton decrease { "-" }, increase { "+" };
    };
    struct RotateControl final : juce::Component
    {
        RotateControl();
        void resized() override;
        juce::Label label;
        juce::TextButton previous, next;
    };
    struct ChoiceControl : juce::Component
    {
        ChoiceControl (const juce::String&, const juce::String&, const juce::StringArray&);
        void resized() override;
        juce::Label label;
        juce::ComboBox combo;
    };
    struct RetriggerControl final : juce::Component
    {
        RetriggerControl();
        void resized() override;
        void setSelected (int);
        juce::Label label;
        std::array<juce::TextButton, 3> buttons;
        std::function<void (int)> onChange;
    };
    struct ButtonPairControl final : juce::Component
    {
        ButtonPairControl (const juce::String&, const juce::String&, const juce::StringArray&, int);
        void resized() override;
        void setSelected (int);
        std::array<juce::TextButton, 2> buttons;
        std::function<void (int)> onChange;
        int preferredWidth;
        void setHeaderMode (bool, const juce::StringArray& = {});
        juce::TextButton header;
        juce::StringArray headerTexts;
        int selectedIndex = 0;
        bool headerMode = false;
    };
    struct RateControl final : ChoiceControl
    {
        using ChoiceControl::ChoiceControl;
        void resized() override;
        void setRotary (bool value) { rotary = value; knob.setVisible (value); resized(); }
        juce::Slider knob;
        bool rotary = false;
    };
    struct PhraseControl final : juce::Component
    {
        PhraseControl();
        ~PhraseControl() override;
        void resized() override;
        void enablementChanged() override { if (! isEnabled()) finishDrag(); }
        void visibilityChanged() override { if (! isVisible()) finishDrag(); }
        void setSelected (int);
        std::function<void (int)> onChange;
        std::function<void()> onGestureStart, onGestureEnd;
        void beginDrag (int source);
        void finishDrag();
        void selectAt (juce::Point<float>);
        void choose (size_t);
        bool dragging = false;
        int dragSource = -1, selectedMode = 0;
        static constexpr std::array<int, 9> modes { 0, 1, 2, 3, 4, 5, 6, 8, 9 };
        struct ModeButton final : juce::TextButton
        {
            PhraseControl* owner = nullptr;
            void mouseDown (const juce::MouseEvent&) override;
            void mouseDrag (const juce::MouseEvent&) override;
            void mouseUp (const juce::MouseEvent&) override;
        };
        std::array<ModeButton, modes.size()> buttons;
    };
    PerformanceLook look;
    TechnicalBackdrop backdrop;
    std::array<melatonin::InnerShadow, 2> chromeLight {
        melatonin::InnerShadow { juce::Colours::white.withAlpha (0.12f), 7, { 0, 1 } },
        melatonin::InnerShadow { juce::Colours::white.withAlpha (0.12f), 7, { 0, 1 } }
    };
    std::array<melatonin::InnerShadow, 2> chromeShade {
        melatonin::InnerShadow { juce::Colours::black.withAlpha (0.65f), 10, { 0, -2 } },
        melatonin::InnerShadow { juce::Colours::black.withAlpha (0.65f), 10, { 0, -2 } }
    };
    ScaleControl scaleControl;
    int lastUiScalePercent = -1;
    juce::Label footerTip;
    struct MidiActivity final : juce::Component
    {
        void paint (juce::Graphics&) override;
        void update (double inputTime, double outputTime);
        float input = 0.0f, output = 0.0f;
    } midiActivity;
    struct TipsButton final : juce::TextButton
    {
        void paintButton (juce::Graphics&, bool, bool) override;
    } tipsButton;
    juce::Component content;
    std::array<juce::TextButton, 5> phoneTabs;
    RhythmEditor rhythmPanel { controller };
    juce::TextButton phonePreview { "Show preview" };
    juce::Label performanceInfo;
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
    int layoutModules = -1, sequenceFold = 0, arpFold = 0;
    bool sequenceFolded = false, arpFolded = false;
    ModuleCard sequenceModule { "SEQUENCER" }, arpModule { "ARPEGGIATOR" };
    std::array<PatternPad, 16> pads;
    struct IconButton final : juce::TextButton
    {
        using juce::TextButton::TextButton;
        void paintButton (juce::Graphics&, bool, bool) override;
    };
    juce::TextButton projectTitle { "Untitled Project" }, projectMenu { "Project" };
    IconButton undoButton { "Undo" }, redoButton { "Redo" };
    VoicingSelector voicingSelector;
    juce::TextButton mutePad { "M" };
    ChoiceControl launch { "launch", "Launch", { "Immediate", "Next 1/16", "Next beat", "Next bar", "Pattern end" } };
    juce::TextButton open { "Open" }, save { "Save" };
    struct DictionaryButton final : juce::TextButton
    {
        void paintButton (juce::Graphics&, bool, bool) override;
    } glossary;
    ModulePower sequencer { "SEQ power" }, arpeggiator { "ARP power" };
    SequenceLoopBar loopBar { controller };
    ChordInput progression;
    ChordCompletion completion { progression };
    IconButton apply { "Apply" };
    juce::TextButton example { "Try Dm9 - G13b9 - Cmaj9" };
    juce::Label message;
    IconButton play { "Play" };
    juce::TextButton panicButton { "MIDI PANIC" };
    NumberControl tempo { "tempo", "Tempo", 20, 300, 1, 120, " BPM" };
    ChoiceControl clock { "clock", "Clock", { "Internal", "Host sync" } };
    ChoiceControl octave { "octave", "Octave", { "0", "1", "2", "3", "4", "5", "6", "7", "8" } };
    NumberControl velocity { "velocity", "Velocity", 1, 127, 1, 100, "" };
    PhraseControl style;
    ButtonPairControl rateMode { "arp.sync", "Rate mode", { "Sync", "Free" }, 200 };
    RateControl rate { "arp.rate", "Rate", { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/4 triplet", "1/8 triplet", "1/16 triplet", "1/8 dotted", "1/16 dotted" } };
    NumberControl freeRate { "arp.freeMs", "Rate", 10, 2000, 0.1, 100, " ms" };
    NumberControl gate { "arp.gate", "Gate", 1, 200, 0.1, 100, " %" };
    NumberControl swing { "arp.swing", "Swing", 0, 100, 1, 0, " %" };
    ButtonPairControl swingSubdivision { "arp.swingSubdivision", "Swing grid", { "1x", "2x" }, 200 };
    NumberControl velocityHumanize { "arp.velocityHumanize", "Velocity humanize", 0, 100, 1, 0, " %" };
    NumberControl timingHumanize { "arp.timingHumanize", "Timing humanize", 0, 100, 1, 0, " %" };
    ButtonPairControl octaveOrder { "arp.octaveMode", "Octave order", { "Note", "Phrase" }, 200 };
    NumberControl steps { "arp.steps", "Added Octaves", 0, 8, 1, 0, "" };
    RotateControl offset;
    NumberControl repeats { "arp.repeats", "Repeats", 1, 8.1, 0.1, 8.1, "" };
    RetriggerControl retrigger;
    NumberControl interval { "arp.interval", "Every", 0.0625, 64, 0.0625, 4, " beats" };
    ModulePower envelopePower { "Velocity envelope power" };
    VelocityEnvelopeView envelopeDisplay;
    struct LatchButton final : juce::TextButton
    {
        LatchButton() : juce::TextButton ("Latch") {}
        void paintButton (juce::Graphics&, bool, bool) override;
        melatonin::DropShadow glow { accentYellow.withAlpha (0.45f), 4 };
    } hold;
    std::array<juce::GroupComponent, 3> groups;
    juce::GroupComponent retriggerGroup, humanizeGroup;
    std::vector<NumberControl*> numbers;
    juce::Component::SafePointer<juce::DialogWindow> glossaryWindow;
    int startStepCount = 4;
    int lastRevision = -1;
    bool syncing = false, projectModified = false;
    juce::var bankState;
    void showProjectMenu();
    int copySource = -1, copyRevision = -1;
    void showProjectDetails();
    void timerCallback() override;
    void bind (NumberControl&, const juce::Identifier&, bool arp = false);
    void bind (ChoiceControl&, const juce::Identifier&, bool arp = false, int offsetValue = 0);
    void showGlossary();
    void updateTimelineSize();
};
}
