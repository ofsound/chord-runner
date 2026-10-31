#include "ArpVisualizer.h"

namespace chordrunner::ui
{
    namespace
    {
        const juce::Colour ink (0xffe9ecee), muted (0xff929da8), mint (accentGreen), gold (accentYellow);
        juce::Colour velocityColour (int velocity)
        {
            const float amount = juce::jlimit (0.0f, 1.0f, static_cast<float> (velocity) / 127.0f);
            return accentYellow.withMultipliedBrightness (0.06f + 0.94f * std::pow (amount, 1.25f));
        }
    }
    ArpVisualizer::ArpVisualizer()
    {
        setComponentID ("arp.visualizer");
        inspection.setColour (juce::Label::textColourId, muted);
        inspection.setFont (uiFont (11.0f));
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
                                  ? " / Phrase restart resets velocity"
                                  : "";
        return restart + " / " + repeats + " / "
               + (settings.hold ? "Hold latches input" : "Hold off follows releases") + envelope;
    }

    void ArpVisualizer::resized()
    {
        plot = { 20.0f, 32.0f, static_cast<float> (getWidth() - 40), juce::jmax (24.0f, static_cast<float> (getHeight()) - 46.0f) };
    }
    juce::Rectangle<float> ArpVisualizer::noteBounds (const PreviewNote& note) const
    {
        const float h = juce::jlimit (4.0f, 14.0f, plot.getHeight() / static_cast<float> (high - low + 4) * 2.0f);
        // Reserve two note-heights of breathing room on each edge, scaling down for compact views.
        const float padding = juce::jmin (plot.getHeight() * 0.25f, juce::jmax (h * 2.5f, plot.getHeight() * 0.1f));
        const float y = plot.getBottom() - padding - (high > low ? static_cast<float> (note.note - low) / static_cast<float> (high - low) : 0.5f) * (plot.getHeight() - padding * 2.0f);
        return { plot.getX() + static_cast<float> (note.start / beats) * plot.getWidth(), y - h * 0.5f, juce::jmax (2.0f, static_cast<float> ((note.end - note.start) / beats) * plot.getWidth() - 2), h };
    }
    void ArpVisualizer::paint (juce::Graphics& g)
    {
        g.setColour (juce::Colour (0xff131c21));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 8);
        g.setColour (juce::Colour (0xff34434a));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8, 1);
        const auto xAt = [this] (double at) { return plot.getX() + static_cast<float> (at / beats) * plot.getWidth(); };
        g.setColour (muted.withAlpha (0.6f));
        g.setFont (uiFont (10.0f));
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
        g.setFont (uiFont (10.0f));
        for (double unit = 0; unit <= unitSpan + tick * 1.0e-6; unit += tick)
        {
            g.setColour (muted);
            const int labelX = static_cast<int> (juce::jmin (xAt (unit / unitsPerBeat), plot.getRight() - 40.0f));
            g.drawText (juce::String (unit, unitSpan < 1 ? 2 : 1), labelX, static_cast<int> (plot.getY()) - 18, 40, 14, juce::Justification::left);
        }
        const double visibleSteps = beats / stepBeats;
        const int gridStride = juce::jmax (1, static_cast<int> (std::ceil (visibleSteps / juce::jmax (1.0f, plot.getWidth() / 10.0f))));
        for (int i = 0; i <= static_cast<int> (std::ceil (visibleSteps)); i += gridStride)
        {
            const double at = i * stepBeats;
            if (at >= beats - 1.0e-8)
                break;
            const float x = xAt (at);
            g.setColour (ink.withAlpha (0.1f));
            const float dash[] { 1, 5 };
            g.drawDashedLine ({ x, plot.getY(), x, plot.getBottom() }, dash, 2);
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
                g.drawDashedLine ({ x, plot.getY(), x, plot.getBottom() }, dash, 2);
            }
            if (settings.retrigger == 2 && base > 0)
            {
                g.setColour (gold.withAlpha (0.65f));
                const float dash[] { 3, 4 };
                g.drawDashedLine ({ xAt (base), plot.getY(), xAt (base), plot.getBottom() }, dash, 2);
            }
            const double repeatEnd = base + naturalBeats * settings.repeats;
            if (settings.repeats > 0 && repeatEnd < end)
            {
                g.setColour (ink.withAlpha (0.025f));
                g.fillRect (xAt (repeatEnd), plot.getY(), xAt (end) - xAt (repeatEnd), plot.getBottom() - plot.getY());
                if (xAt (end) - xAt (repeatEnd) > 120)
                {
                    g.setColour (muted);
                    g.drawText ("Repeat limit / waiting", static_cast<int> (xAt (repeatEnd)) + 8,
                        static_cast<int> (plot.getCentreY()), static_cast<int> (xAt (end) - xAt (repeatEnd)) - 16, 30, juce::Justification::centred);
                }
            }
        }
        std::array<juce::Path, 4> silhouettes;
        for (const auto& note : notes)
        {
            const auto r = noteBounds (note);
            silhouettes[static_cast<size_t> (juce::jmin (3, note.velocity / 32))].addRoundedRectangle (r, 3);

        }
        g.saveState();
        g.reduceClipRegion (plot.expanded (0, 6).toNearestInt());
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
                    g.setFont (uiFont (9.0f));
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
