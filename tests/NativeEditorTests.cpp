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
        { "arp.swing", "swing", 17.5, true }, { "arp.steps", "steps", 4, true },
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
            CHECK (component->getWidth() >= 65);
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
