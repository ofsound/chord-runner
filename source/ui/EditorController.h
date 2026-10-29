#pragma once
#include "../PluginProcessor.h"

namespace chordrunner::ui
{
// Message-thread adapter. All validation and scheduling remain in the processor.
class EditorController
{
public:
    explicit EditorController (PluginProcessor&);
    PluginProcessor& processor;
    juce::var state, transport;
    juce::String draft, error;
    int refreshSerial = 0;
    std::function<void()> changed;

    bool dirty() const { return draft != state["progression"].toString(); }
    bool set (const juce::Identifier&, const juce::var&, bool arpeggiator = false);
    bool applyDraft();
    bool selectPattern (int);
    void undo();
    void redo();
    void togglePlay();
    void panic();
    void refresh (bool replaceDraft = false);
    void poll();

private:
    bool commit (const juce::var&, bool replaceDraft, const juce::String& transaction = {});
};
}
