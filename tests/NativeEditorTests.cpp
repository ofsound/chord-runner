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

TEST_CASE ("Native controls retain every numeric engine mapping", "[native-ui]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1120, 900);
    toggle (editor, "arp.enabled", true);
    toggle (editor, "arp.velocityEnabled", true);
    struct Setting { const char* id; const char* key; double value; bool arp; };
    const std::array<Setting, 12> settings {{
        Setting { "tempo", "tempo", 137, false }, { "velocity", "velocity", 93, false },
        { "arp.freeMs", "freeMs", 237.5, true }, { "arp.gate", "gate", 63.5, true },
        { "arp.swing", "swing", 17, true }, { "arp.steps", "steps", 4, true },
        { "arp.distance", "distance", -7, true }, { "arp.offset", "offset", 13, true },
        { "arp.repeats", "repeats", 8, true }, { "arp.interval", "interval", 0.0625, true },
        { "arp.target", "target", 0, true }, { "arp.decay", "decay", 4567, true }
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
    choose (editor, "channel", 7);
    choose (editor, "clock", 2);
    CHECK (static_cast<int> (processor.getTimelineState()["octave"]) == 3);
    CHECK (static_cast<int> (processor.getTimelineState()["channel"]) == 7);
    CHECK (static_cast<bool> (processor.getTimelineState()["hostSync"]));
    CHECK_FALSE (control<juce::Slider> (editor, "tempo").isEnabled());
    for (int style = 0; style < 11; ++style)
    {
        choose (editor, "arp.style", style + 1);
        CHECK (static_cast<int> (processor.getTimelineState()["arp"]["style"]) == style);
    }
    constexpr std::array<double, 12> divisions { 4, 2, 1, 0.5, 0.25, 0.125, 0.0625, 2.0 / 3, 1.0 / 3, 1.0 / 6, 0.75, 0.375 };
    for (size_t i = 0; i < divisions.size(); ++i)
    {
        choose (editor, "arp.rate", static_cast<int> (i) + 1);
        CHECK (static_cast<double> (processor.getTimelineState()["arp"]["rate"]) == Catch::Approx (divisions[i]));
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
    CHECK_FALSE (control<juce::Slider> (editor, "arp.target").isEnabled());
    toggle (editor, "arp.velocityEnabled", true);
    CHECK (control<juce::Slider> (editor, "arp.target").isEnabled());
    toggle (editor, "arp.velocityRetrigger", false);
    CHECK_FALSE (static_cast<bool> (processor.getTimelineState()["arp"]["velocityRetrigger"]));
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
    CHECK_FALSE (control<juce::Slider> (editor, "arp.target").isEnabled());
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
    control<juce::Slider> (editor, "arp.distance").setValue (-12, juce::sendNotificationSync);
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
    CHECK (static_cast<int> (state["arp"]["distance"]) == -12);
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
        for (const auto* id : { "progression", "tempo", "arp.style", "arp.freeMs", "arp.interval", "arp.decay" })
        {
            const auto* component = findControl (editor, id);
            REQUIRE (component != nullptr);
            CHECK ((component->getComponentID() == "arp.interval" ? component->getParentComponent()->getWidth() : component->getWidth()) >= 65);
            CHECK (component->getHeight() >= 20);
            CHECK (component->getParentComponent()->getLocalBounds().contains (component->getBounds()));
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
            const juce::StringArray ids = page == 0 ? juce::StringArray { "pattern.1", "pattern.16", "launch", "tempo", "clock" }
                : page == 1 ? juce::StringArray { "progression", "apply", "octave", "channel", "velocity", "voiceLeading" }
                : page == 2 ? juce::StringArray { "arp.style", "arp.rate", "arp.gate", "arp.steps", "arp.offset", "arp.interval" }
                : page == 4 ? juce::StringArray { "rhythm.gate.1", "rhythm.gate.16", "rhythm.tie.1", "rhythm.tie.16" }
                : juce::StringArray { "arp.swing", "arp.swingSubdivision", "arp.velocityHumanize", "arp.timingHumanize", "arp.target", "arp.decay" };
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
        const auto& viewport = control<juce::Viewport> (editor, "body.viewport");
        REQUIRE (viewport.getViewedComponent() != nullptr);
        CHECK (viewport.getViewedComponent()->getHeight() <= viewport.getHeight());
        CHECK (viewport.getViewPositionY() == 0);
    }
}

TEST_CASE ("Rhythm cycle handles and cells edit the saved lane with undo", "[native-ui][rhythm]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (390, 844);
    toggle (editor, "arp.enabled", true);
    control<juce::TextButton> (editor, "phone.page.4").onClick();
    auto& handle = control<juce::Component> (editor, "rhythm.gate.length");
    REQUIRE (handle.keyPressed (juce::KeyPress (juce::KeyPress::leftKey)));
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateLength"]) == 15);
    editor.controller.undo();
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateLength"]) == 16);
    control<juce::TextButton> (editor, "rhythm.gate.2").onClick();
    CHECK (static_cast<int> (processor.getTimelineState()["arp"]["gateMask"]) == 65533);
}

TEST_CASE ("Voice-leading choice updates the native sequence header and actual timeline", "[native-ui][voicing]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (390, 844);
    control<juce::TextButton> (editor, "phone.page.1").onClick();
    REQUIRE (editor.controller.set ("voiceLeading", 2));
    CHECK (control<juce::TextButton> (editor, "voiceLeading").getButtonText() == "Ramp Up");
    editor.controller.undo();
    CHECK (control<juce::TextButton> (editor, "voiceLeading").getButtonText() == "Root position");
}

TEST_CASE ("Arp-only view reclaims sequence space and previews production MIDI", "[native-ui][arp-preview]")
{
    PluginProcessor processor;
    NativeEditor editor (processor);
    editor.setSize (1400, 1000);
    toggle (editor, "arp.enabled", true);
    const auto original = control<juce::ComboBox> (editor, "arp.style").getScreenBounds();
    toggle (editor, "sequencerEnabled", false);
    auto& preview = control<ArpVisualizer> (editor, "arp.visualizer");
    CHECK (preview.isVisible());
    CHECK_FALSE (control<juce::TextEditor> (editor, "progression").isVisible());
    CHECK (control<juce::TextButton> (editor, "sequencerEnabled").getParentComponent()->getHeight() == 44);
    CHECK (control<juce::ComboBox> (editor, "arp.style").getScreenBounds() == original);
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
    control<juce::Slider> (editor, "arp.target").setValue (42, juce::sendNotificationSync);
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
        CHECK (control<juce::ComboBox> (editor, "arp.style").getParentComponent()->isVisible());
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
