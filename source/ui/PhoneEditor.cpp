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
    for (auto* component : std::initializer_list<juce::Component*> { &sequencer, &progression, &apply, &message, &summary, &patternName, &fit, &patternMenu, &glossary, &voicingButton, &timelineViewport,
             &arpeggiator, &style, &offset, &repeats, &rateMode, &gate, &swing, &swingSubdivision, &steps, &distance, &retrigger, &interval, &hold,
             &velocityEnabled, &target, &decay, &velocityRetrigger, &velocityHumanize, &timingHumanize, &projectTitle, &projectMenu, &undoButton, &redoButton, &mutePad })
        component->setVisible (true);
    message.setVisible (controller.error.isNotEmpty() || controller.dirty());
    summary.setVisible (! message.isVisible());
    for (auto& group : groups) group.setVisible (true);
    for (auto& pad : pads) pad.setVisible (true);
    for (auto& tab : phoneTabs) tab.setVisible (false);
    phonePreview.setVisible (false);
    performanceInfo.setVisible (false);
    pitchHint.setVisible (true); humanizeHint.setVisible (true);
    rhythmPanel.setVisible (false);
    rhythmButton.setVisible (true);
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
    arpVisualizer.setVisible (false);
    phonePreview.setVisible (false);
    pitchHint.setVisible (false); humanizeHint.setVisible (false);
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
    for (int i = 0; i < 5; ++i)
    {
        auto& tab = phoneTabs[static_cast<size_t> (i)];
        if (landscape)
            tab.setBounds (156 + i * (w - 246) / 5, 5, (w - 246) / 5 - 4, 40);
        else
            tab.setBounds (margin + i * (w - margin * 2) / 5, 60, (w - margin * 2) / 5 - 4, 40);
        tab.setToggleState (phonePage == i, juce::dontSendNotification);
        tab.setVisible (true);
    }
    auto footer = getLocalBounds().removeFromBottom (44).reduced (margin, 2);
    play.setBounds (footer.removeFromLeft (76)); footer.removeFromLeft (8);
    panicButton.setBounds (footer.removeFromRight (108)); footer.removeFromRight (6);
    status.setBounds (footer);
    rhythmPanel.setVisible (phonePage == 4);
    rhythmButton.setVisible (false);
    rhythmPanel.setBounds (8, 50, body.getWidth() - 16, body.getHeight() - 58);
    sequenceModule.setVisible (sequence);
    arpModule.setVisible (! perform && ! sequence);
    sequenceModule.setBounds (body);
    arpModule.setBounds (body);
    sequenceModule.setTitleText ("");
    arpModule.setTitleText (arpPage ? "ARPEGGIATOR" : phonePage == 4 ? "RHYTHM" : "GROOVE");
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
        patternName.setBounds (60, 8, juce::jmax (0, cw - 278), 32);
        patternName.setVisible (cw >= 340);
        voicingButton.setBounds (cw - 207, 4, 110, 40);
        patternMenu.setBounds (cw - 89, 4, 77, 40);
        octave.setVisible (true); velocity.setVisible (true); channel.setVisible (true);
        progression.setFont (juce::FontOptions (24.0f));
        progression.setBorder (juce::BorderSize<int> (0, 14, 0, 14));
        if (landscape)
        {
            const int left = juce::jmax (260, cw * 2 / 5);
            progression.setBounds (12, 52, left - 100, 60);
            apply.setBounds (left - 80, 52, 68, 60);
            auto settings = juce::Rectangle<int> (12, 132, left - 24, juce::jmin (66, ch - 144));
            octave.setBounds (settings.removeFromLeft (64)); settings.removeFromLeft (8);
            channel.setBounds (settings.removeFromRight (64)); settings.removeFromRight (8);
            velocity.setBounds (settings);
            timelineViewport.setBounds (left + 8, 52, cw - left - 20, ch - 88);
            summary.setBounds (left + 8, ch - 28, cw - left - 20, 24);
        }
        else
        {
            progression.setBounds (12, 54, cw - 102, 64);
            apply.setBounds (cw - 80, 54, 68, 64);
            auto settings = juce::Rectangle<int> (12, 134, cw - 24, 66);
            octave.setBounds (settings.removeFromLeft (64)); settings.removeFromLeft (8);
            channel.setBounds (settings.removeFromRight (64)); settings.removeFromRight (8);
            velocity.setBounds (settings);
            timelineViewport.setBounds (12, 216, cw - 24, ch - 254);
            summary.setBounds (12, ch - 30, cw - 24, 24);
        }
        message.setBounds (summary.getBounds());
        timelineViewport.setVisible (true);
    }
    else
    {
        // Separate phone pages keep every control at a useful touch size.
        // The same native controls and controller serve all layouts.
        arpeggiator.setVisible (true);
        arpeggiator.setBounds (10, 2, 44, 44);
        hold.setVisible (arpPage);
        hold.setBounds (body.getWidth() - 124, 4, 112, 40);
        if (phonePage == 4)
        {
            arpModule.setTitleText ("");
            arpeggiator.setVisible (false);
            rhythmPanel.setBounds (0, 0, body.getWidth(), body.getHeight());
            timelineViewport.setVisible (false);
            updateTimelineSize();
            return;
        }
        std::vector<juce::Component*> controls;
        if (arpPage)
            controls = { &style, &rateMode, static_cast<bool> (controller.state["arp"]["sync"]) ? static_cast<juce::Component*> (&rate) : &freeRate,
                &gate, &steps, &distance, &offset, &repeats, &retrigger, &interval };
        else
            controls = { &swing, &swingSubdivision, &velocityHumanize, &timingHumanize, &velocityEnabled, &target, &decay, &velocityRetrigger };
        const int columns = landscape ? (arpPage ? 5 : 4) : 2;
        const int rows = (static_cast<int> (controls.size()) + columns - 1) / columns;
        auto grid = arpModule.getLocalBounds().reduced (12, 0).withTrimmedTop (54).withTrimmedBottom (12);
        const bool previewAvailable = arpPage && arpOnlyLayout;
        const bool inlinePreview = previewAvailable && ! landscape && body.getHeight() >= 600;
        const bool focusPreview = previewAvailable && ! inlinePreview && phonePreview.getToggleState();
        phonePreview.setVisible (previewAvailable && ! inlinePreview);
        if (previewAvailable && ! inlinePreview)
        {
            arpModule.setTitleText ("");
            phonePreview.setBounds (64, 4, juce::jmin (128, body.getWidth() - 200), 40);
            phonePreview.setButtonText (focusPreview ? "Show controls" : "Show preview");
        }
        if (inlinePreview || focusPreview)
        {
            arpVisualizer.setVisible (true);
            arpVisualizer.setBounds (grid.removeFromTop (inlinePreview ? 224 : grid.getHeight()));
            grid.removeFromTop (10);
        }
        if (focusPreview)
        {
            timelineViewport.setVisible (false);
            return;
        }
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
