#include "NativeEditor.h"

namespace chordrunner::ui
{
namespace
{
const juce::Colour background { 0xff101114 }, surface { 0xff1b1d22 }, border { 0xff393d45 },
    foreground { 0xffe3e5e8 }, muted { 0xffa0a6b0 }, green { 0xff86dfb7 }, amber { 0xffe9bb7c };
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
    label.setFont (juce::FontOptions (12.0f));
}
void identify (juce::Component& component, const juce::String& id, const juce::String& name)
{
    component.setComponentID (id);
    component.setName (name);
    component.setTitle (name);
}
}

TimelineView::TimelineView (EditorController& c) : controller (c)
{
    identify (*this, "timeline", "Chord timeline");
    setWantsKeyboardFocus (true);
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
    lowest = juce::jmax (0, lowest - 2);
    highest = juce::jmin (127, highest + 2);
    constexpr float keyWidth = 46.0f, top = 66.0f;
    const auto width = static_cast<float> (getWidth()) - keyWidth;
    const auto rowHeight = (static_cast<float> (getHeight()) - top) / static_cast<float> (highest - lowest + 1);
    const auto xAt = [total, width] (double beat) { return keyWidth + static_cast<float> (beat / total) * width; };
    g.setFont (juce::FontOptions (10.0f));
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
    const auto active = static_cast<int> (controller.transport["activeChord"]);
    const bool sequenceOn = static_cast<bool> (controller.state["sequencerEnabled"]);
    for (int i = 0; i < chords->size(); ++i)
    {
        const auto& chord = chords->getReference (i);
        const auto x = xAt (static_cast<double> (chord["startBeat"]));
        const auto chordWidth = xAt (static_cast<double> (chord["startBeat"]) + static_cast<double> (chord["beats"])) - x;
        const bool sounding = sequenceOn && i == active;
        g.setColour (sounding ? juce::Colour (0xff234e3d) : i == selected ? juce::Colour (0xff363a42) : surface);
        g.fillRect (x, 24.0f, chordWidth - 1.0f, 42.0f);
        g.setColour (sounding ? green : foreground);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (chord["label"].toString(), juce::Rectangle<float> (x + 6, 26, chordWidth - 10, 21), juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (10.0f));
        g.setColour (muted);
        g.drawText (juce::String (static_cast<double> (chord["beats"]) / 4.0, 2) + " bars",
                    juce::Rectangle<float> (x + 6, 46, chordWidth - 10, 17), juce::Justification::centredLeft);
        if (const auto* notes = chord["notes"].getArray())
            for (const auto& note : *notes)
            {
                const auto y = top + static_cast<float> (highest - static_cast<int> (note)) * rowHeight;
                const juce::Rectangle<float> rect (x + 1, y + 1, juce::jmax (1.0f, chordWidth - 3), juce::jmax (2.0f, rowHeight - 2));
                g.setColour (sounding ? green : juce::Colour (0xff426f5b));
                g.fillRoundedRectangle (rect, 2.0f);
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
    g.setColour (border);
    g.drawVerticalLine (46, 0, static_cast<float> (getHeight()));
    if (sequenceOn && static_cast<bool> (controller.transport["playing"]))
    {
        g.setColour (green);
        g.fillRect (xAt (static_cast<double> (controller.transport["beat"])), 24.0f, 2.0f, static_cast<float> (getHeight()) - 24);
    }
    if (hasKeyboardFocus (false))
    {
        g.setColour (green);
        g.drawRect (getLocalBounds(), 1);
    }
}
void TimelineView::mouseDown (const juce::MouseEvent& event)
{
    const auto beat = (event.position.x - 46.0f) / static_cast<float> (getWidth() - 46) * static_cast<double> (controller.state["totalBeats"]);
    if (const auto* chords = controller.state["chords"].getArray())
        for (int i = 0; i < chords->size(); ++i)
        {
            const auto& chord = chords->getReference (i);
            const auto start = static_cast<double> (chord["startBeat"]);
            if (beat >= start && beat < start + static_cast<double> (chord["beats"]))
                selected = i;
        }
    repaint();
}
bool TimelineView::keyPressed (const juce::KeyPress& key)
{
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
    title.setFont (juce::FontOptions (22.0f));
    labelStyle (hint, "One chord = one bar. C9 is a ninth chord; C9@2 lasts two bars. Use spaces, - or | between chords.\n"
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
    g.setFont (juce::FontOptions (15.0f));
    g.drawText (root.getText() + chord["suffix"].toString(), 12, 4, 150, 23, juce::Justification::centredLeft);
    g.setColour (foreground);
    g.setFont (juce::FontOptions (12.0f));
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
    slider.setChangeNotificationOnlyOnRelease (true);
    slider.setTooltip (name + ": drag to adjust, click the value to type, double-click to reset.");
}
void NativeEditor::NumberControl::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (18));
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
    label.setBounds (area.removeFromTop (18));
    combo.setBounds (area.reduced (0, 2));
}
NativeEditor::NativeEditor (PluginProcessor& processor) : controller (processor), timeline (controller)
{
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
    look.setColour (juce::TextEditor::outlineColourId, border);
    look.setColour (juce::TextEditor::focusedOutlineColourId, green);
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
    for (auto* component : std::initializer_list<juce::Component*> { &open, &save, &glossary, &status, &panicButton })
        addAndMakeVisible (component);
    for (auto* component : std::initializer_list<juce::Component*> { &sequencer, &arpeggiator, &progression, &apply, &example, &message, &play, &tempo, &clock, &octave, &velocity, &channel, &timelineViewport, &summary, &hold })
        content.addAndMakeVisible (component);
    const juce::StringArray groupNames { "PATTERN", "TIMING", "PITCH", "VELOCITY" };
    for (size_t i = 0; i < groups.size(); ++i)
    {
        groups[i].setText (groupNames[static_cast<int> (i)]);
        content.addAndMakeVisible (groups[i]);
    }
    for (auto* component : std::initializer_list<juce::Component*> { &style, &offset, &repeats, &rateMode, &rate, &freeRate, &gate, &swing, &steps, &distance, &retrigger, &interval, &velocityEnabled, &target, &decay, &velocityRetrigger })
        content.addAndMakeVisible (component);
    timelineViewport.setViewedComponent (&timeline, false);
    timelineViewport.setScrollBarsShown (false, true);
    timelineViewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
    identify (open, "open", "Open project");
    identify (save, "save", "Save project");
    identify (glossary, "glossary.open", "Chord glossary");
    identify (apply, "apply", "Apply progression");
    identify (play, "play", "Play or stop");
    identify (panicButton, "panic", "All notes off");
    identify (progression, "progression", "Chord progression");
    progression.setMultiLine (false);
    progression.setInputRestrictions (4096);
    progression.setFont (juce::FontOptions (21.0f));
    progression.setTextToShowWhenEmpty ("C - G - Am - F", muted);
    progression.onTextChange = [this]
    {
        if (! syncing)
        {
            controller.draft = progression.getText();
            controller.error.clear();
            message.setText (controller.dirty() ? "Unapplied changes" : "One chord. One bar. Fixed 4/4.", juce::dontSendNotification);
            message.setColour (juce::Label::textColourId, muted);
        }
    };
    progression.onReturnKey = [this] { controller.applyDraft(); };
    apply.onClick = [this] { controller.applyDraft(); };
    example.onClick = [this] { progression.setText ("Dm9 - G13b9 - Cmaj9"); };
    play.onClick = [this] { controller.togglePlay(); };
    panicButton.onClick = [this] { controller.panic(); };
    open.onClick = [this] { if (chooseProject) chooseProject (false); };
    save.onClick = [this] { if ((! controller.dirty() || controller.applyDraft()) && chooseProject) chooseProject (true); };
    glossary.onClick = [this] { showGlossary(); };
    channel.combo.addItemList ({ "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" }, 1);
    bind (tempo, "tempo");
    bind (velocity, "velocity");
    bind (octave, "octave");
    bind (channel, "channel", false, 1);
    bind (style, "style", true);
    bind (offset, "offset", true);
    bind (repeats, "repeats", true);
    bind (freeRate, "freeMs", true);
    bind (gate, "gate", true);
    bind (swing, "swing", true);
    bind (steps, "steps", true);
    bind (distance, "distance", true);
    bind (retrigger, "retrigger", true);
    bind (interval, "interval", true);
    bind (target, "target", true);
    bind (decay, "decay", true);
    freeRate.slider.setSkewFactorFromMidPoint (200);
    decay.slider.setSkewFactorFromMidPoint (1000);
    repeats.slider.textFromValueFunction = [] (double value) { return value < 0.5 ? juce::String ("Infinite") : juce::String (static_cast<int> (value)); };
    repeats.slider.valueFromTextFunction = [] (const juce::String& text) { return text.containsIgnoreCase ("inf") ? 0.0 : text.getDoubleValue(); };
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
    toggle (sequencer, "sequencerEnabled", false);
    toggle (arpeggiator, "enabled", true);
    toggle (hold, "hold", true);
    toggle (velocityEnabled, "velocityEnabled", true);
    toggle (velocityRetrigger, "velocityRetrigger", true);
    for (auto* label : { &message, &status, &summary })
        labelStyle (*label, "");
    addKeyListener (this);
    controller.changed = [this] { sync(); };
    sync();
    startTimerHz (25);
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
    const bool sequenceOn = static_cast<bool> (state["sequencerEnabled"]);
    const bool arpOn = static_cast<bool> (arp["enabled"]);
    const bool standalone = controller.processor.isStandalone();
    const bool host = ! standalone && static_cast<bool> (state["hostSync"]);
    const int revision = controller.refreshSerial;
    if (lastRevision != revision)
    {
        lastRevision = revision;
        if (progression.getText() != controller.draft)
            progression.setText (controller.draft, false);
        for (auto* control : numbers)
        {
            const auto id = control->slider.getComponentID();
            const auto value = id.startsWith ("arp.") ? arp[juce::Identifier (id.substring (4))] : state[juce::Identifier (id)];
            // A host restore must not move the thumb underneath a user's active drag.
            if (! control->slider.isMouseButtonDown())
                control->slider.setValue (static_cast<double> (value), juce::dontSendNotification);
        }
        octave.combo.setSelectedId (static_cast<int> (state["octave"]) + 1, juce::dontSendNotification);
        channel.combo.setSelectedId (static_cast<int> (state["channel"]), juce::dontSendNotification);
        clock.combo.setSelectedId (host ? 2 : 1, juce::dontSendNotification);
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
        updateTimelineSize();
    }
    // Draft changes and validation failures do not increment the processor revision.
    if (! progression.hasKeyboardFocus (true) && progression.getText() != controller.draft)
        progression.setText (controller.draft, false);
    sequencer.setToggleState (sequenceOn, juce::dontSendNotification);
    arpeggiator.setToggleState (arpOn, juce::dontSendNotification);
    hold.setToggleState (static_cast<bool> (arp["hold"]), juce::dontSendNotification);
    velocityEnabled.setToggleState (static_cast<bool> (arp["velocityEnabled"]), juce::dontSendNotification);
    velocityRetrigger.setToggleState (static_cast<bool> (arp["velocityRetrigger"]), juce::dontSendNotification);
    play.setEnabled (sequenceOn);
    play.setButtonText (static_cast<bool> (transport["armed"]) ? "Stop" : host ? "Arm" : "Play");
    tempo.setEnabled (! host);
    clock.setEnabled (! standalone);
    for (auto* component : std::initializer_list<juce::Component*> { &style, &offset, &repeats, &rateMode, &rate, &freeRate, &gate, &swing, &steps, &distance, &retrigger, &hold, &velocityEnabled })
        component->setEnabled (arpOn);
    interval.setEnabled (arpOn && static_cast<int> (arp["retrigger"]) == 2);
    const bool envelopeOn = arpOn && static_cast<bool> (arp["velocityEnabled"]);
    target.setEnabled (envelopeOn);
    decay.setEnabled (envelopeOn);
    velocityRetrigger.setEnabled (envelopeOn);
    rate.setVisible (static_cast<bool> (arp["sync"]));
    freeRate.setVisible (! static_cast<bool> (arp["sync"]));
    message.setColour (juce::Label::textColourId, controller.error.isNotEmpty() ? juce::Colour (0xffffa5aa) : muted);
    message.setText (controller.error.isNotEmpty() ? controller.error : controller.dirty() ? "Unapplied changes" : "One chord. One bar. Fixed 4/4. Use @ for explicit durations, e.g. C7@2.", juce::dontSendNotification);
    auto transportText = juce::String ("Stopped");
    if (! sequenceOn)
        transportText = arpOn ? host && ! static_cast<bool> (transport["playing"]) ? "Waiting for host transport" : "Arpeggiating MIDI input" : "MIDI thru";
    else if (static_cast<bool> (transport["playing"]))
        transportText = "Playing at " + juce::String (static_cast<double> (transport["tempo"]), 1) + " BPM";
    else if (host && static_cast<bool> (transport["armed"]))
        transportText = static_cast<bool> (transport["hostAvailable"]) ? "Waiting for host transport" : "Host transport unavailable";
    status.setText ((standalone ? juce::String ("Chord Runner Output") : juce::String ("MIDI to host")) + "  |  " + transportText, juce::dontSendNotification);
    if (const auto* chords = state["chords"].getArray(); chords != nullptr && ! chords->isEmpty())
    {
        const auto& chord = chords->getReference (juce::jlimit (0, chords->size() - 1, timeline.selectedChord()));
        juce::StringArray noteNames;
        if (const auto* notes = chord["notes"].getArray())
            for (const auto& note : *notes)
                noteNames.add (noteName (static_cast<int> (note)));
        summary.setText (chord["label"].toString() + "   " + noteNames.joinIntoString ("  ") + "     MIDI " + joined (chord["notes"], ", ")
            + "     |     " + juce::String (chords->size()) + " chords / " + juce::String (static_cast<double> (state["totalBeats"]) / 4.0, 2) + " bars", juce::dontSendNotification);
    }
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
    g.setColour (foreground);
    g.setFont (juce::FontOptions (22.0f));
    g.drawText ("Chord Runner", 20, 12, 180, 32, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (muted);
    g.drawText ("ofsound", 202, 15, 90, 28, juce::Justification::centredLeft);
    g.setColour (border);
    g.drawHorizontalLine (getWidth() < 760 ? 108 : 57, 0, static_cast<float> (getWidth()));
    g.drawHorizontalLine (getHeight() - 37, 0, static_cast<float> (getWidth()));
}
void NativeEditor::updateTimelineSize()
{
    const auto* chords = controller.state["chords"].getArray();
    const int count = chords != nullptr ? chords->size() : 1;
    const int width = juce::jmax (timelineViewport.getWidth(), 46 + count * 95);
    timeline.setSize (width, juce::jmax (100, timelineViewport.getHeight() - (width > timelineViewport.getWidth() ? 14 : 0)));
}
void NativeEditor::resized()
{
    const bool narrow = getWidth() < 760;
    const int headerHeight = narrow ? 108 : 57;
    auto header = getLocalBounds().withY (narrow ? 52 : 0).withHeight (narrow ? 52 : 57).reduced (16, 6);
    glossary.setBounds (header.removeFromRight (narrow ? 126 : 130));
    header.removeFromRight (8);
    save.setBounds (header.removeFromRight (66));
    header.removeFromRight (8);
    open.setBounds (header.removeFromRight (66));
    auto footer = getLocalBounds().removeFromBottom (narrow ? 84 : 36).reduced (16, 4);
    if (narrow)
    {
        status.setBounds (footer.removeFromBottom (30));
        play.setBounds (footer.removeFromLeft (90));
        footer.removeFromLeft (8);
        panicButton.setBounds (footer.removeFromRight (130));
    }
    else
    {
        panicButton.setBounds (footer.removeFromRight (115));
        status.setBounds (footer);
    }
    // Keep transport within reach while scrolling the phone's control sections.
    auto* playParent = narrow ? static_cast<juce::Component*> (this) : &content;
    if (play.getParentComponent() != playParent)
        playParent->addAndMakeVisible (play);
    viewport.setBounds (16, headerHeight + 10, getWidth() - 32, juce::jmax (1, getHeight() - headerHeight - (narrow ? 102 : 54)));
    const int columns = narrow ? (getWidth() < 500 ? 1 : 2) : 4;
    const int contentHeight = narrow ? (columns == 1 ? 1590 : 1130) : juce::jmax (740, viewport.getHeight());
    content.setSize (viewport.getWidth() - (contentHeight > viewport.getHeight() ? 16 : 0), contentHeight);
    const int w = content.getWidth();
    const int toggleWidth = juce::jmin (145, (w - 10) / 2);
    sequencer.setBounds (0, 0, toggleWidth, narrow ? 44 : 32);
    arpeggiator.setBounds (toggleWidth + 10, 0, toggleWidth, narrow ? 44 : 32);
    progression.setBounds (0, narrow ? 54 : 44, w - 84, 44);
    apply.setBounds (w - 76, narrow ? 54 : 44, 76, 44);
    example.setBounds (0, narrow ? 102 : 88, w, 28);
    message.setBounds (0, narrow ? 134 : 112, w, narrow ? 46 : 22);
    int timelineY = 206;
    if (narrow)
    {
        const int half = (w - 12) / 2;
        tempo.setBounds (0, 190, half, 62);
        clock.setBounds (half + 12, 190, half, 62);
        octave.setBounds (0, 264, 68, 62);
        channel.setBounds (80, 264, 68, 62);
        velocity.setBounds (160, 264, w - 160, 62);
        timelineY = 340;
    }
    else
    {
        auto transport = juce::Rectangle<int> (0, 141, w, 54);
        play.setBounds (transport.removeFromLeft (85).withTrimmedTop (18));
        transport.removeFromLeft (12);
        tempo.setBounds (transport.removeFromLeft (juce::jmax (140, w / 5)));
        transport.removeFromLeft (12);
        clock.setBounds (transport.removeFromLeft (115));
        channel.setBounds (transport.removeFromRight (75));
        transport.removeFromRight (10);
        velocity.setBounds (transport.removeFromRight (155));
        transport.removeFromRight (10);
        octave.setBounds (transport.removeFromRight (70));
    }
    const int timelineHeight = narrow ? 200 : contentHeight - 514;
    timelineViewport.setBounds (0, timelineY, w, timelineHeight);
    summary.setBounds (0, timelineY + timelineHeight, w, narrow ? 48 : 26);
    const int arpY = timelineY + timelineHeight + (narrow ? 58 : 36);
    hold.setBounds (0, arpY, 130, narrow ? 44 : 28);
    const int groupY = arpY + (narrow ? 56 : 36);
    for (size_t i = 0; i < groups.size(); ++i)
    {
        const int column = static_cast<int> (i) % columns;
        const int x = column * w / columns;
        const int right = (column + 1) * w / columns;
        groups[i].setBounds (x, groupY + static_cast<int> (i) / columns * 230, right - x - (column == columns - 1 ? 0 : 8), 222);
    }
    auto groupArea = [this] (size_t i) { return groups[i].getBounds().reduced (11, 0).withTrimmedTop (22); };
    auto a = groupArea (0);
    style.setBounds (a.removeFromTop (54)); a.removeFromTop (7);
    offset.setBounds (a.removeFromTop (54)); a.removeFromTop (7);
    repeats.setBounds (a.removeFromTop (54));
    a = groupArea (1);
    auto row = a.removeFromTop (54);
    rateMode.setBounds (row.removeFromLeft (row.getWidth() * 2 / 5)); row.removeFromLeft (7);
    rate.setBounds (row); freeRate.setBounds (row);
    a.removeFromTop (7); gate.setBounds (a.removeFromTop (54));
    a.removeFromTop (7); swing.setBounds (a.removeFromTop (54));
    a = groupArea (2);
    steps.setBounds (a.removeFromTop (54)); a.removeFromTop (7);
    distance.setBounds (a.removeFromTop (54)); a.removeFromTop (7);
    row = a.removeFromTop (54);
    retrigger.setBounds (row.removeFromLeft (row.getWidth() * 2 / 5)); row.removeFromLeft (7);
    interval.setBounds (row);
    a = groupArea (3);
    velocityEnabled.setBounds (a.removeFromTop (27)); a.removeFromTop (6);
    target.setBounds (a.removeFromTop (54)); a.removeFromTop (5);
    decay.setBounds (a.removeFromTop (54)); a.removeFromTop (8);
    velocityRetrigger.setBounds (a.removeFromTop (28));
    // Narrow timing/interval controls use direct numeric entry without a tiny rail.
    freeRate.slider.setSliderStyle (juce::Slider::IncDecButtons);
    interval.slider.setSliderStyle (juce::Slider::IncDecButtons);
    freeRate.slider.setTextBoxStyle (juce::Slider::TextBoxAbove, false, 90, 23);
    interval.slider.setTextBoxStyle (juce::Slider::TextBoxAbove, false, 90, 23);
    updateTimelineSize();
}
}
