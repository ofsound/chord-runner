#include "NativeEditor.h"

namespace chordrunner::ui
{
void NativeEditor::restoreDesktopParents()
{
    if (sequenceModule.getParentComponent() != &content) content.addAndMakeVisible (sequenceModule);
    if (arpModule.getParentComponent() != &content) content.addAndMakeVisible (arpModule);
    if (timelineViewport.getParentComponent() != &sequenceModule) sequenceModule.addAndMakeVisible (timelineViewport);
    for (auto* component : std::initializer_list<juce::Component*> { &tempo, &clock, &launch })
    {
        if (component->getParentComponent() != this) addAndMakeVisible (component);
        component->setVisible (true);
    }
    for (auto* component : std::initializer_list<juce::Component*> { &sequencer, &progression, &apply, &message, &voicingSelector, &timelineViewport,
             &arpeggiator, &style, &offset, &repeats, &rateMode, &gate, &swing, &swingSubdivision, &steps, &octaveOrder, &retrigger, &interval, &hold,
             &envelopePower, &velocityHumanize, &timingHumanize, &projectTitle, &projectMenu, &undoButton, &redoButton, &mutePad })
        component->setVisible (true);
    message.setVisible (controller.error.isNotEmpty());
    for (auto& group : groups) group.setVisible (true);
    for (size_t i = 0; i < pads.size(); ++i) pads[i].setVisible (i < 8);
    for (auto& tab : phoneTabs) tab.setVisible (false);
    phonePreview.setVisible (false);
    performanceInfo.setVisible (false);
    rhythmPanel.setVisible (false);
    viewport.setVisible (true);
    viewport.setScrollBarsShown (true, false);
    sequenceModule.setTitleText ("SEQUENCER");
    arpModule.setTitleText ("ARPEGGIATOR");
    mutePad.setButtonText ("M");
    rate.setVisible (static_cast<bool> (controller.state["arp"]["sync"]));
    freeRate.setVisible (! rate.isVisible());
    for (auto* number : numbers)
        number->slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 78, 26);
}

void NativeEditor::layoutPhone()
{
    if (! phoneLayout) return;
    scaleControl.setVisible (false);
    loopBar.setVisible (false);
    envelopeDisplay.setVisible (false);
    rate.setRotary (false);
    for (auto* mode : { &rateMode, &octaveOrder, &swingSubdivision }) mode->setHeaderMode (false);
    for (auto* label : { &rate.label, &freeRate.label, &steps.label, &swing.label }) label->setVisible (true);
    timingHumanize.label.setText ("Timing humanize", juce::dontSendNotification);
    velocityHumanize.label.setText ("Velocity humanize", juce::dontSendNotification);
    humanizeGroup.setVisible (false);
    for (auto* control : { &repeats, &steps, &freeRate, &gate, &swing })
        control->slider.setSliderStyle (juce::Slider::LinearHorizontal);
    arpVisualizer.setVisible (false);
    phonePreview.setVisible (false);
    const int w = getWidth(), h = getHeight();
    const bool landscape = w > h;
    const bool perform = phonePage == 0, sequence = phonePage == 1, arpPage = phonePage == 2;
    const bool localTransport = ! controller.processor.isHosted();
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
    projectTitle.setBounds (10, 23, juce::jmax (40, w - 172), 28);
    undoButton.setBounds (w - 156, 13, 32, 32);
    redoButton.setBounds (w - 120, 13, 32, 32);
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
    play.setVisible (localTransport);
    if (localTransport)
    {
        play.setBounds (footer.removeFromLeft (36)); footer.removeFromLeft (8);
    }
    tipsButton.setBounds (footer.removeFromRight (36)); footer.removeFromRight (4);
    glossary.setBounds (footer.removeFromRight (36)); footer.removeFromRight (4);
    panicButton.setBounds (footer.removeFromRight (w < 360 ? 80 : 100)); footer.removeFromRight (4);
    midiActivity.setBounds (footer.removeFromRight (juce::jmin (132, footer.getWidth()))); footer.removeFromRight (4);
    footerTip.setBounds (footer);
    rhythmPanel.setHorizontal (false);
    rhythmPanel.setVisible (phonePage == 4);
    rhythmPanel.setEnabled (static_cast<bool> (controller.state["arp"]["enabled"]));
    rhythmPanel.setBounds (8, 50, body.getWidth() - 16, body.getHeight() - 58);
    sequenceModule.setVisible (sequence);
    arpModule.setVisible (! perform && ! sequence);
    sequenceModule.setBounds (body);
    arpModule.setBounds (body);
    sequenceModule.setTitleText ("");
    arpModule.setTitleText (arpPage ? "ARP" : phonePage == 4 ? "RHYTHM" : "GROOVE");
    for (auto& group : groups) group.setVisible (false);
    retriggerGroup.setVisible (false);
    for (auto* component : std::initializer_list<juce::Component*> { &style, &offset, &repeats, &rateMode, &rate, &freeRate, &gate, &swing, &swingSubdivision, &steps, &octaveOrder,
             &retrigger, &interval, &hold, &envelopePower, &velocityHumanize, &timingHumanize })
        component->setVisible (false);
    for (size_t i = 0; i < pads.size(); ++i) pads[i].setVisible (perform && i < 8);
    mutePad.setVisible (perform);
    mutePad.setButtonText ("Mute / E1");
    launch.setVisible (perform);
    tempo.setVisible (perform && localTransport);
    clock.setVisible (perform && localTransport);
    performanceInfo.setVisible (perform);
    glossary.setVisible (true);
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
            if (localTransport)
            {
                tempo.setBounds (settings.removeFromTop (controlH)); settings.removeFromTop (3);
                clock.setBounds (settings.removeFromTop (controlH)); settings.removeFromTop (3);
            }
            launch.setBounds (settings.removeFromTop (controlH)); settings.removeFromTop (6);
            mutePad.setBounds (settings.removeFromTop (juce::jmin (44, settings.getHeight())));
            performanceInfo.setBounds (area.removeFromTop (38)); area.removeFromTop (6);
            const int cellW = area.getWidth() / 8;
            const int cellH = juce::jmin (76, area.getHeight());
            const int side = juce::jmin (cellW, cellH) - 4;
            for (int i = 0; i < 8; ++i)
                pads[static_cast<size_t> (i)].setBounds (area.getX() + (i % 8) * cellW + (cellW - side) / 2,
                    area.getY() + (i / 8) * cellH, side, side);
            timelineViewport.setVisible (false);
        }
        else
        {
            performanceInfo.setBounds (area.removeFromTop (44)); area.removeFromTop (4);
            const int gridH = juce::jlimit (88, 168, area.getHeight() - 142);
            const auto grid = area.removeFromTop (gridH);
            const int cellW = grid.getWidth() / 4, cellH = grid.getHeight() / 2;
            const int side = juce::jmin (cellW, cellH) - 4;
            for (int i = 0; i < 8; ++i)
                pads[static_cast<size_t> (i)].setBounds (grid.getX() + (i % 4) * cellW + (cellW - side) / 2,
                    grid.getY() + (i / 4) * cellH, side, side);
            area.removeFromTop (6);
            if (localTransport)
            {
                auto timing = area.removeFromTop (60);
                tempo.setBounds (timing.removeFromLeft (timing.getWidth() / 2).withTrimmedRight (10));
                clock.setBounds (timing);
                area.removeFromTop (6);
            }
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
        sequencer.setBounds (10, 2, 32, 32);
        voicingSelector.setVisible (true);
        octave.setVisible (true); velocity.setVisible (true);
        loopBar.setVisible (true);
        const int entryHeight = 40;
        progression.setBounds (12, 42, cw - 74, entryHeight);
        apply.setBounds (cw - 52, 42, 40, entryHeight);
        const int dockBottom = ch - 12 - (message.isVisible() ? 26 : 0);
        const int dockHeight = landscape ? 88 : 102;
        const int dockTop = dockBottom - dockHeight;
        voicingSelector.setBounds (12, dockTop, cw - 24, 32);
        auto values = juce::Rectangle<int> (12, dockTop + 40, cw - 24, 48);
        velocity.setBounds (values.removeFromLeft (110)); values.removeFromLeft (8);
        octave.setBounds (values.removeFromLeft (70));
        loopBar.setBounds (12, 50 + entryHeight, cw - 24, 22);
        timelineViewport.setBounds (12, 72 + entryHeight, cw - 24, juce::jmax (1, dockTop - entryHeight - 80));
        message.setBounds (12, ch - 30, cw - 24, 24);
        timelineViewport.setVisible (true);
    }
    else
    {
        // Separate phone pages keep every control at a useful touch size.
        // The same native controls and controller serve all layouts.
        arpeggiator.setVisible (true);
        arpeggiator.setBounds (10, 2, 32, 32);
        hold.setVisible (arpPage);
        hold.setBounds (104, 8, 62, 21);
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
            controls = { &rateMode, static_cast<bool> (controller.state["arp"]["sync"]) ? static_cast<juce::Component*> (&rate) : &freeRate,
                &gate, &steps, &octaveOrder, &offset, &repeats, &retrigger, &interval };
        else
            controls = { &swing, &swingSubdivision, &velocityHumanize, &timingHumanize, &envelopePower };
        const int columns = landscape ? (arpPage ? 5 : 4) : 2;
        const int rows = (static_cast<int> (controls.size()) + columns - 1) / columns;
        auto grid = arpModule.getLocalBounds().reduced (12, 0).withTrimmedTop (54).withTrimmedBottom (12);
        if (! arpPage)
        {
            envelopeDisplay.setVisible (true);
            if (landscape)
            {
                envelopeDisplay.setBounds (grid.removeFromRight (grid.getWidth() / 2));
                grid.removeFromRight (8);
            }
            else
            {
                envelopeDisplay.setBounds (grid.removeFromBottom (juce::jmin (180, grid.getHeight() / 2)));
                grid.removeFromBottom (8);
            }
        }
        const bool previewAvailable = arpPage && arpOnlyLayout;
        const bool inlinePreview = previewAvailable && ! landscape && body.getHeight() >= 600;
        const bool focusPreview = previewAvailable && ! inlinePreview && phonePreview.getToggleState();
        phonePreview.setVisible (previewAvailable && ! inlinePreview);
        if (previewAvailable && ! inlinePreview)
        {
            phonePreview.setBounds (178, 4, juce::jmin (128, body.getWidth() - 186), 40);
            phonePreview.setButtonText (focusPreview ? "Show controls" : "Show preview");
        }
        if (inlinePreview || focusPreview)
        {
            arpVisualizer.setVisible (true);
            arpVisualizer.setBounds (inlinePreview ? grid.removeFromTop (224) : grid);
            if (inlinePreview) grid.removeFromTop (10);
        }
        if (focusPreview)
        {
            timelineViewport.setVisible (false);
            return;
        }
        if (arpPage)
        {
            style.setVisible (true);
            style.setBounds (grid.removeFromTop (landscape ? 48 : 104));
            grid.removeFromTop (6);
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
    if ((sequence && sequenceFold == 1) || (arpPage && arpFold == 1))
    {
        auto& module = sequence ? sequenceModule : arpModule;
        for (auto* child : module.getChildren()) child->setVisible (child == (sequence ? &sequencer : &arpeggiator) || (arpPage && child == &hold));
        module.setSize (module.getWidth(), ModuleCard::headerHeight);
    }
    updateTimelineSize();
}
}
