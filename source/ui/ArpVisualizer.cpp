#include "ArpVisualizer.h"

namespace chordrunner::ui
{
    namespace
    {
        const juce::Colour ink (0xffe9ecee), muted (0xff929da8), mint (0xff83dabb), gold (0xffeab975);
        const juce::StringArray styles { "Up", "Down", "Up & Down", "Down & Up", "Converge", "Diverge", "Play order", "Chord trigger", "Random", "Random other", "Random once" };
    }
    ArpVisualizer::ArpVisualizer()
    {
        setComponentID ("arp.visualizer");
        inspection.setColour (juce::Label::textColourId, muted);
        inspection.setFont (juce::FontOptions (11.0f));
        addAndMakeVisible (inspection);
        inspection.setText ("Height = relative pitch / width = gate / gold = velocity / dotted ticks = straight timing", juce::dontSendNotification);
    }
    void ArpVisualizer::update (const ArpeggiatorSettings& value, double bpm, int heldNotes)
    {
        settings = value;
        tempo = bpm;
        heldCount = juce::jlimit (0, Arpeggiator::capacity, heldNotes);
        rankCount = heldCount > 0 ? heldCount : 4;
        rankSpacing = juce::jmax (1, 12 / rankCount);
        referenceBase = juce::jmin (48, 128 - rankCount * rankSpacing);
        rebuild();
    }
    void ArpVisualizer::rebuild()
    {
        notes.clear();
        hovered = -1;
        const int baseLength = settings.style == ArpeggiatorSettings::chordTrigger                                              ? 1
                               : settings.style == ArpeggiatorSettings::upDown || settings.style == ArpeggiatorSettings::downUp ? juce::jmax (1, 2 * rankCount - 2)
                                                                                                                                : rankCount;
        stepCount = baseLength * (settings.steps + 1);
        stepBeats = settings.sync ? settings.rate : settings.freeMs * tempo / 60000.0;
        naturalBeats = stepCount * stepBeats;
        // Beat mode shows the entire interval, including ordinary pattern wraps
        // and rests after finite repeats. A pattern wrap is not a retrigger.
        beats = settings.retrigger == 2 ? settings.interval : naturalBeats;
        const double seconds = beats * 60.0 / tempo;
        const double sampleRate = juce::jlimit (128.0, 4000.0, 131072.0 / seconds);
        const double increment = tempo / (60.0 * sampleRate);
        const int total = static_cast<int> (std::ceil (beats / increment));
        Arpeggiator engine;
        juce::MidiBuffer input, output;
        input.ensureSize (65536);
        output.ensureSize (262144);
        std::array<int, 128> active;
        active.fill (-1);
        // Only the number of physically held input notes is observed. Pitch and
        // arrival order remain anonymous, independent of the sequencer and input pitches.
        for (int start = 0; start < total; start += 256)
        {
            const int count = juce::jmin (256, total - start);
            input.clear();
            output.clear();
            if (start == 0)
                for (int i = 0; i < rankCount; ++i)
                {
                    const int rank = settings.style == ArpeggiatorSettings::playOrder
                                         ? (i % 2 == 0 ? rankCount / 2 + i / 2 : i / 2)
                                         : i;
                    input.addEvent (juce::MidiMessage::noteOn (1, referenceBase + rank * rankSpacing, static_cast<juce::uint8> (100)), 0);
                }
            engine.render (settings, input, output, start * increment, tempo, sampleRate, count, true, false);
            for (const auto event : output)
            {
                const auto message = event.getMessage();
                if (!message.isNoteOnOrOff())
                    continue;
                auto& index = active[static_cast<size_t> (message.getNoteNumber())];
                const double at = juce::jmin (beats, (start + event.samplePosition) * increment);
                if (index >= 0)
                    notes[static_cast<size_t> (index)].end = at;
                index = -1;
                if (message.isNoteOn() && at < beats)
                {
                    index = static_cast<int> (notes.size());
                    notes.push_back ({ message.getNoteNumber(), message.getVelocity(), at, beats });
                }
            }
        }
        reachesWindowEdge.assign (notes.size(), false);
        for (const int index : active)
            if (index >= 0)
                reachesWindowEdge[static_cast<size_t> (index)] = true;
        low = referenceBase;
        high = referenceBase + (rankCount - 1) * rankSpacing;
        for (const auto& note : notes)
        {
            low = juce::jmin (low, note.note);
            high = juce::jmax (high, note.note);
        }
        inspection.setText (description(), juce::dontSendNotification);
        repaint();
    }
    juce::String ArpVisualizer::timeText (double value) const
    {
        return settings.sync || settings.retrigger == 2 ? juce::String (value, 2) + " beats" : juce::String (value * 60000.0 / tempo, 0) + " ms";
    }
    juce::String ArpVisualizer::description() const
    {
        if (getWidth() < 600)
            return (settings.retrigger == 2 ? "Restart: " + timeText (beats) : settings.retrigger == 1 ? juce::String ("Note restart")
                                                                                                       : juce::String ("Restart off"))
                   + " / " + (settings.repeats == 0 ? juce::String ("Infinite repeats") : "Repeats: " + juce::String (settings.repeats))
                   + (settings.hold ? " / Hold on" : " / Hold off");
        const auto restart = settings.retrigger == 2   ? "Gold = restart at " + timeText (beats)
                             : settings.retrigger == 1 ? juce::String ("New input notes restart the phrase")
                                                       : juce::String ("Off = no input-note or grid restart");
        const auto repeats = settings.repeats == 0 ? juce::String ("Repeats forever")
                                                   : juce::String (settings.repeats) + " cycles then waits";
        const auto envelope = settings.velocityEnabled && getWidth() >= 800
                                  ? (settings.velocityRetrigger ? " / Retrigger resets velocity" : " / Retrigger preserves velocity")
                                  : "";
        return restart + " / " + repeats + " / "
               + (settings.hold ? "Hold latches input" : "Hold off follows releases") + envelope;
    }

    void ArpVisualizer::resized()
    {
        const bool compact = getHeight() < 270;
        const float top = compact ? 70.0f : 112.0f;
        const float footer = compact ? 78.0f : 136.0f;
        plot = { 58.0f, top, static_cast<float> (getWidth() - 78), juce::jmax (24.0f, static_cast<float> (getHeight()) - top - footer) };
        timing = { plot.getX(), plot.getBottom() + 12, plot.getWidth(), compact ? 14.0f : 24.0f };
        velocities = { plot.getX(), timing.getBottom() + 8, plot.getWidth(), compact ? 15.0f : 30.0f };
        inspection.setBounds (12, getHeight() - 24, getWidth() - 24, 20);
    }
    juce::Rectangle<float> ArpVisualizer::noteBounds (const PreviewNote& note) const
    {
        const float h = juce::jlimit (4.0f, 14.0f, plot.getHeight() / static_cast<float> (high - low + 4) * 2.0f);
        const float y = plot.getBottom() - 10 - (high > low ? static_cast<float> (note.note - low) / static_cast<float> (high - low) : 0.5f) * (plot.getHeight() - 20);
        return { plot.getX() + static_cast<float> (note.start / beats) * plot.getWidth(), y - h * 0.5f, juce::jmax (2.0f, static_cast<float> ((note.end - note.start) / beats) * plot.getWidth() - 2), h };
    }
    void ArpVisualizer::paint (juce::Graphics& g)
    {
        const bool compact = getHeight() < 270, narrow = getWidth() < 600;
        g.setColour (juce::Colour (0xff131c21));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 8);
        g.setColour (juce::Colour (0xff34434a));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8, 1);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.setColour (mint);
        g.drawText (settings.retrigger == 2 ? "RESTART WINDOW" : "ARP CYCLE", 16, 10, 160, 24, juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (11.0f));
        g.setColour (muted);
        g.drawText (timeText (beats), getWidth() - 126, 10, 110, 24, juce::Justification::centredRight);
        juce::String relationship;
        if (settings.retrigger == 2)
        {
            if (settings.repeats > 0 && naturalBeats * settings.repeats < beats)
                relationship = juce::String (settings.repeats) + (settings.repeats == 1 ? " cycle + rest" : " cycles + rest");
            else if (beats < naturalBeats)
                relationship = "Restart interrupts cycle";
            else
            {
                const double cycles = beats / naturalBeats;
                relationship = juce::String (cycles, std::abs (cycles - std::round (cycles)) < 1.0e-6 ? 0 : 2)
                               + (std::abs (cycles - 1.0) < 1.0e-6 ? " cycle per restart" : " cycles per restart");
            }
        }
        const auto source = heldCount > 0 ? juce::String (heldCount) + (narrow ? " held" : heldCount == 1 ? " held note"
                                                                                                          : " held notes")
                                          : juce::String (narrow ? "4-note demo" : "4-note example (no input)");
        const auto context = source + " / cycle " + timeText (naturalBeats)
                             + (relationship.isEmpty() ? juce::String() : " / " + relationship);
        g.drawText ((narrow ? juce::String() : styles[settings.style] + " / ") + context,
            16,
            34,
            getWidth() - 32,
            18,
            juce::Justification::left);
        if (!compact)
        {
            g.drawText ("RELATIVE PITCH + GATE", 58, 61, 200, 16, juce::Justification::left);
            if (!narrow)
                g.drawText ("Dotted grid = straight rate / attack velocity starts at 100", 280, 61, getWidth() - 300, 16, juce::Justification::right);
        }
        const auto xAt = [this] (double at) { return plot.getX() + static_cast<float> (at / beats) * plot.getWidth(); };
        g.setColour (muted.withAlpha (0.6f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("HIGH", 8, static_cast<int> (plot.getY()), 42, 14, juce::Justification::centredRight);
        g.drawText ("LOW", 8, static_cast<int> (plot.getBottom()) - 14, 42, 14, juce::Justification::centredRight);
        // Sparse relative guides replace the literal chromatic piano-roll grid.
        for (int row = 0; row < 4; ++row)
        {
            const float y = plot.getY() + 10 + static_cast<float> (row) / 3 * (plot.getHeight() - 20);
            g.setColour (ink.withAlpha (0.045f));
            g.drawHorizontalLine (static_cast<int> (y), plot.getX(), plot.getRight());
        }
        // A real time ruler is distinct from the smaller, unlabeled rate ticks.
        // Keep both grids legible at the fastest free rate / longest interval.
        const double unitsPerBeat = settings.sync || settings.retrigger == 2 ? 1.0 : 60000.0 / tempo;
        const double unitSpan = beats * unitsPerBeat;
        const double desired = unitSpan / juce::jmax (2.0, plot.getWidth() / 90.0);
        const double magnitude = std::pow (10.0, std::floor (std::log10 (desired)));
        const double normalized = desired / magnitude;
        const double tick = (normalized <= 1 ? 1 : normalized <= 2 ? 2
                                               : normalized <= 5   ? 5
                                                                   : 10)
                            * magnitude;
        g.setFont (juce::FontOptions (10.0f));
        for (double unit = 0; unit < unitSpan - tick * 0.2; unit += tick)
        {
            if (unit > 0 && xAt (unit / unitsPerBeat) > plot.getRight() - 110)
                continue;
            g.setColour (muted);
            g.drawText (juce::String (unit, unitSpan < 1 ? 2 : 1), static_cast<int> (xAt (unit / unitsPerBeat)), static_cast<int> (plot.getY()) - 18, 40, 14, juce::Justification::left);
        }
        g.setColour (settings.retrigger == 2 ? gold : mint);
        g.drawText (timeText (beats), static_cast<int> (plot.getRight()) - 92, static_cast<int> (plot.getY()) - 18, 92, 14, juce::Justification::right);
        const double visibleSteps = beats / stepBeats;
        const int gridStride = juce::jmax (1, static_cast<int> (std::ceil (visibleSteps / juce::jmax (1.0f, plot.getWidth() / 10.0f))));
        for (int i = 0; i <= static_cast<int> (std::ceil (visibleSteps)); i += gridStride)
        {
            const double at = i * stepBeats;
            if (at > beats + 1.0e-8)
                break;
            const float x = xAt (at);
            g.setColour (ink.withAlpha (0.1f));
            const float dash[] { 1, 5 };
            g.drawDashedLine ({ x, plot.getY(), x, timing.getBottom() }, dash, 2);
        }
        // Ordinary order-cycle wraps preserve the velocity envelope and rhythm.
        // Gold belongs only to the beat restart, so these are deliberately mint.
        const int cycleStride = juce::jmax (1, static_cast<int> (std::ceil ((beats / naturalBeats) / juce::jmax (1.0f, plot.getWidth() / 90.0f))));
        for (double at = naturalBeats * cycleStride; at < beats - 1.0e-8; at += naturalBeats * cycleStride)
        {
            const float x = xAt (at);
            g.setColour (mint.withAlpha (0.3f));
            const float dash[] { 4, 5 };
            g.drawDashedLine ({ x, plot.getY(), x, velocities.getBottom() }, dash, 2);
            if (!compact && !narrow && beats / naturalBeats <= 12)
            {
                g.setColour (mint);
                g.drawText ("cycle wraps", static_cast<int> (x) + 5, static_cast<int> (plot.getY()) - 33, 80, 14, juce::Justification::left);
            }
        }
        const double repeatEnd = settings.repeats > 0 ? naturalBeats * settings.repeats : beats;
        if (repeatEnd < beats)
        {
            g.setColour (ink.withAlpha (0.025f));
            g.fillRect (xAt (repeatEnd), plot.getY(), plot.getRight() - xAt (repeatEnd), velocities.getBottom() - plot.getY());
            if (plot.getRight() - xAt (repeatEnd) > 80)
            {
                g.setColour (muted);
                g.drawText ("Repeat limit / waits for restart", static_cast<int> (xAt (repeatEnd)) + 8, static_cast<int> (plot.getCentreY()), static_cast<int> (plot.getRight() - xAt (repeatEnd)) - 16, 30, juce::Justification::centred);
            }
        }
        juce::Path contour, silhouette;
        bool first = true;
        int previousCycle = -1;
        for (const auto& note : notes)
        {
            const auto r = noteBounds (note);
            silhouette.addRoundedRectangle (r, 3);
            const int cycle = static_cast<int> (note.start / naturalBeats);
            if (first || cycle != previousCycle)
            {
                contour.startNewSubPath (r.getX(), r.getCentreY());
                first = false;
            }
            else
                contour.lineTo (r.getX(), r.getCentreY());
            previousCycle = cycle;
        }
        g.saveState();
        g.reduceClipRegion (plot.expanded (0, 6).toNearestInt());
        g.setColour (mint.withAlpha (0.16f));
        g.strokePath (contour, juce::PathStrokeType (1.3f));
        if (notes.size() <= 512)
            noteGlow.render (g, silhouette);
        for (const auto& note : notes)
        {
            const auto r = noteBounds (note);
            const auto colour = mint.interpolatedWith (gold, static_cast<float> (note.velocity) / 127);
            g.setColour (colour.withAlpha (0.55f + static_cast<float> (note.velocity) / 300));
            g.fillRoundedRectangle (r, 3);
            g.setColour (ink.withAlpha (0.7f));
            g.fillRoundedRectangle (r.withWidth (juce::jmin (3.0f, r.getWidth())), 1);
        }
        if (hovered >= 0)
        {
            g.setColour (ink);
            g.drawRoundedRectangle (noteBounds (notes[static_cast<size_t> (hovered)]).expanded (1), 3, 1);
        }
        g.restoreState();
        g.setFont (juce::FontOptions (10.0f));
        g.setColour (muted);
        g.drawText ("TIME", 8, static_cast<int> (timing.getY()), 42, 14, juce::Justification::centredRight);
        g.drawText ("VEL", 8, static_cast<int> (velocities.getY()), 42, 14, juce::Justification::centredRight);
        // Actual onset strokes can be compared with the dotted straight grid above.
        // No nearest-grid mapping is assumed: swing can move whole groups.
        juce::Path velocityCurve;
        for (const auto& note : notes)
        {
            const float x = xAt (note.start);
            g.setColour (mint.withAlpha (0.4f));
            g.drawLine (x, timing.getY(), x, timing.getCentreY(), 1);
            g.fillRoundedRectangle (x, timing.getCentreY(), juce::jmax (2.0f, noteBounds (note).getWidth()), 3, 1);
            const float y = velocities.getBottom() - static_cast<float> (note.velocity) / 127 * velocities.getHeight();
            g.setColour (gold.withAlpha (0.6f));
            g.fillRect (x, y, 2.0f, velocities.getBottom() - y);
            velocityCurve.startNewSubPath (x, y);
            velocityCurve.lineTo (xAt (note.end), y);
        }
        g.setColour (gold.withAlpha (0.7f));
        g.strokePath (velocityCurve, juce::PathStrokeType (1));
        g.setColour (settings.retrigger == 2 ? gold : mint);
        const float dash[] { 3, 4 };
        g.drawDashedLine ({ plot.getRight(), plot.getY(), plot.getRight(), velocities.getBottom() }, dash, 2);
        if (!compact)
        {
            g.setColour (muted);
            g.setFont (juce::FontOptions (11.0f));
            const auto end = settings.retrigger != 2 ? (settings.repeats == 0 ? juce::String ("Order cycle wraps / envelope continues") : juce::String ("Order cycle complete / finite repeat limit applies"))
                             : beats < naturalBeats  ? juce::String ("Beat restart interrupts the order cycle")
                                                     : "Restart at " + timeText (beats) + " / order cycle " + timeText (naturalBeats);
            g.drawText (end, 58, static_cast<int> (velocities.getBottom()) + 7, getWidth() - 78, 16, juce::Justification::right);
            if (!narrow)
                g.drawText ("Height: relative pitch    Width: visible gate / tie    Dotted: straight timing    Gold lane: attack velocity", 58, static_cast<int> (velocities.getBottom()) + 25, getWidth() - 78, 16, juce::Justification::left);
        }
        if (notes.empty())
        {
            g.setColour (muted);
            g.drawText ("Silent cycle / check gate pattern and velocity", plot, juce::Justification::centred);
        }
    }
    void ArpVisualizer::mouseMove (const juce::MouseEvent& event)
    {
        for (size_t i = 0; i < notes.size(); ++i)
            if (noteBounds (notes[i]).contains (event.position))
            {
                hovered = static_cast<int> (i);
                const auto& note = notes[i];
                inspection.setText ("Relative pitch " + juce::String (note.note - referenceBase) + " st / onset " + juce::String (note.start, 3)
                                        + " beats / visible length " + juce::String (note.end - note.start, 3) + (reachesWindowEdge[i] ? " beats (window edge) / velocity " : " beats / velocity ") + juce::String (note.velocity),
                    juce::dontSendNotification);
                repaint();
                return;
            }
        mouseExit (event);
    }
    void ArpVisualizer::mouseExit (const juce::MouseEvent&)
    {
        hovered = -1;
        inspection.setText (description(), juce::dontSendNotification);
        repaint();
    }
}
