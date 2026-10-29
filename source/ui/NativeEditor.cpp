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

void ModuleCard::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (0.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff22262c), 0, 0,
        juce::Colour (0xff181c21), 0, static_cast<float> (getHeight()), false));
    g.fillRoundedRectangle (area, 10.0f);
    g.setColour (juce::Colour (0xff363e46));
    g.drawRoundedRectangle (area, 10.0f, 1.0f);
    g.setColour (foreground);
    g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
    g.drawText (title, 62, 7, 170, 30, juce::Justification::centredLeft);
}

void PerformanceLook::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& colour, bool over, bool down)
{
    const auto area = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool power = button.getComponentID() == "sequencerEnabled" || button.getComponentID() == "arp.enabled";
    const auto fill = button.getToggleState() ? juce::Colour (0xff27483d) : colour;
    g.setColour (fill.brighter (down ? 0.14f : over ? 0.07f : 0.0f).withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.4f));
    g.fillRoundedRectangle (area, power ? 15.0f : 5.0f);
    g.setColour (button.getToggleState() ? green.withAlpha (0.65f) : border);
    g.drawRoundedRectangle (area, power ? 15.0f : 5.0f, 1.0f);
}

void PatternPad::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto area = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (active ? juce::Colour (0xff284f43) : selected ? juce::Colour (0xff303c47) : surface);
    g.fillRoundedRectangle (area, 6.0f);
    if (over || down)
    {
        g.setColour (foreground.withAlpha (down ? 0.12f : 0.05f));
        g.fillRoundedRectangle (area, 6.0f);
    }
    g.setColour (queued ? amber : active ? green : selected ? juce::Colour (0xff8caccc) : border);
    g.drawRoundedRectangle (area, 6.0f, queued || selected ? 1.5f : 1.0f);
    g.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));
    g.setColour (active ? green : foreground);
    if (getWidth() > 78)
        g.drawText (juce::String (slot + 1).paddedLeft ('0', 2), 9, 4, 26, getHeight() - 8, juce::Justification::centredLeft);
    else
        g.drawText (juce::String (slot + 1).paddedLeft ('0', 2), 4, 4, getWidth() - 8, getHeight() / 2, juce::Justification::centred);
    if (getWidth() > 78)
    {
        g.setFont (juce::FontOptions (11.0f));
        g.setColour (muted);
        g.drawText (patternName, 37, 4, getWidth() - 44, 18, juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (9.0f));
        g.drawText (juce::MidiMessage::getMidiNoteName (slot, true, true, 5), 37, 22, getWidth() - 44, 14, juce::Justification::centredLeft);
    }
    if (getWidth() <= 78)
    {
        g.setColour (muted);
        g.setFont (juce::FontOptions (9.0f));
        g.drawText (juce::MidiMessage::getMidiNoteName (slot, true, true, 5), 4, getHeight() / 2 + 2, getWidth() - 8, getHeight() / 2 - 6, juce::Justification::centred);
    }
    if (queued)
    {
        g.setColour (amber);
        g.fillEllipse (static_cast<float> (getWidth() - 9), 5.0f, 4.0f, 4.0f);
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
    constexpr float keyWidth = 46.0f, top = 54.0f;
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
        const bool sounding = sequenceOn && i == active
            && controller.transport["selectedPattern"] == controller.transport["activePattern"];
        g.setColour (sounding ? juce::Colour (0xff234e3d) : i == selected ? juce::Colour (0xff363a42) : surface);
        g.fillRect (x, 24.0f, chordWidth - 1.0f, 30.0f);
        g.setColour (sounding ? green : foreground);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (chord["label"].toString(), juce::Rectangle<float> (x + 6, 23, chordWidth - 10, 18), juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (10.0f));
        g.setColour (muted);
        g.drawText (juce::String (static_cast<double> (chord["beats"]) / 4.0, 2) + " bars",
                    juce::Rectangle<float> (x + 6, 40, chordWidth - 10, 14), juce::Justification::centredLeft);
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
    slider.setChangeNotificationOnlyOnRelease (false);
    slider.setTooltip (name + ": drag to adjust, click the value to type, double-click to reset.");
}
void NativeEditor::NumberControl::resized()
{
    auto area = getLocalBounds();
    const auto* editor = findParentComponentOfClass<NativeEditor>();
    label.setBounds (area.removeFromTop (editor != nullptr && editor->phoneLayout ? 16 : 18));
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
    const auto* editor = findParentComponentOfClass<NativeEditor>();
    const bool touch = editor != nullptr && editor->phoneLayout;
    label.setBounds (area.removeFromTop (touch ? 16 : 18));
    combo.setBounds (area.reduced (0, touch ? 1 : 2));
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
    const juce::StringArray groupNames { "ORDER", "TIMING", "PITCH", "HUMANIZE", "ENVELOPE" };
    for (size_t i = 0; i < groups.size(); ++i)
    {
        groups[i].setText (groupNames[static_cast<int> (i)]);
        content.addAndMakeVisible (groups[i]);
    }
    for (auto* component : std::initializer_list<juce::Component*> { &style, &offset, &repeats, &rateMode, &rate, &freeRate, &gate, &swing, &swingSubdivision, &velocityHumanize, &timingHumanize, &steps, &distance, &retrigger, &interval, &velocityEnabled, &target, &decay, &velocityRetrigger })
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
    save.onClick = [this] { if ((! controller.dirty() || controller.applyDraft()) && chooseProject) chooseProject (1); };
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
    bind (swingSubdivision, "swingSubdivision", true);
    bind (velocityHumanize, "velocityHumanize", true);
    bind (timingHumanize, "timingHumanize", true);
    bind (steps, "steps", true);
    bind (distance, "distance", true);
    bind (retrigger, "retrigger", true);
    bind (interval, "interval", true);
    bind (target, "target", true);
    bind (decay, "decay", true);
    freeRate.slider.setSkewFactorFromMidPoint (200);
    decay.slider.setSkewFactorFromMidPoint (1000);
    repeats.slider.textFromValueFunction = [] (double value) { return value < 0.5 ? juce::String ("Infinite") : juce::String (static_cast<int> (value)); };
    repeats.slider.updateText();
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
    // Each power control belongs to the module it enables.
    content.addAndMakeVisible (sequenceModule);
    content.addAndMakeVisible (arpModule);
    for (auto* component : std::initializer_list<juce::Component*> { &sequencer, &progression, &apply, &message, &timelineViewport, &summary, &patternName, &fit, &patternMenu, &glossary })
        sequenceModule.addAndMakeVisible (component);
    example.setVisible (false);
    for (auto* component : std::initializer_list<juce::Component*> { &arpeggiator, &style, &offset, &repeats, &rateMode, &rate, &freeRate, &gate, &swing, &swingSubdivision, &steps, &distance, &retrigger, &interval, &hold, &velocityEnabled, &target, &decay, &velocityRetrigger, &velocityHumanize, &timingHumanize })
        arpModule.addAndMakeVisible (component);
    for (auto& group : groups)
        arpModule.addAndMakeVisible (group);
    for (auto* component : std::initializer_list<juce::Component*> { &projectTitle, &projectMenu, &undoButton, &redoButton, &mutePad, &launch, &tempo, &clock, &octave, &velocity, &channel, &play })
        addAndMakeVisible (component);
    open.setVisible (false);
    save.setVisible (false);
    identify (projectMenu, "project.menu", "Project menu");
    identify (projectTitle, "project.details", "Project details");
    identify (undoButton, "undo", "Undo last edit");
    identify (redoButton, "redo", "Redo last edit");
    identify (fit, "timeline.fit", "Fit progression in view");
    identify (mutePad, "pattern.mute", "Mute pattern / MIDI note 16");
    identify (patternName, "pattern.name", "Pattern name");
    patternName.setEditable (false, true);
    patternName.setColour (juce::Label::textColourId, muted);
    patternName.onTextChange = [this]
    {
        if (! syncing)
        {
            controller.processor.renamePattern (controller.processor.selectedPattern(), patternName.getText());
            controller.refresh();
        }
    };
    patternMenu.onClick = [this] { showPatternMenu(); };
    projectMenu.onClick = [this] { showProjectMenu(); };
    projectTitle.onClick = [this] { showProjectDetails(); };
    undoButton.onClick = [this] { controller.undo(); };
    redoButton.onClick = [this] { controller.redo(); };
    mutePad.setClickingTogglesState (false);
    mutePad.onClick = [this] { controller.selectPattern (-1); };
    launch.combo.onChange = [this]
    {
        if (! syncing)
        {
            controller.processor.setLaunchMode (launch.combo.getSelectedId() - 1);
            controller.refresh();
        }
    };
    fit.setClickingTogglesState (true);
    fit.setToggleState (true, juce::dontSendNotification);
    fit.onClick = [this] { fitNotes = fit.getToggleState(); updateTimelineSize(); };
    for (int i = 0; i < 16; ++i)
    {
        auto& pad = pads[static_cast<size_t> (i)];
        pad.slot = i;
        identify (pad, "pattern." + juce::String (i + 1), "Pattern " + juce::String (i + 1));
        pad.setTooltip ("Launch pattern " + juce::String (i + 1) + " / "
            + juce::MidiMessage::getMidiNoteName (i, true, true, 5) + " (MIDI " + juce::String (i) + ")");
        pad.onClick = [this, i]
        {
            if (juce::ModifierKeys::getCurrentModifiers().isShiftDown() && controller.processor.selectedPattern() >= 0)
            {
                if (! controller.dirty() || controller.applyDraft())
                    controller.processor.copyPattern (controller.processor.selectedPattern(), i);
                controller.refresh();
            }
            else controller.selectPattern (i);
        };
        addAndMakeVisible (pad);
    }
    swing.slider.setTooltip ("KSH swing: delays alternate groups by up to half a subdivision.");
    timingHumanize.slider.setTooltip ("KSH timing humanize: independent early/late offsets, up to 20% of the step duration.");
    velocityHumanize.slider.setTooltip ("KSH velocity humanize: random variation relative to each note's velocity.");
    const juce::StringArray phonePages { "Perform", "Sequence", "Arp", "Groove" };
    for (int i = 0; i < 4; ++i)
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
    performanceInfo.setFont (juce::FontOptions (14.0f));
    performanceInfo.setColour (juce::Label::textColourId, foreground);
    performanceInfo.setJustificationType (juce::Justification::centredLeft);
    addChildComponent (performanceInfo);
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
    control.slider.onDragStart = [this] { controller.processor.endEditGesture(); };
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
        swingSubdivision.combo.setSelectedId (static_cast<int> (arp["swingSubdivision"]) + 1, juce::dontSendNotification);
        launch.combo.setSelectedId (controller.processor.getLaunchMode() + 1, juce::dontSendNotification);
        bankState = controller.processor.getProjectState();
        projectModified = controller.processor.isDocumentModified();
        patternName.setText (state["patternName"].toString(), juce::dontSendNotification);
        updateTimelineSize();
    }
    projectTitle.setButtonText (state["projectName"].toString() + (projectModified ? "  *" : ""));
    undoButton.setEnabled (controller.processor.canUndo());
    redoButton.setEnabled (controller.processor.canRedo());
    const int selectedSlot = static_cast<int> (transport["selectedPattern"]);
    sequenceModule.setEnabled (selectedSlot >= 0);
    arpModule.setEnabled (selectedSlot >= 0);
    octave.setEnabled (selectedSlot >= 0);
    velocity.setEnabled (selectedSlot >= 0);
    channel.setEnabled (selectedSlot >= 0);
    const int activeSlot = static_cast<int> (transport["activePattern"]);
    const int queuedSlot = static_cast<int> (transport["queuedPattern"]);
    for (int i = 0; i < 16; ++i)
    {
        auto& pad = pads[static_cast<size_t> (i)];
        pad.active = i == activeSlot;
        pad.queued = i == queuedSlot;
        pad.selected = i == selectedSlot;
        pad.patternName = bankState["patterns"][i]["name"].toString();
        pad.repaint();
    }
    mutePad.setToggleState (selectedSlot < 0, juce::dontSendNotification);
    sequencer.setButtonText (sequenceOn ? "ON" : "OFF");
    arpeggiator.setButtonText (arpOn ? "ON" : "OFF");
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
    for (auto* component : std::initializer_list<juce::Component*> { &style, &offset, &repeats, &rateMode, &rate, &freeRate, &gate, &swing, &swingSubdivision, &velocityHumanize, &timingHumanize, &steps, &distance, &retrigger, &hold, &velocityEnabled })
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
    if (queuedSlot != -2)
        transportText += queuedSlot < 0 ? "  /  Mute queued" : "  /  P" + juce::String (queuedSlot + 1) + " queued";
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
    performanceInfo.setText ((selectedSlot < 0 ? juce::String ("Muted") : "P" + juce::String (selectedSlot + 1).paddedLeft ('0', 2)
        + "  " + state["patternName"].toString()) + "\n" + state["progression"].toString(), juce::dontSendNotification);
    if (phoneLayout)
    {
        status.setText (queuedSlot != -2 ? (queuedSlot < 0 ? juce::String ("Mute queued") : "P" + juce::String (queuedSlot + 1) + " queued")
            : (activeSlot < 0 ? juce::String ("Muted") : "P" + juce::String (activeSlot + 1)) + (static_cast<bool> (transport["playing"]) ? "  Playing" : "  Stopped"), juce::dontSendNotification);
        layoutPhone();
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
    g.setFont (juce::FontOptions (phoneLayout ? 12.0f : 18.0f).withStyle ("Bold"));
    if (! phoneLayout || getWidth() <= getHeight())
        g.drawText ("CHORD RUNNER", phoneLayout ? 12 : 18, phoneLayout ? 3 : 10, 165, phoneLayout ? 18 : 24, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (10.0f));
    g.setColour (muted);
    if (! phoneLayout)
        g.drawText ("ofsound  /  MIDI PERFORMANCE", 19, 32, 190, 16, juce::Justification::centredLeft);
    g.setColour (border);
    g.drawHorizontalLine (55, 16, static_cast<float> (getWidth() - 16));
    g.drawHorizontalLine (getHeight() - 46, 16, static_cast<float> (getWidth() - 16));
}

void NativeEditor::updateTimelineSize()
{
    const auto* chords = controller.state["chords"].getArray();
    const int count = chords != nullptr ? chords->size() : 1;
    const int width = fitNotes ? timelineViewport.getWidth() : juce::jmax (timelineViewport.getWidth(), 46 + count * 95);
    timeline.setSize (juce::jmax (60, width), juce::jmax (phoneLayout ? 60 : 80, timelineViewport.getHeight() - (width > timelineViewport.getWidth() ? 14 : 0)));
}

void NativeEditor::resized()
{
    const bool compact = getWidth() < 700 || (getHeight() <= 500 && getWidth() <= 1100);
    if (compact)
    {
        phoneLayout = true;
        layoutPhone();
        return;
    }
    restoreDesktopParents();
    phoneLayout = false;
    const int w = getWidth();
    const bool phone = w < 700;
    const int margin = phone ? 10 : 16;
    auto header = juce::Rectangle<int> (phone ? 10 : 210, phone ? 56 : 8, w - (phone ? 20 : 226), 38);
    redoButton.setBounds (header.removeFromRight (52)); header.removeFromRight (5);
    undoButton.setBounds (header.removeFromRight (52)); header.removeFromRight (8);
    projectMenu.setBounds (header.removeFromRight (72)); header.removeFromRight (8);
    projectTitle.setBounds (header);
    const int padTop = phone ? 108 : 56;
    const int padColumns = phone ? 4 : 8;
    const int padWidth = (w - margin * 2 - 60) / padColumns;
    for (int i = 0; i < 16; ++i)
        pads[static_cast<size_t> (i)].setBounds (margin + (i % padColumns) * padWidth, padTop + (i / padColumns) * 42, padWidth - 4, 40);
    mutePad.setBounds (w - margin - 54, padTop, 54, phone ? 180 : 88);
    const int transportY = padTop + (phone ? 186 : 90);
    auto row = juce::Rectangle<int> (margin, transportY, w - margin * 2, phone ? 112 : 56);
    if (phone)
    {
        auto top = row.removeFromTop (56);
        tempo.setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (0, 0).withTrimmedRight (8));
        clock.setBounds (top);
        row.removeFromTop (6);
    }
    else
    {
        tempo.setBounds (row.removeFromLeft (juce::jmax (140, w / 6))); row.removeFromLeft (10);
        clock.setBounds (row.removeFromLeft (110)); row.removeFromLeft (10);
    }
    launch.setBounds (row.removeFromRight (phone ? 110 : 130)); row.removeFromRight (10);
    channel.setBounds (row.removeFromRight (phone ? 62 : 72)); row.removeFromRight (10);
    octave.setBounds (row.removeFromRight (phone ? 62 : 72)); row.removeFromRight (10);
    velocity.setBounds (row);
    const int bodyTop = transportY + (phone ? 126 : 60);
    viewport.setBounds (margin, bodyTop, w - margin * 2, juce::jmax (40, getHeight() - bodyTop - 50));
    auto footer = getLocalBounds().removeFromBottom (42).reduced (margin, 3);
    play.setBounds (footer.removeFromLeft (90)); footer.removeFromLeft (12);
    panicButton.setBounds (footer.removeFromRight (phone ? 105 : 120)); footer.removeFromRight (10);
    status.setBounds (footer);
    const int columns = w >= 1000 ? 5 : w >= 700 ? 3 : w >= 500 ? 2 : 1;
    const int groupRows = (5 + columns - 1) / columns;
    const int arpHeight = 44 + groupRows * 188;
    const int minimumSequence = 224;
    const int minimumHeight = minimumSequence + 12 + arpHeight;
    const int contentHeight = juce::jmax (minimumHeight, viewport.getHeight());
    content.setSize (viewport.getWidth() - (contentHeight > viewport.getHeight() ? 14 : 0), contentHeight);
    const int cw = content.getWidth();
    const int sequenceHeight = contentHeight - arpHeight - 12;
    sequenceModule.setBounds (0, 0, cw, sequenceHeight);
    arpModule.setBounds (0, sequenceHeight + 12, cw, arpHeight);
    sequencer.setBounds (14, 8, 40, 28);
    auto moduleHeader = juce::Rectangle<int> (230, 7, juce::jmax (0, cw - 244), 30);
    patternMenu.setBounds (moduleHeader.removeFromRight (75)); moduleHeader.removeFromRight (6);
    fit.setBounds (moduleHeader.removeFromRight (42)); moduleHeader.removeFromRight (6);
    glossary.setBounds (moduleHeader.removeFromRight (64));
    patternName.setBounds (moduleHeader);
    patternName.setVisible (cw >= 700);
    glossary.setVisible (cw >= 650);
    if (cw < 650)
    {
        patternMenu.setBounds (cw - 89, 7, 75, 30);
        fit.setBounds (cw - 137, 7, 42, 30);
    }
    progression.setBounds (14, 47, cw - 108, 42);
    apply.setBounds (cw - 86, 47, 72, 42);
    message.setBounds (14, 92, cw - 28, 22);
    timelineViewport.setBounds (14, 115, cw - 28, sequenceHeight - 144);
    summary.setBounds (14, sequenceHeight - 27, cw - 28, 22);
    arpeggiator.setBounds (14, 8, 40, 28);
    hold.setBounds (cw - 124, 8, 110, 28);
    for (size_t i = 0; i < groups.size(); ++i)
    {
        const int col = static_cast<int> (i) % columns;
        const int x = 10 + col * (cw - 20) / columns;
        const int right = 10 + (col + 1) * (cw - 20) / columns;
        groups[i].setBounds (x, 44 + static_cast<int> (i) / columns * 188, right - x - 5, 184);
    }
    auto groupArea = [this] (size_t i) { return groups[i].getBounds().reduced (10, 0).withTrimmedTop (19); };
    auto a = groupArea (0);
    style.setBounds (a.removeFromTop (48)); a.removeFromTop (6);
    offset.setBounds (a.removeFromTop (48)); a.removeFromTop (6);
    repeats.setBounds (a.removeFromTop (48));
    a = groupArea (1);
    auto timing = a.removeFromTop (48);
    rateMode.setBounds (timing.removeFromLeft (65)); timing.removeFromLeft (6);
    rate.setBounds (timing); freeRate.setBounds (timing);
    a.removeFromTop (6); gate.setBounds (a.removeFromTop (48));
    a.removeFromTop (6); swing.setBounds (a.removeFromTop (48));
    a = groupArea (2);
    steps.setBounds (a.removeFromTop (48)); a.removeFromTop (6);
    distance.setBounds (a.removeFromTop (48)); a.removeFromTop (6);
    auto pitch = a.removeFromTop (48);
    retrigger.setBounds (pitch.removeFromLeft (65)); pitch.removeFromLeft (6);
    interval.setBounds (pitch);
    a = groupArea (3);
    swingSubdivision.setBounds (a.removeFromTop (48)); a.removeFromTop (6);
    velocityHumanize.setBounds (a.removeFromTop (48)); a.removeFromTop (6);
    timingHumanize.setBounds (a.removeFromTop (48));
    a = groupArea (4);
    velocityEnabled.setBounds (a.removeFromTop (27)); a.removeFromTop (4);
    target.setBounds (a.removeFromTop (48)); a.removeFromTop (4);
    decay.setBounds (a.removeFromTop (48)); a.removeFromTop (6);
    velocityRetrigger.setBounds (a.removeFromTop (28));
    freeRate.slider.setSliderStyle (juce::Slider::IncDecButtons);
    interval.slider.setSliderStyle (juce::Slider::IncDecButtons);
    freeRate.slider.setTextBoxStyle (juce::Slider::TextBoxAbove, false, 76, 25);
    interval.slider.setTextBoxStyle (juce::Slider::TextBoxAbove, false, 76, 25);
    updateTimelineSize();
}
}
