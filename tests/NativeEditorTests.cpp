#include <PluginEditor.h>
#include <ui/NativeEditor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using namespace chordrunner::ui;

namespace
{
juce::Component* findControl (juce::Component& parent, const juce::String& id)
{
    if (parent.getComponentID() == id)
        return &parent;
    for (auto* child : parent.getChildren())
        if (auto* result = findControl (*child, id))
            return result;
    return nullptr;
}
template <typename T>
T& control (juce::Component& editor, const juce::String& id)
{
    auto* result = dynamic_cast<T*> (findControl (editor, id));
    REQUIRE (result != nullptr);
    return *result;
}
void choose (NativeEditor& editor, const juce::String& id, int item)
{
    if (id == "arp.style" || id == "arp.retrigger" || id == "arp.sync" || id == "arp.octaveMode")
    {
        control<juce::TextButton> (editor, id + "." + juce::String (id == "arp.octaveMode" ? 2 - item : item - 1)).onClick();
        return;
    }
    auto& combo = control<juce::ComboBox> (editor, id);
    combo.setSelectedId (item, juce::dontSendNotification);
    combo.onChange();
}
void toggle (NativeEditor& editor, const juce::String& id, bool enabled)
{
    auto& button = control<juce::TextButton> (editor, id);
    button.setToggleState (enabled, juce::dontSendNotification);
    button.onClick();
}
}

TEST_CASE ("Loop bracket draws and saves its range and bypass independently per pattern", "[native-ui][loop-range]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1400, 1000);
    auto& bracket = control<SequenceLoopBar> (editor, "sequence.loop.range");
    const float width = static_cast<float> (bracket.getWidth() - 24);
    const juce::Point<float> origin { 24 + width / 4, 10 }, end { 24 + width / 2, 10 };
    const auto event = [&] (juce::Point<float> point) { return juce::MouseEvent {
        juce::Desktop::getInstance().getMainMouseSource(), point, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
        1, 0, 0, 0, 0, &bracket, &bracket, juce::Time(), origin, juce::Time(), 1, true }; };
    bracket.mouseDown (event (origin)); bracket.mouseDrag (event (end)); bracket.mouseUp (event (end));
    CHECK (static_cast<double> (processor.getTimelineState()["loopStart"]) == Catch::Approx (4));
    CHECK (static_cast<double> (processor.getTimelineState()["loopEnd"]) == Catch::Approx (8));
    CHECK (static_cast<bool> (processor.getTimelineState()["loopEnabled"]));
    toggle (editor, "sequence.loop.enabled", false);
    processor.selectPattern (1);
    CHECK_FALSE (static_cast<bool> (processor.getTimelineState()["loopEnabled"]));
    juce::MemoryBlock saved; processor.getStateInformation (saved);
    PluginProcessor restored; restored.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    restored.selectPattern (0);
    CHECK_FALSE (static_cast<bool> (restored.getTimelineState()["loopEnabled"]));
    CHECK (static_cast<double> (restored.getTimelineState()["loopStart"]) == Catch::Approx (4));
    CHECK (static_cast<double> (restored.getTimelineState()["loopEnd"]) == Catch::Approx (8));
}

TEST_CASE ("Both visualizations use scheduled humanized velocities and positions", "[native-ui][humanize-viz]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1400, 1000);
    toggle (editor, "arp.enabled", true);
    auto& arp = control<ArpVisualizer> (editor, "arp.visualizer");
    auto settings = processor.getArpPreviewSettings();
    arp.update (settings, 120, 3);
    const auto beforeArp = arp.previewNotes(), beforeSeq = processor.getOutputPreview();
    REQUIRE (editor.controller.set ("loopStart", 4));
    REQUIRE (editor.controller.set ("loopEnd", 8));
    REQUIRE (editor.controller.set ("loopEnabled", true));
    const auto loopedPlain = processor.getOutputPreview();
    REQUIRE (editor.controller.set ("velocityHumanize", 80, true));
    REQUIRE (editor.controller.set ("timingHumanize", 80, true));
    settings = processor.getArpPreviewSettings(); arp.update (settings, 120, 3);
    const auto changed = [] (const auto& before, const auto& after, bool velocity)
    {
        const auto count = juce::jmin (before.size(), after.size());
        for (size_t i = 0; i < count; ++i)
            if (velocity ? before[i].velocity != after[i].velocity : std::abs (before[i].start - after[i].start) > 1.0e-6) return true;
        return false;
    };
    CHECK (changed (beforeArp, arp.previewNotes(), true));
    CHECK (changed (beforeArp, arp.previewNotes(), false));
    const auto humanized = processor.getOutputPreview();
    REQUIRE_FALSE (beforeSeq.empty());
    REQUIRE_FALSE (humanized.empty());
    CHECK (changed (loopedPlain, humanized, true));
    CHECK (changed (loopedPlain, humanized, false));
    for (const auto& note : humanized) { CHECK (note.start >= 4); CHECK (note.end <= 8); }
}

TEST_CASE ("Mode pad scrubs on press and drag as one host gesture and undo step", "[native-ui][mode-pad]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1400, 1000);
    toggle (editor, "arp.enabled", true);
    auto& first = control<juce::TextButton> (editor, "arp.style.0");
    auto& last = control<juce::TextButton> (editor, "arp.style.9");
    auto& grid = *first.getParentComponent();
    struct Listener final : juce::AudioProcessorParameter::Listener
    {
        int starts = 0, ends = 0, values = 0;
        void parameterValueChanged (int, float) override { ++values; }
        void parameterGestureChanged (int, bool started) override { if (started) ++starts; else ++ends; }
    } listener;
    auto* parameter = processor.getParameters()[0];
    parameter->addListener (&listener);
    const auto point = first.getLocalBounds().getCentre().toFloat();
    const auto eventAt = [&] (juce::Point<float> at)
    {
        return juce::MouseEvent { juce::Desktop::getInstance().getMainMouseSource(), at,
            juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0,
            &first, &first, juce::Time(), point, juce::Time(), 1, true };
    };
    static_cast<juce::Component&> (first).mouseDown (eventAt (point));
    auto location = first.getLocalPoint (&last, last.getLocalBounds().getCentre()).toFloat();
    static_cast<juce::Component&> (first).mouseDrag (eventAt (location));
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["style"]) == 9);
    CHECK (last.getToggleState());
    static_cast<juce::Component&> (first).mouseDrag (eventAt (location)); // Stationary events must not spam automation.
    CHECK (listener.values == 1);
    static_cast<juce::Component&> (first).mouseDrag (eventAt ({ -100, -100 }));
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["style"]) == 9);
    location = first.getLocalPoint (&grid, grid.getLocalBounds().getCentre()).toFloat();
    static_cast<juce::Component&> (first).mouseDrag (eventAt (location));
    static_cast<juce::Component&> (first).mouseUp (eventAt (location));
    CHECK (listener.starts == 1);
    CHECK (listener.ends == 1);
    processor.undo();
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["style"]) == 0);
    parameter->removeListener (&listener);
}

TEST_CASE ("Chord completion covers every root using the parser vocabulary", "[native-ui][completion]")
{
    for (const auto* root : { "C", "C#", "Db", "D", "D#", "Eb", "E", "F", "F#", "Gb", "G", "G#", "Ab", "A", "A#", "Bb", "B" })
    {
        const auto options = ChordCompletion::suggestions (root, juce::String (root).length());
        REQUIRE (options.matches.size() == static_cast<size_t> (chordrunner::chordVocabulary().size()));
        for (const auto& match : options.matches)
        {
            INFO (match.chord);
            chordrunner::Timeline parsed;
            CHECK (chordrunner::parseProgression (match.chord, parsed).wasOk());
            CHECK (match.name.isNotEmpty());
            CHECK (match.degrees.isNotEmpty());
        }
    }
    for (const auto* chord : { "Cm7", "CM7", "Fmaj7", "Bbmin7" })
    {
        const auto options = ChordCompletion::suggestions (chord, juce::String (chord).length());
        REQUIRE_FALSE (options.matches.empty());
        CHECK (options.matches.front().chord == chord);
    }
    for (const auto* text : { "", "C - ", "C | ", "H", "F7@2", "F1/2", "C nonsense" })
        CHECK (ChordCompletion::suggestions (text, juce::String (text).length()).matches.empty());
    CHECK_FALSE (ChordCompletion::suggestions ("Cmaj7", 0).matches.empty());
    const auto accidental = ChordCompletion::suggestions ("F#m7", 1);
    REQUIRE_FALSE (accidental.matches.empty());
    CHECK (accidental.matches.front().chord == "F#");
    const auto implicit = ChordCompletion::suggestions ("F1/2", 1);
    CHECK (implicit.begin == 0);
    CHECK (implicit.end == 1);
    const auto word = ChordCompletion::suggestions ("Fminor", 6);
    REQUIRE_FALSE (word.matches.empty());
    CHECK (word.matches.front().chord == "Fm");
}

TEST_CASE ("Chord suggestions replace only the active chord and do not commit playback", "[native-ui][completion]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1400, 1000);
    editor.setVisible (true);
    auto& input = control<ChordInput> (editor, "progression");
    auto& popup = control<ChordCompletion> (editor, "progression.completion");
    const auto saved = processor.getTimelineState()["progression"].toString();
    input.setText ("C - Fma@2 - G", false);
    input.setCaretPosition (7);
    popup.refresh();
    REQUIRE (popup.isVisible());
    const auto inputBounds = editor.getLocalArea (&input, input.getLocalBounds());
    CHECK (popup.getY() > inputBounds.getBottom());
    CHECK (editor.getLocalBounds().contains (popup.getBounds()));
    const auto options = ChordCompletion::suggestions (input.getText(), 7);
    auto& list = control<juce::ListBox> (editor, "progression.completion.list");
    int row = 0;
    for (size_t i = 0; i < options.matches.size(); ++i)
        if (options.matches[i].chord == "Fmaj7") row = static_cast<int> (i);
    list.selectRow (row);
    CHECK (input.keyPressed (juce::KeyPress (juce::KeyPress::tabKey)));
    CHECK (input.getText() == "C - Fmaj7@2 - G");
    CHECK_FALSE (popup.isVisible());
    popup.refresh();
    CHECK_FALSE (popup.isVisible()); // Asynchronous text notifications cannot reopen it.
    CHECK (processor.getTimelineState()["progression"].toString() == saved);
    input.onReturnKey(); // Apply directly, without waiting for an asynchronous edit notification.
    CHECK (editor.controller.error.isEmpty());
    CHECK (static_cast<double> (processor.getTimelineState()["chords"][1]["beats"]) == Catch::Approx (8.0));
    input.setText ("D", false); input.setCaretPosition (1); popup.refresh();
    CHECK (input.keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
    CHECK (list.getSelectedRow() == 1);
    CHECK (input.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK_FALSE (popup.isVisible());
    CHECK (input.getText() == "D");
    popup.refresh();
    CHECK_FALSE (popup.isVisible());
    popup.refresh (true); // Clicking the same chord explicitly reopens a dismissed menu.
    REQUIRE (popup.isVisible());
    CHECK (input.keyPressed (juce::KeyPress (juce::KeyPress::returnKey)));
    CHECK_FALSE (popup.isVisible());
    CHECK (input.getText() == "D");
    input.setText ("Bb", false); input.setCaretPosition (2); popup.refresh();
    REQUIRE (popup.isVisible());
    const auto image = editor.createComponentSnapshot (editor.getLocalBounds());
    juce::FileOutputStream stream (juce::File ("/tmp/chord-runner-completion.png"));
    stream.setPosition (0); stream.truncate();
    juce::PNGImageFormat().writeImageToStream (image, stream);
    const auto click = juce::MouseEvent { juce::Desktop::getInstance().getMainMouseSource(), { 10, 10 }, {},
        1, 0, 0, 0, 0, &list, &list, juce::Time(), { 10, 10 }, juce::Time(), 1, false };
    list.getListBoxModel()->listBoxItemClicked (1, click);
    CHECK_FALSE (popup.isVisible());
    CHECK (input.getText().startsWith ("Bb"));
    CHECK (input.getText() != "Bb");
}

TEST_CASE ("Envelope graph edits start end and duration ratio without dials", "[native-ui][envelope]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1400, 1000);
    toggle (editor, "arp.enabled", true);
    toggle (editor, "arp.velocityEnabled", true);
    REQUIRE (editor.controller.set ("startVelocity", 100, true));
    REQUIRE (editor.controller.set ("durationRatio", 1.0, true));
    REQUIRE (editor.controller.set ("target", 25, true));
    auto& graph = control<VelocityEnvelopeView> (editor, "arp.envelope.display");
    auto drag = [&] (int handle, juce::Point<float> destination)
    {
        const auto origin = graph.handlePosition (handle);
        const auto event = [&] (juce::Point<float> at) { return juce::MouseEvent {
            juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
            1, 0, 0, 0, 0, &graph, &graph, juce::Time(), origin, juce::Time(), 1, true }; };
        graph.mouseDown (event (origin)); graph.mouseDrag (event (destination)); graph.mouseUp (event (destination));
    };
    drag (0, { 20, 38 });
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["startVelocity"]) == 127);
    drag (1, { static_cast<float> (graph.getWidth() - 20), static_cast<float> (graph.getHeight() - 60) });
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["target"]) == 0);
    drag (2, { static_cast<float> (graph.getWidth()) * 0.5f, graph.handlePosition (2).y });
    CHECK (static_cast<double> (processor.getTimelineState()["arp"]["durationRatio"]) == Catch::Approx (0.5));
    CHECK (findControl (editor, "arp.target") == nullptr);
    CHECK (findControl (editor, "arp.decay") == nullptr);
    const auto image = editor.createComponentSnapshot (editor.getLocalBounds());
    juce::FileOutputStream stream (juce::File ("/tmp/chord-runner-envelope.png"));
    stream.setPosition (0); stream.truncate(); juce::PNGImageFormat().writeImageToStream (image, stream);
    toggle (editor, "arp.velocityEnabled", false);
    CHECK (control<juce::Slider> (editor, "arp.velocityHumanize").isEnabled());
    juce::MemoryBlock saved; processor.getStateInformation (saved);
    PluginProcessor restored; restored.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    CHECK (static_cast<int> (restored.getTimelineState()["arp"]["startVelocity"]) == 127);
    CHECK (static_cast<double> (restored.getTimelineState()["arp"]["durationRatio"]) == Catch::Approx (0.5));
}

TEST_CASE ("Mode headers toggle their state and stay clickable above the controls", "[native-ui]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1400, 1000);
    editor.setVisible (true);
    toggle (editor, "arp.enabled", true);
    for (const auto* id : { "arp.sync", "arp.octaveMode", "arp.swingSubdivision" })
    {
        auto& header = control<juce::TextButton> (editor, juce::String (id) + ".mode");
        REQUIRE (header.isVisible());
        const auto point = editor.getLocalPoint (&header, header.getLocalBounds().getCentre());
        CHECK (editor.getComponentAt (point) == &header);
        CHECK_FALSE (control<juce::TextButton> (editor, juce::String (id) + ".0").isVisible());
        const auto before = header.getButtonText();
        header.onClick();
        CHECK (header.getButtonText() != before);
        header.onClick();
        CHECK (header.getButtonText() == before);
    }
    auto& humanize = control<juce::GroupComponent> (editor, "arp.humanize.panel");
    auto& lane = control<CycleLane> (editor, "rhythm.tie");
    CHECK (editor.getLocalArea (&lane, lane.getLocalBounds()).getRight() < editor.getLocalArea (&humanize, humanize.getLocalBounds()).getX());
    auto& seq = control<ModuleCard> (editor, "sequence.module");
    auto& arp = control<ModuleCard> (editor, "arp.module");
    auto& body = control<juce::Viewport> (editor, "body.viewport");
    const auto seqBounds = editor.getLocalArea (&seq, seq.getLocalBounds());
    const auto arpBounds = editor.getLocalArea (&arp, arp.getLocalBounds());
    const int gap = arpBounds.getY() - seqBounds.getBottom();
    CHECK (seqBounds.getY() - body.getY() == gap);
    CHECK (editor.getHeight() - 42 - arpBounds.getBottom() == gap);
    CHECK (std::abs (seqBounds.getHeight() - arpBounds.getHeight()) <= 1);
    const auto image = editor.createComponentSnapshot (editor.getLocalBounds());
    juce::FileOutputStream stream (juce::File ("/tmp/chord-runner-layout.png"));
    stream.setPosition (0); stream.truncate();
    juce::PNGImageFormat().writeImageToStream (image, stream);
}

TEST_CASE ("Native controls retain every numeric engine mapping", "[native-ui]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1120, 900);
    toggle (editor, "arp.enabled", true);
    toggle (editor, "arp.velocityEnabled", true);
    struct Setting { const char* id; const char* key; double value; bool arp; };
    const std::array<Setting, 8> settings {{
        Setting { "tempo", "tempo", 137, false }, { "velocity", "velocity", 93, false },
        { "arp.freeMs", "freeMs", 237.5, true }, { "arp.gate", "gate", 63.5, true },
        { "arp.swing", "swing", 17, true }, { "arp.steps", "steps", 4, true },
        { "arp.repeats", "repeats", 8, true }, { "arp.interval", "interval", 0.0625, true },
    }};
    for (const auto& setting : settings)
    {
        INFO (setting.id);
        control<juce::Slider> (editor, setting.id).setValue (setting.value, juce::sendNotificationSync);
        const auto state = processor.getTimelineState();
        CHECK (static_cast<double> ((setting.arp ? state["arp"] : state)[setting.key]) == Catch::Approx (setting.value));
        CHECK (editor.controller.error.isEmpty());
    }
    choose (editor, "octave", 4);
    CHECK (findControl (editor, "channel") == nullptr);
    choose (editor, "clock", 2);
    CHECK (static_cast<int> (processor.getTimelineState()["octave"]) == 3);
    CHECK (static_cast<int> (processor.getTimelineState()["channel"]) == 1);
    CHECK (static_cast<bool> (processor.getTimelineState()["hostSync"]));
    CHECK_FALSE (control<juce::Slider> (editor, "tempo").isEnabled());
    for (const int style : { 0, 1, 2, 3, 4, 5, 6, 8, 9 })
    {
        choose (editor, "arp.style", style + 1);
        CHECK (static_cast<int> (processor.getTimelineState()["arp"]["style"]) == style);
        for (const int other : { 0, 1, 2, 3, 4, 5, 6, 8, 9 })
            CHECK (control<juce::TextButton> (editor, "arp.style." + juce::String (other)).getToggleState() == (other == style));
    }
    constexpr std::array<double, 12> divisions { 4, 2, 1, 0.5, 0.25, 0.125, 0.0625, 2.0 / 3, 1.0 / 3, 1.0 / 6, 0.75, 0.375 };
    for (size_t i = 0; i < divisions.size(); ++i)
    {
        choose (editor, "arp.rate", static_cast<int> (i) + 1);
        CHECK (static_cast<double> (processor.getTimelineState()["arp"]["rate"]) == Catch::Approx (divisions[i]));
    }
}

TEST_CASE ("Tempo and velocity fields support vertical dragging and typed values", "[native-ui]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1120, 840);
    for (const auto* id : { "tempo", "velocity" })
    {
        auto& slider = control<juce::Slider> (editor, id);
        slider.setValue (100, juce::sendNotificationSync);
        REQUIRE (slider.getWidth() <= (juce::String (id) == "tempo" ? 100 : 90));
        const juce::Point<float> origin { 20, 10 };
        const auto pointer = [&slider, origin] (juce::Point<float> point, bool dragged)
        {
            return juce::MouseEvent { juce::Desktop::getInstance().getMainMouseSource(), point,
                juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                &slider, &slider, juce::Time::getCurrentTime(), origin, juce::Time::getCurrentTime(), 1, dragged };
        };
        slider.mouseDown (pointer (origin, false));
        CHECK (slider.getValue() == Catch::Approx (100));
        slider.mouseDrag (pointer (origin.translated (30, 0), true));
        CHECK (slider.getValue() == Catch::Approx (100));
        slider.mouseDrag (pointer (origin.translated (0, -12), true));
        const int draggedValue = juce::String (id) == "tempo" ? 103 : 112;
        CHECK (slider.getValue() == Catch::Approx (draggedValue));
        slider.mouseUp (pointer (origin.translated (0, -12), true));
        CHECK (static_cast<int> (processor.getTimelineState()[id]) == draggedValue);
        slider.showTextBox();
        auto* label = dynamic_cast<juce::Label*> (slider.getChildComponent (0));
        REQUIRE (label != nullptr);
        REQUIRE (label->getCurrentTextEditor() != nullptr);
        label->getCurrentTextEditor()->setText ("91");
        slider.hideTextBox (false);
        CHECK (slider.getValue() == Catch::Approx (91));
        CHECK (static_cast<int> (processor.getTimelineState()[id]) == 91);
    }
}

TEST_CASE ("Native switches and dependent controls follow engine state", "[native-ui]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    CHECK_FALSE (control<juce::Slider> (editor, "arp.gate").isEnabled());
    toggle (editor, "arp.enabled", true);
    CHECK (control<juce::Slider> (editor, "arp.gate").isEnabled());
    toggle (editor, "sequencerEnabled", false);
    CHECK_FALSE (static_cast<bool> (processor.getTimelineState()["sequencerEnabled"]));
    CHECK_FALSE (control<juce::TextButton> (editor, "play").isEnabled());
    toggle (editor, "arp.hold", true);
    CHECK (static_cast<bool> (processor.getTimelineState()["arp"]["hold"]));
    CHECK_FALSE ((control<ModulePower> (editor, "arp.velocityEnabled").isEnabled() && control<ModulePower> (editor, "arp.velocityEnabled").getToggleState()));
    toggle (editor, "arp.velocityEnabled", true);
    CHECK (control<ModulePower> (editor, "arp.velocityEnabled").getToggleState());
    CHECK (findControl (editor, "arp.velocityRetrigger") == nullptr);
    CHECK (static_cast<bool> (processor.getTimelineState()["arp"]["velocityRetrigger"]));
    CHECK_FALSE (control<juce::Slider> (editor, "arp.interval").isEnabled());
    for (int retrigger = 0; retrigger < 3; ++retrigger)
    {
        choose (editor, "arp.retrigger", retrigger + 1);
        CHECK (static_cast<int> (processor.getTimelineState()["arp"]["retrigger"]) == retrigger);
        CHECK (control<juce::Slider> (editor, "arp.interval").isEnabled() == (retrigger == 2));
    }
    choose (editor, "arp.sync", 2);
    CHECK_FALSE (static_cast<bool> (processor.getTimelineState()["arp"]["sync"]));
    CHECK_FALSE (control<juce::ComboBox> (editor, "arp.rate").getParentComponent()->isVisible());
    CHECK (control<juce::Slider> (editor, "arp.freeMs").getParentComponent()->isVisible());
    toggle (editor, "arp.enabled", false);
    CHECK_FALSE ((control<ModulePower> (editor, "arp.velocityEnabled").isEnabled() && control<ModulePower> (editor, "arp.velocityEnabled").getToggleState()));
    CHECK_FALSE (control<juce::Slider> (editor, "arp.interval").isEnabled());
}

TEST_CASE ("Native drafts preserve playback and invalid edits never replace the engine state", "[native-ui]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    auto& text = control<juce::TextEditor> (editor, "progression");
    text.setText ("Dm9 - G13b9 - Cmaj9", false);
    text.onTextChange();
    CHECK (processor.getTimelineState()["progression"].toString() == "C - G - Am - F");
    control<juce::TextButton> (editor, "play").onClick();
    CHECK (processor.getTimelineState()["progression"].toString() == text.getText());
    CHECK (static_cast<bool> (processor.getTransportState()["armed"]));
    const auto valid = processor.getTimelineState()["progression"].toString();
    text.setText ("Not a chord!", false);
    text.onTextChange();
    control<juce::TextButton> (editor, "apply").onClick();
    CHECK (editor.controller.error.isNotEmpty());
    CHECK (processor.getTimelineState()["progression"].toString() == valid);
    CHECK (static_cast<bool> (processor.getTransportState()["armed"]));
    control<juce::Slider> (editor, "tempo").setValue (150, juce::sendNotificationSync);
    CHECK (text.getText() == "Not a chord!");
    CHECK (processor.getTimelineState()["progression"].toString() == valid);
    control<juce::TextButton> (editor, "panic").onClick();
    CHECK_FALSE (static_cast<bool> (processor.getTransportState()["armed"]));
    control<juce::TextButton> (editor, "play").onClick();
    CHECK_FALSE (static_cast<bool> (processor.getTransportState()["armed"]));
}

TEST_CASE ("Native state updates preserve custom rates and reject invalid octave changes", "[native-ui]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    auto state = processor.getTimelineState();
    state.getDynamicObject()->setProperty ("progression", "Bmaj9");
    state.getDynamicObject()->setProperty ("octave", 7);
    state["arp"].getDynamicObject()->setProperty ("rate", 3.25);
    REQUIRE (processor.setTimelineState (state).wasOk());
    editor.controller.poll();
    CHECK (control<juce::TextEditor> (editor, "progression").getText() == "Bmaj9");
    CHECK (control<juce::ComboBox> (editor, "arp.rate").getText().contains ("3.25"));
    choose (editor, "octave", 9);
    CHECK (editor.controller.error.isNotEmpty());
    CHECK (static_cast<int> (processor.getTimelineState()["octave"]) == 7);
    CHECK (control<juce::ComboBox> (editor, "octave").getSelectedId() == 8);
}

TEST_CASE ("Native edits round trip through existing projects and restore stopped", "[native-ui]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.controller.draft = "Dm9@1/2 G13@1/2 Cmaj9@2";
    REQUIRE (editor.controller.applyDraft());
    toggle (editor, "arp.enabled", true);
    toggle (editor, "arp.hold", true);
    choose (editor, "arp.style", 6);
    control<juce::Slider> (editor, "arp.steps").setValue (1, juce::sendNotificationSync);
    control<juce::TextButton> (editor, "play").onClick();
    juce::MemoryBlock data;
    processor.getStateInformation (data);
    PluginProcessor restored;
    restored.setStateInformation (data.getData(), static_cast<int> (data.getSize()));
    const auto state = restored.getTimelineState();
    CHECK (state["progression"].toString() == editor.controller.draft);
    CHECK (static_cast<bool> (state["arp"]["enabled"]));
    CHECK (static_cast<bool> (state["arp"]["hold"]));
    CHECK (static_cast<int> (state["arp"]["style"]) == 5);
    CHECK (static_cast<int> (state["arp"]["distance"]) == 12);
    CHECK_FALSE (static_cast<bool> (restored.getTransportState()["armed"]));
}

TEST_CASE ("Native glossary filters shared vocabulary and inserts the selected root", "[native-ui]")
{
    juce::String inserted;
    ChordGlossary glossary ([&] (juce::String chord) { inserted = chord; });
    REQUIRE (glossary.getNumRows() == chordrunner::chordVocabulary().size());
    auto& search = control<juce::TextEditor> (glossary, "glossary.search");
    search.setText ("half-diminished", false);
    search.onTextChange();
    CHECK (glossary.getNumRows() > 0);
    auto& root = control<juce::ComboBox> (glossary, "glossary.root");
    root.setSelectedId (4, juce::dontSendNotification); // D
    root.onChange();
    control<juce::TextButton> (glossary, "glossary.insert").onClick();
    CHECK (inserted.startsWith ("D"));
    chordrunner::Timeline timeline;
    CHECK (chordrunner::parseProgression (inserted, timeline).wasOk());
    search.setText ("does-not-exist", false);
    search.onTextChange();
    CHECK (glossary.getNumRows() == 0);
    CHECK_FALSE (control<juce::TextButton> (glossary, "glossary.insert").isEnabled());
}

TEST_CASE ("Native editor fits supported sizes and uses the engine timeline", "[native-ui]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    for (const auto size : { juce::Point<int> (760, 540), juce::Point<int> (1120, 900), juce::Point<int> (2560, 1600) })
    {
        editor.setSize (size.x, size.y);
        for (const auto* id : { "voiceLeading" })
        {
            const auto* action = findControl (editor, id);
            REQUIRE (action != nullptr);
            CHECK (action->getParentComponent()->getLocalBounds().contains (action->getParentComponent()->getLocalArea (action, action->getLocalBounds())));
        }
        for (const auto* id : { "progression", "tempo", "arp.style", "arp.freeMs", "arp.interval", "arp.envelope.display" })
        {
            const auto* component = findControl (editor, id);
            REQUIRE (component != nullptr);
            CHECK ((component->getComponentID() == "arp.interval" ? component->getParentComponent()->getWidth() : component->getWidth()) >= 65);
            CHECK (component->getHeight() >= 20);
            INFO (id << " at " << size.x << " x " << size.y);
            if (component->isVisible())
                CHECK (component->getParentComponent()->getLocalBounds().contains (
                    component->getParentComponent()->getLocalArea (component, component->getLocalBounds())));
        }
    }
    auto& timeline = control<TimelineView> (editor, "timeline");
    CHECK (timeline.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    CHECK (timeline.selectedChord() == 1);
    CHECK (editor.controller.state["chords"][1]["notes"][0] == processor.getTimelineState()["chords"][1]["notes"][0]);
    CHECK_FALSE (editor.keyPressed (juce::KeyPress (juce::KeyPress::spaceKey), &control<juce::TextEditor> (editor, "progression")));
    CHECK_FALSE (static_cast<bool> (processor.getTransportState()["armed"]));
}

TEST_CASE ("Phone pages fit portrait and landscape without scrolling or losing controls", "[native-ui][phone]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    toggle (editor, "arp.enabled", true);
    for (const auto size : { juce::Point<int> (320, 568), juce::Point<int> (390, 844), juce::Point<int> (568, 320), juce::Point<int> (844, 390) })
    {
        editor.setSize (size.x, size.y);
        CHECK_FALSE (control<juce::Viewport> (editor, "body.viewport").isVisible());
        for (int page = 0; page < 5; ++page)
        {
            control<juce::TextButton> (editor, "phone.page." + juce::String (page)).onClick();
            CHECK (editor.getLocalBounds().contains (control<juce::TextButton> (editor, "play").getBounds()));
            CHECK (editor.getLocalBounds().contains (control<juce::TextButton> (editor, "panic").getBounds()));
            const juce::StringArray ids = page == 0 ? juce::StringArray { "pattern.1", "pattern.8", "launch", "tempo", "clock" }
                : page == 1 ? juce::StringArray { "progression", "apply", "octave", "velocity", "voiceLeading" }
                : page == 2 ? juce::StringArray { "arp.style", "arp.rate", "arp.gate", "arp.steps", "arp.offset", "arp.interval" }
                : page == 4 ? juce::StringArray { "rhythm.gate.1", "rhythm.gate.8", "rhythm.tie.1", "rhythm.tie.8" }
                : juce::StringArray { "arp.swing", "arp.swingSubdivision", "arp.velocityHumanize", "arp.timingHumanize", "arp.envelope.display" };
            for (const auto& id : ids)
            {
                const auto* component = findControl (editor, id);
                REQUIRE (component != nullptr);
                const auto bounds = editor.getLocalArea (component, component->getLocalBounds());
                INFO (size.x << " x " << size.y << ", page " << page << ", " << id);
                CHECK (editor.getLocalBounds().withTrimmedBottom (46).contains (bounds));
                CHECK (bounds.getHeight() >= 22);
                CHECK (bounds.getWidth() >= 40);
            }
        }
    }
    editor.setSize (1120, 800);
    CHECK (control<juce::Viewport> (editor, "body.viewport").isVisible());
    CHECK_FALSE (control<juce::TextButton> (editor, "phone.page.0").isVisible());
    CHECK (control<juce::TextButton> (editor, "sequencerEnabled").getParentComponent()->getComponentID() == "sequence.module");
    CHECK (control<juce::TextButton> (editor, "arp.enabled").getParentComponent()->getComponentID() == "arp.module");
}

TEST_CASE ("iPad fits both modules within the safe viewport", "[native-ui][tablet]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    for (const auto size : { juce::Point<int> (1024, 724), juce::Point<int> (1024, 768), juce::Point<int> (768, 980) })
    {
        editor.setSize (size.x, size.y);
        auto& viewport = control<juce::Viewport> (editor, "body.viewport");
        REQUIRE (viewport.getViewedComponent() != nullptr);
        CHECK (viewport.getViewedComponent()->getHeight() <= viewport.getHeight());
        CHECK (viewport.getViewPositionY() == 0);
    }
}

TEST_CASE ("Open sequence and arp split the viewport without vertical scrolling", "[native-ui][layout]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    toggle (editor, "arp.enabled", true);
    for (const auto size : { juce::Point<int> (760, 540), juce::Point<int> (900, 650),
             juce::Point<int> (1024, 724), juce::Point<int> (1120, 840), juce::Point<int> (1400, 1000),
             juce::Point<int> (2560, 1600) })
    {
        INFO (size.x << " x " << size.y);
        editor.setSize (size.x, size.y);
        auto& viewport = control<juce::Viewport> (editor, "body.viewport");
        const auto& sequence = control<ModuleCard> (editor, "sequence.module");
        const auto& arp = control<ModuleCard> (editor, "arp.module");
        REQUIRE (viewport.getViewedComponent() != nullptr);
        CHECK (viewport.getViewedComponent()->getHeight() == viewport.getHeight());
        CHECK (viewport.getViewedComponent()->getWidth() == viewport.getWidth());
        CHECK_FALSE (viewport.getVerticalScrollBar().isVisible());
        CHECK (viewport.getViewPositionY() == 0);
        CHECK (std::abs (sequence.getHeight() - arp.getHeight()) <= 1);
        CHECK (sequence.getY() == ModuleCard::verticalMargin);
        CHECK (arp.getY() - sequence.getBottom() == ModuleCard::moduleGap);
        CHECK (arp.getBottom() == viewport.getHeight() - ModuleCard::verticalMargin);
        CHECK_FALSE (control<ArpVisualizer> (editor, "arp.visualizer").isVisible());
        for (const auto* id : { "progression", "apply", "velocity", "voiceLeading", "timeline",
                 "arp.style", "arp.retrigger", "arp.interval", "arp.velocityEnabled", "arp.envelope.display", "rhythm.gate", "rhythm.tie" })
        {
            const auto* component = findControl (editor, id);
            REQUIRE (component != nullptr);
            const auto& module = sequence.isParentOf (component) ? sequence : arp;
            const auto bounds = module.getLocalArea (component, component->getLocalBounds());
            CHECK (bounds.getHeight() > 0);
            CHECK (module.getLocalBounds().expanded (1).contains (bounds));
        }
        if (size.x == 1120 || size.x == 760)
        {
            const auto image = editor.createComponentSnapshot (editor.getLocalBounds());
            juce::FileOutputStream stream (juce::File ("/tmp/chord-runner-split-" + juce::String (size.x) + ".png"));
            stream.setPosition (0); stream.truncate();
            juce::PNGImageFormat().writeImageToStream (image, stream);
        }
    }
}

TEST_CASE ("Rhythm cycle handles and cells edit the saved lane with undo", "[native-ui][rhythm]")
{
    PluginProcessor processor;
    const auto defaults = processor.getTimelineState()["arp"];
    CHECK (static_cast<int> (defaults["gateLength"]) == 4);
    CHECK (static_cast<int> (defaults["tieLength"]) == 4);
    CHECK (static_cast<int> (defaults["gateMask"]) == 15);
    CHECK (static_cast<int> (defaults["tieMask"]) == 0);
    NativeEditor editor (processor);
    editor.setSize (390, 844);
    toggle (editor, "arp.enabled", true);
    control<juce::TextButton> (editor, "phone.page.4").onClick();
    auto& handle = control<juce::Component> (editor, "rhythm.gate.length");
    REQUIRE (handle.keyPressed (juce::KeyPress (juce::KeyPress::leftKey)));
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateLength"]) == 3);
    editor.controller.undo();
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateLength"]) == 4);
    control<juce::TextButton> (editor, "rhythm.gate.2").onClick();
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateMask"]) == 13);
}

TEST_CASE ("Voice-leading choice updates the native sequence header and actual timeline", "[native-ui][voicing]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (390, 844);
    control<juce::TextButton> (editor, "phone.page.1").onClick();
    for (int mode = 0; mode < 4; ++mode)
    {
        auto& button = control<juce::TextButton> (editor, "voiceLeading." + juce::String (mode));
        CHECK (button.isVisible());
        button.onClick();
        CHECK (static_cast<int> (processor.getTimelineState()["voiceLeading"]) == mode);
        for (int other = 0; other < 4; ++other)
            CHECK (control<juce::TextButton> (editor, "voiceLeading." + juce::String (other)).getToggleState() == (other == mode));
    }
    editor.controller.undo();
    CHECK (control<juce::TextButton> (editor, "voiceLeading.0").getToggleState());
    editor.controller.redo();
    CHECK (control<juce::TextButton> (editor, "voiceLeading.3").getToggleState());
}

TEST_CASE ("Manually folding sequence reclaims space and previews production MIDI", "[native-ui][arp-preview]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1400, 1000);
    toggle (editor, "arp.enabled", true);
    const auto original = findControl (editor, "arp.style")->getY();
    toggle (editor, "sequencerEnabled", false);
    control<ModuleCard> (editor, "sequence.module").onHeaderDoubleClick();
    auto& preview = control<ArpVisualizer> (editor, "arp.visualizer");
    CHECK (preview.isVisible());
    CHECK_FALSE (control<juce::TextEditor> (editor, "progression").isVisible());
    CHECK (control<juce::TextButton> (editor, "sequencerEnabled").getParentComponent()->getHeight() == ModuleCard::headerHeight);
    CHECK (findControl (editor, "arp.style")->getY() > original);
    REQUIRE_FALSE (preview.previewNotes().empty());
    for (const auto& note : preview.previewNotes()) CHECK (note.end >= note.start);
    const auto beforeSequenceEdit = preview.previewNotes();
    editor.controller.draft = "F#m7 - D - E";
    REQUIRE (editor.controller.applyDraft());
    REQUIRE (preview.previewNotes().size() == beforeSequenceEdit.size());
    for (size_t i = 0; i < beforeSequenceEdit.size(); ++i)
    {
        CHECK (preview.previewNotes()[i].note == beforeSequenceEdit[i].note);
        CHECK (preview.previewNotes()[i].start == Catch::Approx (beforeSequenceEdit[i].start));
        CHECK (preview.previewNotes()[i].velocity == beforeSequenceEdit[i].velocity);
    }
    control<juce::Slider> (editor, "arp.steps").setValue (1, juce::sendNotificationSync);
    CHECK (std::any_of (preview.previewNotes().begin(), preview.previewNotes().end(), [] (const auto& n) { return n.note >= 60; }));
    choose (editor, "arp.style", 3);
    choose (editor, "arp.rate", 4);
    choose (editor, "arp.retrigger", 3);
    toggle (editor, "arp.velocityEnabled", true);
    REQUIRE (editor.controller.set ("target", 42, true));
    control<juce::Slider> (editor, "arp.gate").setValue (70, juce::sendNotificationSync);
    control<juce::Slider> (editor, "arp.swing").setValue (18, juce::sendNotificationSync);
    // Match the reported ambiguity: a two-beat Down cycle in a four-beat restart window.
    choose (editor, "arp.style", 2);
    control<juce::Slider> (editor, "arp.steps").setValue (0, juce::sendNotificationSync);
    const auto image = editor.createComponentSnapshot (editor.getLocalBounds());
    juce::FileOutputStream stream (juce::File ("/tmp/chord-runner-arp-preview.png"));
    stream.setPosition (0); stream.truncate();
    juce::PNGImageFormat().writeImageToStream (image, stream);
    toggle (editor, "sequencerEnabled", true);
    control<ModuleCard> (editor, "sequence.module").onHeaderDoubleClick();
    CHECK_FALSE (preview.isVisible());
    CHECK (control<juce::TextEditor> (editor, "progression").isVisible());
}

TEST_CASE ("Phone arp preview adapts to touch space in both orientations", "[native-ui][arp-preview][phone]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    toggle (editor, "arp.enabled", true);
    toggle (editor, "sequencerEnabled", false);
    choose (editor, "arp.retrigger", 3);
    choose (editor, "arp.rate", 4);
    for (const auto size : { juce::Point<int> (390, 844), juce::Point<int> (320, 568), juce::Point<int> (844, 390) })
    {
        editor.setSize (size.x, size.y);
        control<juce::TextButton> (editor, "phone.page.2").onClick();
        auto& preview = control<ArpVisualizer> (editor, "arp.visualizer");
        auto& button = control<juce::TextButton> (editor, "arp.preview.toggle");
        if (button.isVisible())
        {
            button.setToggleState (true, juce::dontSendNotification); button.onClick();
        }
        CHECK (preview.isVisible());
        CHECK (editor.getLocalBounds().contains (editor.getLocalArea (&preview, preview.getLocalBounds())));
        CHECK (preview.getHeight() >= 180);
        const auto image = editor.createComponentSnapshot (editor.getLocalBounds());
        juce::FileOutputStream stream (juce::File ("/tmp/chord-runner-phone-" + juce::String (size.x) + ".png"));
        stream.setPosition (0); stream.truncate();
        juce::PNGImageFormat().writeImageToStream (image, stream);
        button.setToggleState (false, juce::dontSendNotification); button.onClick();
        CHECK (findControl (editor, "arp.style")->isVisible());
    }
}

TEST_CASE ("Abstract arp cycle is independent of the sequence and fits one traversal", "[arp-preview]")
{
    ArpVisualizer preview;
    chordrunner::ArpeggiatorSettings settings;
    preview.update (settings, 120);
    REQUIRE (preview.previewNotes().size() == 4);
    CHECK (preview.windowBeats() == Catch::Approx (1));
    settings.style = chordrunner::ArpeggiatorSettings::down;
    preview.update (settings, 120);
    CHECK (preview.previewNotes().front().note > preview.previewNotes().back().note);
    settings.steps = 1;
    preview.update (settings, 120);
    CHECK (preview.previewNotes().size() == 8);
    CHECK (preview.windowBeats() == Catch::Approx (2));
    settings.retrigger = 2;
    settings.interval = 0.5;
    preview.update (settings, 120);
    CHECK (preview.windowBeats() == Catch::Approx (0.5));
    CHECK (preview.previewNotes().size() == 2);
    settings.retrigger = 0;
    settings.velocityEnabled = true;
    settings.target = 20;
    settings.decay = 200;
    settings.gate = 40;
    preview.update (settings, 120);
    CHECK (preview.previewNotes().back().velocity == 20);
    CHECK (preview.previewNotes().front().end - preview.previewNotes().front().start == Catch::Approx (0.1).margin (0.001));
}

TEST_CASE ("Beat preview separates restart windows from order cycles and preserves envelopes", "[arp-preview]")
{
    ArpVisualizer preview;
    chordrunner::ArpeggiatorSettings settings;
    settings.style = chordrunner::ArpeggiatorSettings::down;
    settings.rate = 0.5;
    settings.retrigger = 2;
    settings.interval = 4;
    settings.velocityEnabled = true;
    settings.target = 20;
    settings.decay = 2000;
    preview.update (settings, 120);
    CHECK (preview.windowBeats() == Catch::Approx (4));
    CHECK (preview.orderCycleBeats() == Catch::Approx (2));
    REQUIRE (preview.previewNotes().size() == 8);
    CHECK (preview.previewNotes()[0].velocity == 100);
    CHECK (preview.previewNotes()[4].velocity == 60);
    CHECK (preview.previewNotes()[4].note == preview.previewNotes()[0].note);
    settings.repeats = 1;
    preview.update (settings, 120);
    CHECK (preview.windowBeats() == Catch::Approx (4));
    REQUIRE (preview.previewNotes().size() == 4);
    CHECK (preview.previewNotes().back().end == Catch::Approx (2));
    settings.interval = 1;
    preview.update (settings, 120);
    CHECK (preview.windowBeats() == Catch::Approx (1));
    CHECK (preview.previewNotes().size() == 2);
    settings.retrigger = 0;
    preview.update (settings, 120);
    CHECK (preview.windowBeats() == Catch::Approx (2));
}

TEST_CASE ("Abstract timing follows production swing, gate masks and free-rate tempo", "[arp-preview]")
{
    ArpVisualizer preview;
    chordrunner::ArpeggiatorSettings settings;
    settings.rate = 0.5;
    settings.retrigger = 2;
    settings.interval = 4;
    settings.swing = 50;
    settings.swingSubdivision = 0;
    settings.gate = 50;
    preview.update (settings, 120);
    REQUIRE (preview.previewNotes().size() == 8);
    CHECK (preview.previewNotes()[1].start == Catch::Approx (0.625));
    CHECK (preview.previewNotes()[1].end == Catch::Approx (0.875));
    settings.swing = 0;
    settings.gateSequenceEnabled = true;
    settings.gateLength = 4;
    settings.gateMask = 5;
    preview.update (settings, 120);
    REQUIRE (preview.previewNotes().size() == 4);
    CHECK (preview.previewNotes()[1].start == Catch::Approx (1));
    settings.gateSequenceEnabled = false;
    settings.sync = false;
    settings.freeMs = 250;
    preview.update (settings, 120);
    CHECK (preview.windowBeats() == Catch::Approx (4));
    CHECK (preview.orderCycleBeats() == Catch::Approx (2));
    CHECK (preview.previewNotes().size() == 8);
    preview.update (settings, 60);
    CHECK (preview.windowBeats() == Catch::Approx (4));
    CHECK (preview.orderCycleBeats() == Catch::Approx (1));
    CHECK (preview.previewNotes().size() == 16);
}

TEST_CASE ("Host tempo changes refresh the abstract free-rate window without a state edit", "[arp-preview][native-ui]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1120, 800);
    toggle (editor, "arp.enabled", true);
    toggle (editor, "sequencerEnabled", false);
    choose (editor, "clock", 2);
    choose (editor, "arp.sync", 2);
    choose (editor, "arp.retrigger", 3);
    control<juce::Slider> (editor, "arp.freeMs").setValue (250, juce::sendNotificationSync);
    auto& preview = control<ArpVisualizer> (editor, "arp.visualizer");
    editor.controller.transport.getDynamicObject()->setProperty ("hostAvailable", true);
    editor.controller.transport.getDynamicObject()->setProperty ("tempo", 120.0);
    editor.sync();
    CHECK (preview.orderCycleBeats() == Catch::Approx (2));
    const int revision = editor.controller.refreshSerial;
    editor.controller.transport.getDynamicObject()->setProperty ("tempo", 60.0);
    editor.sync();
    CHECK (editor.controller.refreshSerial == revision);
    CHECK (preview.orderCycleBeats() == Catch::Approx (1));
    CHECK (preview.windowBeats() == Catch::Approx (4));
}

TEST_CASE ("Restart interval uses a readable whole-beat stepper", "[native-ui][arp-preview]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1120, 800);
    toggle (editor, "arp.enabled", true);
    choose (editor, "arp.retrigger", 3);
    auto& slider = control<juce::Slider> (editor, "arp.interval");
    auto& minus = control<juce::TextButton> (editor, "arp.interval.decrease");
    auto& plus = control<juce::TextButton> (editor, "arp.interval.increase");
    CHECK (slider.getTextFromValue (4) == "4 beats");
    CHECK (slider.getTextFromValue (1) == "1 beat");
    CHECK (slider.getValueFromText ("3.7 beats") == Catch::Approx (4));
    CHECK (minus.getHeight() >= 28);
    CHECK (minus.getWidth() >= 28);
    CHECK (minus.getY() == slider.getY());
    CHECK (minus.getHeight() == slider.getHeight());
    CHECK (plus.getHeight() == minus.getHeight());
    plus.onClick();
    CHECK (static_cast<double> (processor.getTimelineState()["arp"]["interval"]) == Catch::Approx (5));
    minus.onClick();
    CHECK (slider.getValue() == Catch::Approx (4));
    // Restoring old fractional sessions remains lossless; the next user step
    // lands on a neighboring whole beat rather than silently rewriting state.
    slider.setValue (3.5, juce::sendNotificationSync);
    CHECK (slider.getTextFromValue (slider.getValue()) == "3.5 beats");
    plus.onClick();
    CHECK (slider.getValue() == Catch::Approx (4));
    slider.setValue (1, juce::sendNotificationSync);
    CHECK_FALSE (minus.isEnabled());
    slider.setValue (64, juce::sendNotificationSync);
    CHECK_FALSE (plus.isEnabled());
    slider.keyPressed (juce::KeyPress (juce::KeyPress::leftKey));
    CHECK (slider.getValue() == Catch::Approx (63));
    choose (editor, "arp.retrigger", 1);
    CHECK_FALSE (minus.isEnabled());
    CHECK_FALSE (plus.isEnabled());
}

TEST_CASE ("Abstract arp ranks follow live MIDI key counts and fall back to four on release", "[arp-preview][native-ui]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1120, 900);
    toggle (editor, "arp.enabled", true);
    toggle (editor, "sequencerEnabled", false);
    processor.setRateAndBufferSizeDetails (48000, 512);
    processor.prepareToPlay (48000, 512);
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    auto& preview = control<ArpVisualizer> (editor, "arp.visualizer");
    CHECK (preview.inputRanks() == 4);
    for (const int count : { 1, 2, 3, 5, 6 })
    {
        midi.clear();
        for (int i = 0; i < count; ++i)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60 + i, static_cast<juce::uint8> (100)), 0);
        processor.processBlock (audio, midi);
        editor.controller.poll();
        CHECK (preview.inputRanks() == count);
        CHECK (preview.orderCycleBeats() == Catch::Approx (count * 0.25));
        CHECK (preview.previewNotes().size() == static_cast<size_t> (count));
        // Actual pitches can change while the anonymous rank diagram stays the same.
        const auto abstract = preview.previewNotes();
        midi.clear();
        for (int i = 0; i < count; ++i)
            midi.addEvent (juce::MidiMessage::noteOff (1, 60 + i), 0);
        for (int i = 0; i < count; ++i)
            midi.addEvent (juce::MidiMessage::noteOn (1, 80 + i, static_cast<juce::uint8> (100)), 1);
        processor.processBlock (audio, midi);
        editor.controller.poll();
        REQUIRE (preview.previewNotes().size() == abstract.size());
        for (size_t i = 0; i < abstract.size(); ++i) CHECK (preview.previewNotes()[i].note == abstract[i].note);
        midi.clear();
        for (int i = 0; i < count; ++i)
            midi.addEvent (juce::MidiMessage::noteOff (1, 80 + i), 0);
        processor.processBlock (audio, midi);
        editor.controller.poll();
        CHECK (preview.inputRanks() == 4);
    }
    toggle (editor, "arp.hold", true);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (100)), 0);
    processor.processBlock (audio, midi);
    editor.controller.poll();
    CHECK (preview.inputRanks() == 1);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    processor.processBlock (audio, midi);
    editor.controller.poll();
    CHECK (preview.inputRanks() == 4);
}

TEST_CASE ("Hosted editors hide local transport across responsive layouts", "[native-ui][header][host]")
{
    for (const auto wrapper : { juce::AudioProcessor::wrapperType_AudioUnit, juce::AudioProcessor::wrapperType_VST3,
                               juce::AudioProcessor::wrapperType_Standalone })
    {
        juce::AudioProcessor::setTypeOfNextNewPlugin (wrapper);
        PluginProcessor processor;
        juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);
        NativeEditor editor (processor);
        for (const auto size : { juce::Point<int> (1400, 900), juce::Point<int> (760, 540),
                                 juce::Point<int> (390, 844), juce::Point<int> (844, 390), juce::Point<int> (1120, 800) })
        {
            editor.setSize (size.x, size.y);
            editor.controller.poll();
            CHECK (findControl (editor, "play")->isVisible() == processor.isStandalone());
            CHECK (findControl (editor, "tempo")->getParentComponent()->isVisible() == processor.isStandalone());
            CHECK (findControl (editor, "clock")->getParentComponent()->isVisible() == processor.isStandalone());
            CHECK (findControl (editor, "launch")->getParentComponent()->isVisible());
        }
    }
}

TEST_CASE ("Desktop header consolidates transport projects and square pattern pads", "[native-ui][header]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    for (const auto size : { juce::Point<int> (760, 540), juce::Point<int> (1024, 724), juce::Point<int> (1120, 800), juce::Point<int> (1400, 1000) })
    {
        editor.setSize (size.x, size.y);
        auto& playButton = control<juce::TextButton> (editor, "play");
        CHECK (playButton.getBottom() <= (size.x >= 1264 ? 62 : 56));
        CHECK (control<juce::Slider> (editor, "tempo").getParentComponent()->getBottom() <= (size.x >= 1264 ? 62 : 56));
        const int bodyTop = control<juce::Viewport> (editor, "body.viewport").getY();
        CHECK (control<juce::ComboBox> (editor, "clock").getParentComponent()->getBottom() < bodyTop);
        CHECK (bodyTop <= (size.x < 1264 ? 126 : 88));
        CHECK (playButton.getWidth() == playButton.getHeight());
        CHECK (playButton.getWidth() == 32);
        CHECK (findControl (editor, "project.details")->getWidth() >= 160);
        if (size.x >= 1264)
            for (const auto* id : { "project.details", "project.menu", "undo", "redo" })
                CHECK (findControl (editor, id)->getBounds().getCentreY() == playButton.getBounds().getCentreY());
        for (const auto* id : { "undo", "redo" })
        {
            const auto* button = findControl (editor, id);
            CHECK (button->getWidth() == 28);
            CHECK (button->getHeight() == 28);
        }
        CHECK (findControl (editor, "pattern.1")->getY() == findControl (editor, "pattern.8")->getY());
        for (int i = 9; i <= 16; ++i)
            CHECK_FALSE (findControl (editor, "pattern." + juce::String (i))->isVisible());
        CHECK (control<juce::Viewport> (editor, "body.viewport").getY() < 206);
        for (int i = 1; i <= 8; ++i)
        {
            const auto& pad = control<juce::TextButton> (editor, "pattern." + juce::String (i));
            CHECK (pad.getWidth() == pad.getHeight());
            CHECK (pad.getWidth() >= 32);
            CHECK (pad.getWidth() <= 40);
            CHECK (editor.getLocalBounds().contains (pad.getBounds()));
            for (const auto* id : { "project.details", "project.menu", "undo", "redo" })
                CHECK_FALSE (pad.getBounds().intersects (findControl (editor, id)->getBounds()));
        }
    }
}

TEST_CASE ("Module power preserves sizes and manual folding stays under user control", "[native-ui][header]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1400, 1000);
    auto& seq = control<ModuleCard> (editor, "sequence.module");
    auto& arp = control<ModuleCard> (editor, "arp.module");
    const auto seqBounds = seq.getBounds(), arpBounds = arp.getBounds();
    CHECK (seq.getHeight() > ModuleCard::headerHeight);
    CHECK (arp.getHeight() > ModuleCard::headerHeight);
    CHECK (control<juce::Label> (editor, "voiceLeading.label").isVisible());
    for (const bool sequenceOn : { false, true })
        for (const bool arpOn : { false, true })
        {
            toggle (editor, "sequencerEnabled", sequenceOn);
            toggle (editor, "arp.enabled", arpOn);
            CHECK (seq.getBounds() == seqBounds);
            CHECK (arp.getBounds() == arpBounds);
        }
    seq.onHeaderDoubleClick();
    CHECK (seq.getHeight() == ModuleCard::headerHeight);
    const auto foldedArp = arp.getBounds();
    toggle (editor, "sequencerEnabled", false);
    toggle (editor, "sequencerEnabled", true);
    CHECK (seq.getHeight() == ModuleCard::headerHeight);
    CHECK (arp.getBounds() == foldedArp);
    CHECK (static_cast<bool> (processor.getTimelineState()["sequencerEnabled"]));
    seq.onHeaderDoubleClick();
    CHECK (seq.getBounds() == seqBounds);
    CHECK (arp.getBounds() == arpBounds);
    arp.onHeaderDoubleClick();
    const auto expandedSeq = seq.getBounds();
    toggle (editor, "arp.enabled", false);
    toggle (editor, "arp.enabled", true);
    CHECK (arp.getHeight() == ModuleCard::headerHeight);
    CHECK (seq.getBounds() == expandedSeq);
    CHECK (static_cast<bool> (processor.getTimelineState()["arp"]["enabled"]));
}

TEST_CASE ("Inline Gate and Tie share a row below arp controls and visualization", "[native-ui][rhythm]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1400, 1000);
    toggle (editor, "arp.enabled", true);
    toggle (editor, "sequencerEnabled", false);
    control<ModuleCard> (editor, "sequence.module").onHeaderDoubleClick();
    auto& gateLane = control<CycleLane> (editor, "rhythm.gate");
    auto& tieLane = control<CycleLane> (editor, "rhythm.tie");
    auto& preview = control<ArpVisualizer> (editor, "arp.visualizer");
    auto& arp = control<ModuleCard> (editor, "arp.module");
    const auto gateBounds = arp.getLocalArea (&gateLane, gateLane.getLocalBounds());
    const auto tieBounds = arp.getLocalArea (&tieLane, tieLane.getLocalBounds());
    CHECK (gateLane.isVisible());
    CHECK (tieLane.isVisible());
    CHECK (gateLane.getParentComponent()->isVisible());
    CHECK (gateBounds.getY() == tieBounds.getY());
    CHECK (gateBounds.getHeight() <= 84);
    CHECK (gateBounds.getRight() < tieBounds.getX());
    CHECK (gateBounds.getY() > 220);
    CHECK (arp.getLocalArea (&preview, preview.getLocalBounds()).getBottom() < gateBounds.getY());
    auto& power = control<ModulePower> (editor, "rhythm.gate.enabled");
    const auto& first = control<juce::TextButton> (editor, "rhythm.gate.1");
    const auto& last = control<juce::TextButton> (editor, "rhythm.gate.8");
    CHECK (first.getY() == last.getY());
    CHECK (first.getHeight() == first.getWidth());
    CHECK (first.getHeight() >= 16);
    CHECK (power.getRight() < first.getX());
    CHECK (power.getBounds().getCentreY() == Catch::Approx (first.getBounds().getCentreY()).margin (2));
    CHECK_FALSE (control<juce::TextButton> (editor, "rhythm.gate.9").isVisible());

    power.setToggleState (true, juce::dontSendNotification); power.onClick();
    control<juce::TextButton> (editor, "rhythm.gate.2").onClick();
    CHECK ((static_cast<int> (processor.getTimelineState()["arp"]["gateMask"]) & 2) == 0);
    editor.setSize (1120, 900);
    auto* row = gateLane.getParentComponent();
    const auto image = row->createComponentSnapshot (row->getLocalBounds());
    juce::FileOutputStream stream (juce::File ("/tmp/chord-runner-rhythm-compact.png"));
    stream.setPosition (0); stream.truncate();
    juce::PNGImageFormat().writeImageToStream (image, stream);

}

TEST_CASE ("Enabled rhythm lanes preview their complete first cycle across beat restarts", "[arp-preview]")
{
    ArpVisualizer preview;
    chordrunner::ArpeggiatorSettings settings;
    settings.retrigger = 2;
    settings.interval = 4;
    settings.rate = 0.5;
    settings.gateSequenceEnabled = true;
    settings.gateLength = 16;
    settings.gateMask = 0x5555;
    preview.update (settings, 120);
    CHECK (preview.windowBeats() == Catch::Approx (8));
    REQUIRE (preview.previewNotes().size() == 8);
    CHECK (preview.previewNotes().back().start == Catch::Approx (7));
    settings.gateMask = 0xffff;
    settings.tieSequenceEnabled = true;
    settings.tieLength = 16;
    settings.tieMask = 0xaaaa;
    preview.update (settings, 120);
    REQUIRE (preview.previewNotes().size() == 8);
    CHECK (preview.previewNotes().front().end == Catch::Approx (1));
    settings.gateSequenceEnabled = settings.tieSequenceEnabled = false;
    preview.update (settings, 120);
    CHECK (preview.windowBeats() == Catch::Approx (4));
    CHECK (preview.previewNotes().size() == 8);
    settings.gateSequenceEnabled = settings.tieSequenceEnabled = true;
    settings.gateMask = 0xdddd;
    settings.tieMask = 0x8888;
    settings.velocityEnabled = true;
    settings.target = 16;
    settings.decay = 2000;
    preview.setSize (1080, 400);
    preview.update (settings, 120);
    const auto image = preview.createComponentSnapshot (preview.getLocalBounds());
    juce::FileOutputStream stream (juce::File ("/tmp/chord-runner-rhythm-preview.png"));
    stream.setPosition (0); stream.truncate();
    juce::PNGImageFormat().writeImageToStream (image, stream);

}

TEST_CASE ("Sequence source settings belong to the sequence panel and collapse with it", "[native-ui]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1120, 900);
    for (const auto* id : { "velocity", "octave" })
    {
        auto* component = findControl (editor, id);
        REQUIRE (component != nullptr);
        CHECK (component->getParentComponent()->getParentComponent()->getComponentID() == "sequence.module");
        CHECK (component->getParentComponent()->isVisible());
    }
    const auto* octaveControl = findControl (editor, "octave")->getParentComponent();
    for (const auto* id : { "voiceLeading" })
    {
        const auto* action = findControl (editor, id);
        REQUIRE (action != nullptr);
        CHECK (action->getRight() < findControl (editor, "velocity")->getParentComponent()->getX());
        CHECK (action->getY() >= octaveControl->getY());
        CHECK (action->getBottom() <= octaveControl->getBottom());
    }
    toggle (editor, "arp.enabled", true);
    toggle (editor, "sequencerEnabled", false);
    control<ModuleCard> (editor, "sequence.module").onHeaderDoubleClick();
    for (const auto* id : { "velocity", "octave" })
        CHECK_FALSE (findControl (editor, id)->getParentComponent()->isVisible());
    const auto& dictionary = control<juce::TextButton> (editor, "glossary.open");
    CHECK (dictionary.getParentComponent() == &editor);
    CHECK (dictionary.isVisible());
    CHECK (dictionary.getY() == findControl (editor, "tips")->getY());
    CHECK (dictionary.getRight() < findControl (editor, "tips")->getX());
}

namespace
{
juce::MouseEvent cyclePointerEvent (juce::Component& component, juce::Point<float> point, juce::Point<float> origin)
{
    return { juce::Desktop::getInstance().getMainMouseSource(), point,
        juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        &component, &component, juce::Time::getCurrentTime(), origin, juce::Time::getCurrentTime(), 1, true };
}
}
TEST_CASE ("KSH-style cycle edge follows the pointer and groups length edits for undo", "[native-ui][rhythm]")
{
    PluginProcessor processor;
    auto initial = processor.getTimelineState();
    initial["arp"].getDynamicObject()->setProperty ("gateLength", 4);
    initial["arp"].getDynamicObject()->setProperty ("gateMask", 5);
    initial["arp"].getDynamicObject()->setProperty ("gateSequenceEnabled", true);
    REQUIRE (processor.setTimelineState (initial).wasOk());
    EditorController controller (processor);
    CycleLane lane (controller, false);
    lane.setInline (true); lane.setSize (640, 60);
    const auto image = lane.createComponentSnapshot (lane.getLocalBounds());
    juce::FileOutputStream stream (juce::File ("/tmp/chord-runner-ksh-cycle.png"));
    stream.setPosition (0); stream.truncate();
    juce::PNGImageFormat().writeImageToStream (image, stream);
    auto& handle = control<juce::Component> (lane, "rhythm.gate.length");
    auto& last = control<juce::TextButton> (lane, "rhythm.gate.4");
    CHECK (handle.getX() == last.getRight() - 8);
    CHECK (handle.getY() == last.getY());
    CHECK (handle.getWidth() == 18);
    const auto origin = handle.getLocalBounds().getCentre().toFloat();
    auto dragTo = [&] (int index)
    {
        const auto& cell = control<juce::TextButton> (lane, "rhythm.gate." + juce::String (index));
        const auto point = cell.getBounds().getTopRight().toFloat() + juce::Point<float> (0, 8);
        handle.mouseDrag (cyclePointerEvent (handle, point - handle.getPosition().toFloat(), origin));
    };
    handle.mouseDown (cyclePointerEvent (handle, origin, origin));
    dragTo (8);
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateLength"]) == 8);
    dragTo (6);
    handle.mouseUp (cyclePointerEvent (handle, origin, origin));
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateLength"]) == 6);
    controller.undo(); lane.refresh();
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateLength"]) == 4);
    CHECK_FALSE (processor.canUndo());
    handle.mouseDown (cyclePointerEvent (handle, origin, origin));
    dragTo (7);
    CHECK (handle.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateLength"]) == 4);
    lane.refresh();
    auto& ninth = control<juce::TextButton> (lane, "rhythm.gate.9");
    CHECK (ninth.isEnabled());
    ninth.onClick();
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateLength"]) == 9);
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateMask"]) == 5);
    handle.mouseDown (cyclePointerEvent (handle, origin, origin));
    processor.selectPattern (1);
    dragTo (12);
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateLength"]) == 4);
}
TEST_CASE ("KSH-style cycle hits move on release and canceled drags never toggle", "[native-ui][rhythm]")
{
    PluginProcessor processor;
    auto initial = processor.getTimelineState();
    initial["arp"].getDynamicObject()->setProperty ("gateLength", 4);
    initial["arp"].getDynamicObject()->setProperty ("gateMask", 5);
    REQUIRE (processor.setTimelineState (initial).wasOk());
    EditorController controller (processor);
    CycleLane lane (controller, false);
    lane.setInline (true); lane.setSize (640, 76);
    auto& first = control<juce::Component> (lane, "rhythm.gate.1");
    auto& fourth = control<juce::TextButton> (lane, "rhythm.gate.4");
    const auto origin = first.getLocalBounds().getCentre().toFloat();
    const auto destination = fourth.getBounds().getCentre().toFloat() - first.getPosition().toFloat();
    first.mouseDown (cyclePointerEvent (first, origin, origin));
    first.mouseDrag (cyclePointerEvent (first, destination, origin));
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateMask"]) == 5);
    first.mouseUp (cyclePointerEvent (first, destination, origin));
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateMask"]) == 12);
    controller.undo(); lane.refresh();
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateMask"]) == 5);
    CHECK_FALSE (processor.canUndo());
    first.mouseDown (cyclePointerEvent (first, origin, origin));
    first.mouseDrag (cyclePointerEvent (first, destination, origin));
    CHECK (first.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    first.mouseUp (cyclePointerEvent (first, destination, origin));
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateMask"]) == 5);
    first.mouseDown (cyclePointerEvent (first, origin, origin));
    first.mouseDrag (cyclePointerEvent (first, destination, origin));
    processor.selectPattern (1); controller.refresh(); lane.refresh();
    first.mouseUp (cyclePointerEvent (first, destination, origin));
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateMask"]) == 15);
}

TEST_CASE ("Header pads arm a blinking copy source then copy and select the destination", "[native-ui][patterns]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.controller.selectPattern (4);
    editor.controller.draft = "Em7";
    REQUIRE (editor.controller.applyDraft());
    editor.controller.selectPattern (0);
    auto& source = control<PatternPad> (editor, "pattern.5");
    auto& destination = control<PatternPad> (editor, "pattern.9");
    const auto before = juce::JSON::toString (processor.getProjectState());
    source.clicked (juce::ModifierKeys (juce::ModifierKeys::shiftModifier));
    source.onClick();
    CHECK (source.copyArmed);
    CHECK (source.getAlpha() >= 0.7f);
    CHECK (source.getAlpha() <= 1.0f);
    CHECK (processor.selectedPattern() == 0);
    CHECK (juce::JSON::toString (processor.getProjectState()) == before);
    destination.clicked (juce::ModifierKeys()); destination.onClick();
    CHECK_FALSE (source.copyArmed);
    CHECK (source.getAlpha() == 1.0f);
    CHECK (processor.selectedPattern() == 8);
    CHECK (processor.getTimelineState()["progression"].toString() == "Em7");
    editor.controller.undo();
    CHECK (processor.getProjectState()["patterns"][8]["progression"].toString() == "C - G - Am - F");
    CHECK (findControl (editor, "pattern.menu") == nullptr);
}

TEST_CASE ("Pattern copy cancels with Escape mute or external edits and reset stays undoable", "[native-ui][patterns]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    auto& source = control<PatternPad> (editor, "pattern.1");
    auto arm = [&]
    {
        source.clicked (juce::ModifierKeys (juce::ModifierKeys::shiftModifier)); source.onClick();
        REQUIRE (source.copyArmed);
    };
    arm();
    CHECK (editor.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey), &source));
    CHECK_FALSE (source.copyArmed);
    arm();
    control<juce::TextButton> (editor, "pattern.mute").onClick();
    CHECK_FALSE (source.copyArmed);
    CHECK (processor.selectedPattern() == -1);
    editor.controller.selectPattern (0);
    arm();
    REQUIRE (editor.controller.set ("velocity", 90));
    CHECK_FALSE (source.copyArmed);
    editor.controller.draft = "Dm9";
    REQUIRE (editor.controller.applyDraft());
    CHECK (source.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    CHECK (processor.getTimelineState()["progression"].toString() == "C - G - Am - F");
    editor.controller.undo();
    CHECK (processor.getTimelineState()["progression"].toString() == "Dm9");
}

TEST_CASE ("Rotate Start nudges and wraps the current arp cycle", "[native-ui][arp]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    toggle (editor, "arp.enabled", true);
    auto& previous = control<juce::TextButton> (editor, "arp.offset.previous");
    auto& next = control<juce::TextButton> (editor, "arp.offset.next");
    const auto value = [&] { return static_cast<int> (processor.getTimelineState()["arp"]["offset"]); };
    previous.onClick();
    CHECK (value() == 2);
    next.onClick();
    CHECK (value() == 0);
    choose (editor, "arp.style", 3);
    control<juce::Slider> (editor, "arp.steps").setValue (2, juce::sendNotificationSync);
    previous.onClick();
    CHECK (value() == 11);
    next.onClick();
    CHECK (value() == 0);
    REQUIRE (editor.controller.set ("offset", 13, true));
    next.onClick();
    CHECK (value() == 2);
    REQUIRE (editor.controller.set ("style", 7, true));
    control<juce::Slider> (editor, "arp.steps").setValue (0, juce::sendNotificationSync);
    CHECK_FALSE (previous.isEnabled());
    CHECK_FALSE (next.isEnabled());
}

TEST_CASE ("Repeats supports tenths from one to eight then infinity", "[native-ui]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    auto& repeat = control<juce::Slider> (editor, "arp.repeats");
    CHECK (repeat.getMinimum() == 1.0);
    CHECK (repeat.getMaximum() == Catch::Approx (8.1));
    CHECK (repeat.getValue() == Catch::Approx (8.1));
    repeat.setValue (3.5, juce::sendNotificationSync);
    CHECK (static_cast<double> (processor.getTimelineState()["arp"]["repeats"]) == Catch::Approx (3.5));
    CHECK (repeat.getTextFromValue (3.5) == "3.5");
    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    PluginProcessor restored;
    restored.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    CHECK (static_cast<double> (restored.getTimelineState()["arp"]["repeats"]) == Catch::Approx (3.5));
    repeat.setValue (8, juce::sendNotificationSync);
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["repeats"]) == 8);
    repeat.setValue (9, juce::sendNotificationSync);
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["repeats"]) == 0);
    CHECK (repeat.getTextFromValue (8.1) == "Infinite");
}

TEST_CASE ("Arp octave range replaces semitone distance", "[native-ui][arp]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    auto& octaves = control<juce::Slider> (editor, "arp.steps");
    CHECK (octaves.getTextFromValue (0) == "0");
    CHECK (octaves.getTextFromValue (2) == "2");
    CHECK (findControl (editor, "arp.distance") == nullptr);
    REQUIRE (editor.controller.set ("distance", -7, true));
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["distance"]) == 12);
}

TEST_CASE ("Octave order is saved and previews the chosen note expansion", "[native-ui][arp-preview]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    choose (editor, "arp.octaveMode", 2);
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["octaveMode"]) == 1);
    juce::MemoryBlock bytes; processor.getStateInformation (bytes);
    PluginProcessor restored;
    restored.setStateInformation (bytes.getData(), static_cast<int> (bytes.getSize()));
    CHECK (static_cast<int> (restored.getTimelineState()["arp"]["octaveMode"]) == 1);
    editor.controller.undo();
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["octaveMode"]) == 0);
    ArpVisualizer preview;
    chordrunner::ArpeggiatorSettings settings;
    settings.steps = 1; settings.octaveMode = 1; settings.retrigger = 0;
    preview.update (settings, 120, 3);
    const auto& notes = preview.previewNotes();
    REQUIRE (notes.size() == 6);
    for (size_t i = 0; i < notes.size(); i += 2) CHECK (notes[i + 1].note == notes[i].note + 12);
}

TEST_CASE ("Arp modes share the top row with phrase controls", "[native-ui][layout]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    toggle (editor, "arp.enabled", true);
    for (const auto size : { juce::Point<int> (760, 540), juce::Point<int> (1120, 840), juce::Point<int> (1400, 1000) })
    {
        editor.setSize (size.x, size.y);
        const auto& module = control<ModuleCard> (editor, "arp.module");
        const auto* bank = findControl (editor, "arp.style");
        REQUIRE (bank != nullptr);
        const auto bounds = module.getLocalArea (bank, bank->getLocalBounds());
        CHECK (bounds.getWidth() > module.getWidth() * 4 / 10);
        CHECK (bounds.getWidth() < module.getWidth() / 2);
        CHECK (findControl (editor, "arp.style.7") == nullptr);
        CHECK (findControl (editor, "arp.style.10") == nullptr);
        constexpr std::array<int, 9> modes { 0, 1, 2, 3, 4, 5, 6, 8, 9 };
        for (size_t i = 0; i < modes.size(); ++i)
        {
            const auto& button = control<juce::TextButton> (editor, "arp.style." + juce::String (modes[i]));
            CHECK (button.isVisible());
            CHECK (bank->getLocalBounds().contains (button.getBounds()));
            CHECK (button.getY() == static_cast<int> (i / 3) * bank->getHeight() / 3);
        }
        for (const auto* id : { "arp.offset", "arp.repeats", "arp.retrigger", "arp.interval", "arp.steps", "arp.octaveMode" })
        {
            const auto* component = findControl (editor, id);
            REQUIRE (component != nullptr);
            const auto controlBounds = module.getLocalArea (component, component->getLocalBounds());
            CHECK (controlBounds.getX() > bounds.getRight());
            CHECK (controlBounds.getY() >= bounds.getY());
            CHECK (controlBounds.getBottom() <= bounds.getBottom() + 1);
        }
    }
}

TEST_CASE ("Rate knob preserves sync divisions free milliseconds and custom restored values", "[native-ui][rate]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1400, 1000);
    toggle (editor, "arp.enabled", true);
    auto& knob = control<juce::Slider> (editor, "arp.rate.knob");
    auto& readout = control<juce::ComboBox> (editor, "arp.rate");
    double previous = 100;
    for (int step = 0; step < 12; ++step)
    {
        knob.setValue (step, juce::sendNotificationSync);
        const double value = static_cast<double> (processor.getTimelineState()["arp"]["rate"]);
        CHECK (value < previous);
        CHECK (readout.getSelectedId() > 0);
        previous = value;
    }
    choose (editor, "arp.rate", 9); // 1/8 triplet.
    CHECK (knob.getValue() == Catch::Approx (7));
    CHECK (static_cast<double> (processor.getTimelineState()["arp"]["rate"]) == Catch::Approx (1.0 / 3));
    REQUIRE (editor.controller.set ("rate", 0.2, true));
    CHECK (readout.getSelectedId() == 0);
    CHECK (readout.getText().contains ("0.2"));
    editor.setSize (1120, 840);
    CHECK (static_cast<double> (processor.getTimelineState()["arp"]["rate"]) == Catch::Approx (0.2));
    choose (editor, "arp.sync", 2);
    auto& milliseconds = control<juce::Slider> (editor, "arp.freeMs");
    CHECK (milliseconds.isRotary());
    CHECK (milliseconds.getTextBoxPosition() == juce::Slider::TextBoxBelow);
    milliseconds.setValue (237.5, juce::sendNotificationSync);
    CHECK (milliseconds.getTextFromValue (milliseconds.getValue()).contains ("ms"));
    CHECK (static_cast<double> (processor.getTimelineState()["arp"]["freeMs"]) == Catch::Approx (237.5));
}

TEST_CASE ("Power changes keep both modules and their controls in place", "[native-ui][layout]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    const juce::StringArray ids { "arp.style.0", "arp.style.9", "arp.offset", "arp.repeats", "arp.retrigger", "arp.interval",
        "arp.steps", "arp.octaveMode", "arp.rate.knob", "arp.rate", "arp.sync", "arp.gate", "arp.swing",
        "arp.swingSubdivision", "arp.timingHumanize", "arp.velocityEnabled", "arp.envelope.display", "arp.velocityHumanize",
        "rhythm.gate.1", "rhythm.gate.8", "rhythm.tie.1", "rhythm.tie.8" };
    for (const auto size : { juce::Point<int> (760, 540), juce::Point<int> (1120, 840),
                             juce::Point<int> (1400, 1000), juce::Point<int> (2560, 1600) })
    {
        editor.setSize (size.x, size.y);
        toggle (editor, "sequencerEnabled", true);
        toggle (editor, "arp.enabled", true);
        std::vector<juce::Rectangle<int>> before;
        for (const auto& id : ids)
        {
            const auto* component = findControl (editor, id);
            REQUIRE (component != nullptr);
            before.push_back (editor.getLocalArea (component, component->getLocalBounds()));
        }
        toggle (editor, "sequencerEnabled", false);
        auto& viewport = control<juce::Viewport> (editor, "body.viewport");
        CHECK (viewport.getViewedComponent()->getHeight() == viewport.getHeight());
        CHECK (viewport.getViewPositionY() == 0);
        auto& preview = control<ArpVisualizer> (editor, "arp.visualizer");
        CHECK_FALSE (preview.isVisible());
        for (size_t i = 0; i < before.size(); ++i)
        {
            INFO (ids[static_cast<int> (i)] << " at " << size.x << " x " << size.y);
            const auto* component = findControl (editor, ids[static_cast<int> (i)]);
            const auto after = editor.getLocalArea (component, component->getLocalBounds());
            CHECK (after.getWidth() == before[i].getWidth());
            // Enclosing transformed bounds can gain a pixel after vertical translation.
            CHECK (std::abs (after.getHeight() - before[i].getHeight()) <= 1);
            CHECK (after.getX() == before[i].getX());
            CHECK (after.getY() == before[i].getY());
        }
    }
}

TEST_CASE ("Desktop editor locks its aspect and scales a fixed canvas through the footer", "[native-ui][scale]")
{
    using chordrunner::ui::UiScale;
    PluginProcessor processor;
    PluginEditor editor (processor);
    NativeEditor* canvas = nullptr;
    for (auto* child : editor.getChildren())
        if (auto* view = dynamic_cast<NativeEditor*> (child)) canvas = view;
    REQUIRE (canvas != nullptr);
    REQUIRE (editor.getConstrainer() != nullptr);
    CHECK (editor.getConstrainer()->getFixedAspectRatio() == Catch::Approx (UiScale::aspectRatio));
    CHECK (editor.getWidth() == juce::roundToInt (UiScale::canvasWidth * UiScale::factor (100)));
    CHECK (editor.getHeight() == juce::roundToInt (UiScale::canvasHeight * UiScale::factor (100)));
    toggle (*canvas, "arp.enabled", true);
    const auto projectBounds = findControl (*canvas, "project.details")->getBounds();
    const auto launchBounds = findControl (*canvas, "launch")->getParentComponent()->getBounds();
    const auto gateBounds = control<juce::Slider> (*canvas, "arp.gate").getBounds();
    const auto modeBounds = findControl (*canvas, "arp.style")->getBounds();
    for (const auto size : { juce::Point<int> (800, 600), juce::Point<int> (1500, 700), juce::Point<int> (1000, 1400),
                             juce::Point<int> (1024, 768), juce::Point<int> (768, 1024) })
    {
        editor.getConstrainer()->setBoundsForComponent (&editor, { 0, 0, size.x, size.y }, false, false, true, true);
        CHECK (static_cast<double> (editor.getWidth()) / editor.getHeight() == Catch::Approx (UiScale::aspectRatio).margin (0.003));
        CHECK (canvas->getWidth() == UiScale::canvasWidth);
        CHECK (canvas->getHeight() == UiScale::canvasHeight);
        CHECK (control<juce::Slider> (*canvas, "arp.gate").getBounds() == gateBounds);
        CHECK (findControl (*canvas, "arp.style")->getBounds() == modeBounds);
        const auto transform = canvas->getTransform();
        CHECK (transform.mat00 == Catch::Approx (transform.mat11));
    }
    auto& scale = control<ScaleControl> (*canvas, "ui.scale");
    const int before = scale.getValue();
    CHECK (scale.keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
    CHECK (processor.getUiScalePercent() == before + 1);
    CHECK (editor.getWidth() == juce::roundToInt (UiScale::canvasWidth * UiScale::factor (before + 1)));
    const auto mouse = [&scale] (juce::ModifierKeys modifiers)
    {
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 10, 10 }, modifiers,
            1, 0, 0, 0, 0, &scale, &scale, juce::Time(), { 10, 10 }, juce::Time(), 2, false);
    };
    scale.mouseDoubleClick (mouse ({}));
    auto& input = control<juce::TextEditor> (*canvas, "ui.scale.input");
    input.setText ("175", false);
    input.onReturnKey();
    CHECK (processor.getUiScalePercent() == 175);
    CHECK (editor.getWidth() == juce::roundToInt (UiScale::canvasWidth * UiScale::factor (175)));
    scale.mouseDown (mouse (juce::ModifierKeys::rightButtonModifier));
    CHECK (scale.getValue() == 100);
    CHECK (processor.getUiScalePercent() == 100);
    juce::MouseWheelDetails wheel;
    wheel.deltaY = -1;
    scale.mouseWheelMove (mouse ({}), wheel);
    CHECK (processor.getUiScalePercent() == 99);
    // Even an unconstrained host request cannot stretch or reflow the canvas.
    editor.setSize (1000, 1000);
    CHECK (canvas->getWidth() == UiScale::canvasWidth);
    CHECK (canvas->getHeight() == UiScale::canvasHeight);
    CHECK (canvas->getTransform().mat00 == Catch::Approx (canvas->getTransform().mat11));
    // iPad and AUv3 hosts supply their own bounds. Fit the same canvas without
    // asking the host to adopt the desktop window's aspect constraint.
    for (const auto size : { juce::Point<int> (1024, 768), juce::Point<int> (1024, 724), juce::Point<int> (768, 1024) })
    {
        editor.setSize (size.x, size.y);
        CHECK (canvas->getWidth() == UiScale::canvasWidth);
        CHECK (canvas->getHeight() == UiScale::canvasHeight);
        CHECK (findControl (*canvas, "project.details")->getBounds() == projectBounds);
        CHECK (findControl (*canvas, "launch")->getParentComponent()->getBounds() == launchBounds);
        CHECK (canvas->getTransform().mat00 == Catch::Approx (canvas->getTransform().mat11));
        CHECK (editor.getLocalBounds().expanded (1).contains (canvas->getBoundsInParent()));
    }
    const auto image = editor.createComponentSnapshot (editor.getLocalBounds());
    juce::FileOutputStream stream (juce::File ("/tmp/chord-runner-fixed-scale.png"));
    stream.setPosition (0); stream.truncate();
    juce::PNGImageFormat().writeImageToStream (image, stream);
}

TEST_CASE ("UI scale persists without dirtying music or entering musical undo", "[native-ui][scale][state]")
{
    PluginProcessor processor;
    processor.markDocumentClean();
    processor.setUiScalePercent (175);
    CHECK_FALSE (processor.isDocumentModified());
    CHECK_FALSE (processor.canUndo());
    juce::MemoryBlock data;
    processor.getStateInformation (data);
    PluginProcessor restored;
    restored.setStateInformation (data.getData(), static_cast<int> (data.getSize()));
    CHECK (restored.getUiScalePercent() == 175);
    juce::PropertySet preferences;
    processor.saveStandaloneSettings (preferences);
    PluginProcessor standalone;
    standalone.restoreStandaloneSettings (preferences);
    CHECK (standalone.getUiScalePercent() == 175);
    auto edit = processor.getTimelineState();
    edit["arp"].getDynamicObject()->setProperty ("swing", 25);
    REQUIRE (processor.editTimeline (edit, "arp.swing").wasOk());
    processor.setUiScalePercent (125);
    processor.undo();
    CHECK (processor.getUiScalePercent() == 125);
    auto invalid = processor.getProjectState();
    invalid.getDynamicObject()->setProperty ("uiScalePercent", 0);
    CHECK (processor.setProjectState (invalid).failed());
    CHECK (processor.getUiScalePercent() == 125);
    invalid.getDynamicObject()->setProperty ("uiScalePercent", 100.5);
    CHECK (processor.setProjectState (invalid).failed());
    processor.setUiScalePercent (10000);
    CHECK (processor.getUiScalePercent() == 400);
    processor.setUiScalePercent (-100);
    CHECK (processor.getUiScalePercent() == 50);
}
