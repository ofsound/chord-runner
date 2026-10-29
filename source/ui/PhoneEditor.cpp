#include "NativeEditor.h"

namespace chordrunner::ui
{
void NativeEditor::restoreDesktopParents()
{
    if (sequenceModule.getParentComponent() != &content) content.addAndMakeVisible (sequenceModule);
    if (arpModule.getParentComponent() != &content) content.addAndMakeVisible (arpModule);
    if (timelineViewport.getParentComponent() != &sequenceModule) sequenceModule.addAndMakeVisible (timelineViewport);
    for (auto* component : std::initializer_list<juce::Component*> { &tempo, &clock, &octave, &velocity, &channel, &launch })
    {
        if (component->getParentComponent() != this) addAndMakeVisible (component);
        component->setVisible (true);
    }
    for (auto* component : std::initializer_list<juce::Component*> { &sequencer, &progression, &apply, &message, &summary, &patternName, &fit, &patternMenu, &glossary, &timelineViewport,
             &arpeggiator, &style, &offset, &repeats, &rateMode, &gate, &swing, &swingSubdivision, &steps, &distance, &retrigger, &interval, &hold,
             &velocityEnabled, &target, &decay, &velocityRetrigger, &velocityHumanize, &timingHumanize, &projectTitle, &projectMenu, &undoButton, &redoButton, &mutePad })
        component->setVisible (true);
    for (auto& group : groups) group.setVisible (true);
    for (auto& pad : pads) pad.setVisible (true);
    for (auto& tab : phoneTabs) tab.setVisible (false);
    performanceInfo.setVisible (false);
    viewport.setVisible (true);
    viewport.setScrollBarsShown (true, false);
    sequenceModule.setTitleText ("SEQUENCE");
    arpModule.setTitleText ("ARPEGGIATOR");
    mutePad.setButtonText ("M");
    fit.setVisible (true);
    fitNotes = fit.getToggleState();
    rate.setVisible (static_cast<bool> (controller.state["arp"]["sync"]));
    freeRate.setVisible (! rate.isVisible());
    for (auto* number : numbers)
        number->slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 78, 26);
}

void NativeEditor::layoutPhone()
{
    if (! phoneLayout) return;
    const int w = getWidth(), h = getHeight();
    const bool landscape = w > h;
    const bool perform = phonePage == 0, sequence = phonePage == 1, arpPage = phonePage == 2;
    const int margin = 10;
    const int bodyTop = landscape ? 56 : 108;
    const auto body = juce::Rectangle<int> (margin, bodyTop, juce::jmax (1, w - margin * 2), juce::jmax (1, h - bodyTop - 52));
    viewport.setVisible (false);
    viewport.setScrollBarsShown (false, false);
    viewport.setViewPosition (0, 0);
    if (sequenceModule.getParentComponent() != this) addChildComponent (sequenceModule);
    if (arpModule.getParentComponent() != this) addChildComponent (arpModule);
    if (octave.getParentComponent() != &sequenceModule) sequenceModule.addChildComponent (octave);
    if (velocity.getParentComponent() != &sequenceModule) sequenceModule.addChildComponent (velocity);
    if (channel.getParentComponent() != &sequenceModule) sequenceModule.addChildComponent (channel);
    projectTitle.setBounds (10, 23, juce::jmax (40, w - 204), 28);
    undoButton.setBounds (w - 188, 9, 50, 40);
    redoButton.setBounds (w - 134, 9, 50, 40);
    projectMenu.setBounds (w - 80, 9, 70, 40);
    undoButton.setVisible (! landscape);
    redoButton.setVisible (! landscape);
    if (landscape)
    {
        projectTitle.setBounds (10, 5, 136, 40);
        projectMenu.setBounds (w - 80, 5, 70, 40);
    }
    for (int i = 0; i < 4; ++i)
    {
        auto& tab = phoneTabs[static_cast<size_t> (i)];
        if (landscape)
            tab.setBounds (156 + i * (w - 246) / 4, 5, (w - 246) / 4 - 4, 40);
        else
            tab.setBounds (margin + i * (w - margin * 2) / 4, 60, (w - margin * 2) / 4 - 4, 40);
        tab.setToggleState (phonePage == i, juce::dontSendNotification);
        tab.setVisible (true);
    }
    auto footer = getLocalBounds().removeFromBottom (44).reduced (margin, 2);
    play.setBounds (footer.removeFromLeft (76)); footer.removeFromLeft (8);
    panicButton.setBounds (footer.removeFromRight (108)); footer.removeFromRight (6);
    status.setBounds (footer);
    sequenceModule.setVisible (sequence);
    arpModule.setVisible (! perform && ! sequence);
    sequenceModule.setBounds (body);
    arpModule.setBounds (body);
    sequenceModule.setTitleText ("");
    arpModule.setTitleText (arpPage ? "ARPEGGIATOR" : "GROOVE");
    for (auto& group : groups) group.setVisible (false);
    for (auto* component : std::initializer_list<juce::Component*> { &style, &offset, &repeats, &rateMode, &rate, &freeRate, &gate, &swing, &swingSubdivision, &steps, &distance,
             &retrigger, &interval, &hold, &velocityEnabled, &target, &decay, &velocityRetrigger, &velocityHumanize, &timingHumanize })
        component->setVisible (false);
    for (auto& pad : pads) pad.setVisible (perform);
    mutePad.setVisible (perform);
    mutePad.setButtonText ("Mute / E1");
    launch.setVisible (perform);
    tempo.setVisible (perform);
    clock.setVisible (perform);
    performanceInfo.setVisible (perform);
    fit.setVisible (false);
    glossary.setVisible (false);
    fitNotes = true;
    for (auto* number : numbers)
        number->slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 74, 36);
    if (perform)
    {
        if (timelineViewport.getParentComponent() != this) addChildComponent (timelineViewport);
        auto area = body;
        if (landscape)
        {
            auto settings = area.removeFromRight (juce::jmax (156, area.getWidth() / 4));
            area.removeFromRight (12);
            const int controlH = juce::jlimit (44, 56, (body.getHeight() - 58) / 3);
            tempo.setBounds (settings.removeFromTop (controlH)); settings.removeFromTop (3);
            clock.setBounds (settings.removeFromTop (controlH)); settings.removeFromTop (3);
            launch.setBounds (settings.removeFromTop (controlH)); settings.removeFromTop (6);
            mutePad.setBounds (settings.removeFromTop (juce::jmin (44, settings.getHeight())));
            performanceInfo.setBounds (area.removeFromTop (38)); area.removeFromTop (6);
            const int cellW = area.getWidth() / 8;
            const int cellH = juce::jmin (76, area.getHeight() / 2);
            for (int i = 0; i < 16; ++i)
                pads[static_cast<size_t> (i)].setBounds (area.getX() + (i % 8) * cellW, area.getY() + (i / 8) * cellH, cellW - 4, cellH - 4);
            timelineViewport.setVisible (false);
        }
        else
        {
            performanceInfo.setBounds (area.removeFromTop (44)); area.removeFromTop (4);
            const int gridH = juce::jlimit (176, 336, area.getHeight() - 142);
            const auto grid = area.removeFromTop (gridH);
            const int cellW = grid.getWidth() / 4, cellH = grid.getHeight() / 4;
            for (int i = 0; i < 16; ++i)
                pads[static_cast<size_t> (i)].setBounds (grid.getX() + (i % 4) * cellW, grid.getY() + (i / 4) * cellH, cellW - 4, cellH - 4);
            area.removeFromTop (6);
            auto timing = area.removeFromTop (60);
            tempo.setBounds (timing.removeFromLeft (timing.getWidth() / 2).withTrimmedRight (10));
            clock.setBounds (timing);
            area.removeFromTop (6);
            auto launching = area.removeFromTop (60);
            launch.setBounds (launching.removeFromLeft (launching.getWidth() / 2).withTrimmedRight (10));
            mutePad.setBounds (launching.withTrimmedTop (18));
            area.removeFromTop (12);
            timelineViewport.setVisible (area.getHeight() >= 100);
            timelineViewport.setBounds (area);
        }
    }
    else if (sequence)
    {
        if (timelineViewport.getParentComponent() != &sequenceModule) sequenceModule.addAndMakeVisible (timelineViewport);
        const int cw = body.getWidth(), ch = body.getHeight();
        sequencer.setBounds (10, 2, 44, 44);
        patternName.setVisible (true);
        patternName.setBounds (60, 8, cw - 158, 32);
        patternMenu.setBounds (cw - 89, 4, 77, 40);
        octave.setVisible (true); velocity.setVisible (true); channel.setVisible (true);
        if (landscape)
        {
            const int left = juce::jmax (260, cw * 2 / 5);
            progression.setBounds (12, 52, left - 100, 44);
            apply.setBounds (left - 80, 52, 68, 44);
            message.setBounds (12, 100, left - 24, 32);
            auto settings = juce::Rectangle<int> (12, 140, left - 24, juce::jmin (66, ch - 152));
            octave.setBounds (settings.removeFromLeft (64)); settings.removeFromLeft (8);
            channel.setBounds (settings.removeFromRight (64)); settings.removeFromRight (8);
            velocity.setBounds (settings);
            timelineViewport.setBounds (left + 8, 52, cw - left - 20, ch - 88);
            summary.setBounds (left + 8, ch - 28, cw - left - 20, 24);
        }
        else
        {
            progression.setBounds (12, 54, cw - 102, 48);
            apply.setBounds (cw - 80, 54, 68, 48);
            message.setBounds (12, 108, cw - 24, 38);
            auto settings = juce::Rectangle<int> (12, 151, cw - 24, 66);
            octave.setBounds (settings.removeFromLeft (64)); settings.removeFromLeft (8);
            channel.setBounds (settings.removeFromRight (64)); settings.removeFromRight (8);
            velocity.setBounds (settings);
            timelineViewport.setBounds (12, 232, cw - 24, ch - 270);
            summary.setBounds (12, ch - 30, cw - 24, 24);
        }
        timelineViewport.setVisible (true);
    }
    else
    {
        // Separate phone pages keep every control at a useful touch size.
        // The same native controls and controller serve all layouts.
        arpeggiator.setBounds (10, 2, 44, 44);
        hold.setVisible (arpPage);
        hold.setBounds (body.getWidth() - 124, 4, 112, 40);
        std::vector<juce::Component*> controls;
        if (arpPage)
            controls = { &style, &rateMode, static_cast<bool> (controller.state["arp"]["sync"]) ? static_cast<juce::Component*> (&rate) : &freeRate,
                &gate, &steps, &distance, &offset, &repeats, &retrigger, &interval };
        else
            controls = { &swing, &swingSubdivision, &velocityHumanize, &timingHumanize, &velocityEnabled, &target, &decay, &velocityRetrigger };
        const int columns = landscape ? (arpPage ? 5 : 4) : 2;
        const int rows = (static_cast<int> (controls.size()) + columns - 1) / columns;
        auto grid = arpModule.getLocalBounds().reduced (12, 0).withTrimmedTop (54).withTrimmedBottom (12);
        const int cellW = grid.getWidth() / columns;
        const int cellH = juce::jmin (104, grid.getHeight() / rows);
        for (size_t i = 0; i < controls.size(); ++i)
        {
            auto* control = controls[i];
            control->setVisible (true);
            control->setBounds (grid.getX() + static_cast<int> (i) % columns * cellW,
                grid.getY() + static_cast<int> (i) / columns * cellH, cellW - 8, cellH - 6);
        }
        freeRate.slider.setSliderStyle (juce::Slider::LinearHorizontal);
        interval.slider.setSliderStyle (juce::Slider::LinearHorizontal);
        timelineViewport.setVisible (false);
    }
    updateTimelineSize();
}
}
