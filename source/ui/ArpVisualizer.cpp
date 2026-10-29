#include "ArpVisualizer.h"

namespace chordrunner::ui
{
    namespace
    {
        const juce::Colour ink (0xffe9ecee), muted (0xff929da8), mint (0xff83dabb), gold (accentYellow);
        juce::Colour velocityColour (int velocity)
        {
            const float amount = juce::jlimit (0.0f, 1.0f, static_cast<float> (velocity) / 127.0f);
            return accentYellow.withMultipliedBrightness (0.06f + 0.94f * std::pow (amount, 1.25f));
        }
        const juce::StringArray styles { "Up", "Down", "Up & Down", "Down & Up", "Converge", "Diverge", "Play order", "Chord trigger", "Random", "Random other", "Random once" };
    }
    ArpVisualizer::ArpVisualizer()
    {
        setComponentID ("arp.visualizer");
        inspection.setColour (juce::Label::textColourId, muted);
        inspection.setFont (juce::FontOptions (11.0f));
        addAndMakeVisible (inspection);
        inspection.setText ("Height = relative pitch / width = gate / dim = soft / bright = hard / dotted ticks = straight timing", juce::dontSendNotification);
    }
    void ArpVisualizer::showOutputPulse (juce::uint64 serial, juce::uint64 ordinal, int velocity, double ageMs, juce::uint64 rhythmOrdinal)
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (serial != pulseSerial)
        {
            pulseSerial = serial;
            pulseOrdinal = ordinal;
            pulseRhythmOrdinal = rhythmOrdinal;
            pulseStarted = now - ageMs;
        }
        const float next = serial == 0 || now < pulseStarted ? 0.0f : static_cast<float> (juce::jlimit (0.0, 1.0, 1.0 - (now - pulseStarted) / 140.0))
            * (0.5f + static_cast<float> (velocity) / 254.0f);
        if (std::abs (pulse - next) > 0.001f) { pulse = next; repaint(); }
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
        const int rhythmLength = juce::jmax (settings.gateSequenceEnabled ? settings.gateLength : 1,
            settings.tieSequenceEnabled ? settings.tieLength : 1);
        const double period = settings.retrigger == 2 ? settings.interval : naturalBeats;
        beats = std::ceil ((juce::jmax (period, rhythmLength * stepBeats) - 1.0e-9) / period) * period;
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
            return (settings.retrigger == 2 ? "Restart: " + timeText (settings.interval) : settings.retrigger == 1 ? juce::String ("Note restart")
                                                                                                       : juce::String ("Restart off"))
                   + " / " + (settings.repeats == 0 ? juce::String ("Infinite repeats") : "Repeats: " + juce::String (settings.repeats))
                   + (settings.hold ? " / Hold on" : " / Hold off");
        const auto restart = settings.retrigger == 2   ? "Yellow = restart every " + timeText (settings.interval)
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
        g.setColour (mint.withAlpha (0.15f + pulse * 0.85f));
        g.fillEllipse (18, 18, 6, 6);
        g.setFont (juce::FontOptions (10.0f)); g.setColour (muted);
        g.drawText ("MIDI OUT", 30, 11, 70, 20, juce::Justification::left);
        g.setFont (juce::FontOptions (11.0f));
        g.setColour (muted);
        g.drawText (timeText (beats) + (settings.retrigger == 2 && beats > settings.interval ? " shown" : ""), getWidth() - 126, 10, 110, 24, juce::Justification::centredRight);
        juce::String relationship;
        if (settings.retrigger == 2)
        {
            if (settings.repeats > 0 && naturalBeats * settings.repeats < settings.interval)
                relationship = juce::String (settings.repeats) + (settings.repeats == 1 ? " cycle + rest" : " cycles + rest");
            else if (settings.interval < naturalBeats)
                relationship = "Restart interrupts cycle";
            else
            {
                const double cycles = settings.interval / naturalBeats;
                relationship = juce::String (cycles, std::abs (cycles - std::round (cycles)) < 1.0e-6 ? 0 : 2)
                               + (std::abs (cycles - 1.0) < 1.0e-6 ? " cycle per restart" : " cycles per restart");
            }
        }
        const auto source = heldCount > 0 ? juce::String (heldCount) + (narrow ? " held" : heldCount == 1 ? " held note"
                                                                                                          : " held notes")
                                          : juce::String (narrow ? "4-note demo" : "4-note example (no input)");
        const auto context = source + " / cycle " + timeText (naturalBeats)
                             + (relationship.isEmpty() ? juce::String() : " / " + relationship)
            + (settings.retrigger == 2 && beats > settings.interval ? " / restart every " + timeText (settings.interval) : "");
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
        // Yellow marks the beat restart, so these are deliberately mint.
        const double period = settings.retrigger == 2 ? settings.interval : beats;
        for (double base = 0; base < beats - 1.0e-8; base += period)
        {
            const double end = juce::jmin (beats, base + period);
            const int stride = juce::jmax (1, static_cast<int> (std::ceil ((period / naturalBeats) / juce::jmax (1.0f, plot.getWidth() / 90.0f))));
            for (double at = base + naturalBeats * stride; at < end - 1.0e-8; at += naturalBeats * stride)
            {
                const float x = xAt (at);
                g.setColour (mint.withAlpha (0.3f));
                const float dash[] { 4, 5 };
                g.drawDashedLine ({ x, plot.getY(), x, velocities.getBottom() }, dash, 2);
            }
            if (settings.retrigger == 2 && base > 0)
            {
                g.setColour (gold.withAlpha (0.65f));
                const float dash[] { 3, 4 };
                g.drawDashedLine ({ xAt (base), plot.getY(), xAt (base), velocities.getBottom() }, dash, 2);
            }
            const double repeatEnd = base + naturalBeats * settings.repeats;
            if (settings.repeats > 0 && repeatEnd < end)
            {
                g.setColour (ink.withAlpha (0.025f));
                g.fillRect (xAt (repeatEnd), plot.getY(), xAt (end) - xAt (repeatEnd), velocities.getBottom() - plot.getY());
                if (xAt (end) - xAt (repeatEnd) > 120)
                {
                    g.setColour (muted);
                    g.drawText ("Repeat limit / waiting", static_cast<int> (xAt (repeatEnd)) + 8,
                        static_cast<int> (plot.getCentreY()), static_cast<int> (xAt (end) - xAt (repeatEnd)) - 16, 30, juce::Justification::centred);
                }
            }
        }
        // Gate-off slots remain visible as rests, even when the entire cycle is silent.
        if (settings.gateSequenceEnabled)
            for (int i = 0; i < static_cast<int> (std::ceil (visibleSteps)); ++i)
            {
                if ((settings.gateMask & (1 << (i % settings.gateLength))) != 0) continue;
                const float left = xAt (i * stepBeats), right = xAt (juce::jmin (beats, (i + 1) * stepBeats));
                if (right - left < 7) continue;
                g.setColour (muted.withAlpha (0.04f));
                g.fillRect (left, plot.getY(), right - left, plot.getHeight());
                const float x = (left + right) * 0.5f, y = timing.getCentreY();
                g.setColour (muted.withAlpha (0.7f));
                g.drawLine (x - 3, y - 3, x + 3, y + 3, 1);
                g.drawLine (x - 3, y + 3, x + 3, y - 3, 1);
                if (! compact && right - left > 36)
                    g.drawText ("REST", static_cast<int> (left), static_cast<int> (plot.getCentreY()), static_cast<int> (right - left), 16, juce::Justification::centred);
            }
        juce::Path contour;
        std::array<juce::Path, 4> silhouettes;
        bool first = true;
        int previousCycle = -1;
        for (const auto& note : notes)
        {
            const auto r = noteBounds (note);
            silhouettes[static_cast<size_t> (juce::jmin (3, note.velocity / 32))].addRoundedRectangle (r, 3);
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
            for (size_t i = 0; i < noteGlows.size(); ++i)
                if (! silhouettes[i].isEmpty()) noteGlows[i].render (g, silhouettes[i]);
        for (const auto& note : notes)
        {
            const auto r = noteBounds (note);
            const auto colour = velocityColour (note.velocity);
            g.setColour (colour);
            g.fillRoundedRectangle (r, 3);
            g.setColour (colour.withMultipliedBrightness (1.1f));
            g.fillRoundedRectangle (r.withWidth (juce::jmin (3.0f, r.getWidth())), 1);
        }
        if (settings.tieSequenceEnabled)
            for (const auto& note : notes)
            {
                const auto r = noteBounds (note);
                bool joined = false;
                for (int i = static_cast<int> (std::floor (note.start / stepBeats)) + 1; i * stepBeats < note.end - 1.0e-6; ++i)
                {
                    if ((settings.tieMask & (1 << (i % settings.tieLength))) == 0) continue;
                    const float x = xAt (i * stepBeats);
                    g.setColour (ink.withAlpha (0.65f));
                    g.fillEllipse (x - 2, r.getCentreY() - 2, 4, 4);
                    joined = true;
                }
                if (joined && r.getWidth() > 42 && r.getHeight() >= 10)
                {
                    g.setFont (juce::FontOptions (9.0f));
                    g.setColour (juce::Colour (0xff101820));
                    g.drawText ("TIE", r.toNearestInt(), juce::Justification::centred);
                }
            }
        if (pulse > 0 && ! notes.empty())
        {
            const auto ordinal = settings.gateSequenceEnabled || settings.tieSequenceEnabled
                ? pulseRhythmOrdinal % static_cast<juce::uint64> (juce::jmax (1, static_cast<int> (std::ceil (beats / stepBeats))))
                : settings.retrigger == 2 ? pulseOrdinal : pulseOrdinal % static_cast<juce::uint64> (stepCount);
            const auto subdivision = static_cast<juce::uint64> (1u << juce::jlimit (0, 2, settings.swingSubdivision));
            const double swing = (ordinal / subdivision) % 2 != 0 ? static_cast<double> (subdivision) * 0.5 * settings.swing / 100.0 : 0;
            const double at = (static_cast<double> (ordinal) + swing) * stepBeats;
            size_t closest = 0;
            for (size_t i = 1; i < notes.size(); ++i)
                if (std::abs (notes[i].start - at) < std::abs (notes[closest].start - at)) closest = i;
            const auto r = noteBounds (notes[closest]);
            g.setColour (velocityColour (notes[closest].velocity).withAlpha (pulse * 0.2f)); g.fillRoundedRectangle (r.expanded (5), 5);
            g.setColour (velocityColour (notes[closest].velocity).withMultipliedBrightness (1.12f).withAlpha (pulse)); g.fillRoundedRectangle (r, 3);
            if (settings.style == ArpeggiatorSettings::chordTrigger)
                for (const auto& note : notes)
                    if (std::abs (note.start - notes[closest].start) < 0.001) g.fillRoundedRectangle (noteBounds (note), 3);
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
        for (const auto& note : notes)
        {
            const float x = xAt (note.start);
            g.setColour (mint.withAlpha (0.4f));
            g.drawLine (x, timing.getY(), x, timing.getCentreY(), 1);
            g.fillRoundedRectangle (x, timing.getCentreY(), juce::jmax (2.0f, noteBounds (note).getWidth()), 3, 1);
            const float y = velocities.getBottom() - static_cast<float> (note.velocity) / 127 * velocities.getHeight();
            g.setColour (velocityColour (note.velocity).withAlpha (0.85f));
            g.fillRect (x, y, 2.0f, velocities.getBottom() - y);
            g.drawLine (x, y, xAt (note.end), y, 1.5f);
        }
        g.setColour (settings.retrigger == 2 ? gold : mint);
        const float dash[] { 3, 4 };
        g.drawDashedLine ({ plot.getRight(), plot.getY(), plot.getRight(), velocities.getBottom() }, dash, 2);
        if (!compact)
        {
            g.setColour (muted);
            g.setFont (juce::FontOptions (11.0f));
            const auto end = settings.retrigger != 2 ? (settings.repeats == 0 ? juce::String ("Order cycle wraps / envelope continues") : juce::String ("Order cycle complete / finite repeat limit applies"))
                             : settings.interval < naturalBeats  ? juce::String ("Beat restart interrupts the order cycle")
                                                     : "Restart every " + timeText (settings.interval) + " / order cycle " + timeText (naturalBeats);
            g.drawText (end, 58, static_cast<int> (velocities.getBottom()) + 7, getWidth() - 78, 16, juce::Justification::right);
            if (!narrow)
                g.drawText ("Height: relative pitch    Width: visible gate / tie    Dotted: straight timing    Color: dim = soft / bright = hard    X: gate rest    Dots: ties", 58, static_cast<int> (velocities.getBottom()) + 25, getWidth() - 78, 16, juce::Justification::left);
        }
        if (notes.empty())
        {
            g.setColour (muted);
            g.drawText ("Silent output / gate pattern or velocity suppresses these attacks", plot, juce::Justification::centred);
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
