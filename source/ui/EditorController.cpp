#include "EditorController.h"

namespace chordrunner::ui
{
EditorController::EditorController (PluginProcessor& p) : processor (p)
{
    refresh (true);
}

void EditorController::refresh (bool replaceDraft)
{
    const bool keepDraft = ! replaceDraft && dirty();
    ++refreshSerial;
    state = processor.getTimelineState();
    transport = processor.getTransportState();
    if (! keepDraft)
        draft = state["progression"].toString();
    if (changed)
        changed();
}

bool EditorController::commit (const juce::var& next, bool replaceDraft)
{
    const auto result = processor.setTimelineState (next);
    error = result.getErrorMessage();
    refresh (replaceDraft && result.wasOk());
    return result.wasOk();
}

bool EditorController::set (const juce::Identifier& key, const juce::var& value, bool arpeggiator)
{
    auto next = processor.getTimelineState();
    auto target = arpeggiator ? next["arp"] : next;
    target.getDynamicObject()->setProperty (key, value);
    return commit (next, false);
}

bool EditorController::applyDraft()
{
    auto next = processor.getTimelineState();
    next.getDynamicObject()->setProperty ("progression", draft);
    return commit (next, true);
}

void EditorController::togglePlay()
{
    if (! static_cast<bool> (state["sequencerEnabled"]))
        return;
    transport = processor.getTransportState();
    if (static_cast<bool> (transport["armed"]))
        processor.setPlaying (false);
    else if (! dirty() || applyDraft())
        processor.setPlaying (true);
    poll();
}

void EditorController::panic()
{
    processor.panic();
    poll();
}

void EditorController::poll()
{
    transport = processor.getTransportState();
    if (static_cast<int> (transport["revision"]) != static_cast<int> (state["revision"]))
        refresh();
    else if (changed)
        changed();
}
}
