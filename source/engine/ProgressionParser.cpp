#include "Timeline.h"
#include <cmath>
#include <regex>

namespace chordrunner
{
juce::Result parseProgression (const juce::String& text, Timeline& destination)
{
    if (text.length() > 4096)
        return juce::Result::fail ("Keep the progression under 4096 characters.");
    const auto tokens = juce::StringArray::fromTokens (text.replace (juce::String::charToString (0x266f), "#").replace (juce::String::charToString (0x266d), "b"), " \t\r\n|-", "");
    auto next = destination;
    next.count = 0;
    // 7, maj7 and m7 are chord qualities; use @7 for a seven-bar duration.
    static const std::regex syntax (R"(^([A-Ga-g])([#b]?)(maj7|min7|dim7|m7b5|sus2|sus4|maj|min|dim|aug|m7|m|7|5)?(@?[0-9]+(?:\.[0-9]+)?(?:/[0-9]+(?:\.[0-9]+)?)?)?$)");
    for (const auto& token : tokens)
    {
        if (token.isEmpty())
            continue;
        if (next.count == maxChords)
            return juce::Result::fail ("Use at most 64 chords.");
        const auto input = token.toStdString();
        std::smatch match;
        if (token.length() > 31 || ! std::regex_match (input, match, syntax))
            return juce::Result::fail ("Cannot read '" + token + "'. Try C, F#m, Bbmaj7 or Am1/2.");

        auto& chord = next.chords[static_cast<size_t> (next.count++)];
        chord = Chord {};
        const auto root = juce::String (match[1].str()).toUpperCase()[0];
        constexpr std::array<int, 7> pitchClasses { 9, 11, 0, 2, 4, 5, 7 };
        int pitch = pitchClasses[static_cast<size_t> (root - 'A')];
        const auto accidental = match[2].str();
        if (accidental == "#") ++pitch;
        if (accidental == "b") --pitch;
        const auto quality = match[3].str();
        std::array<int, maxNotes> intervals { 0, 4, 7 };
        if (quality == "m" || quality == "min") intervals = { 0, 3, 7 };
        else if (quality == "dim") intervals = { 0, 3, 6 };
        else if (quality == "aug") intervals = { 0, 4, 8 };
        else if (quality == "sus2") intervals = { 0, 2, 7 };
        else if (quality == "sus4") intervals = { 0, 5, 7 };
        else if (quality == "5") { intervals = { 0, 7 }; chord.noteCount = 2; }
        else if (quality == "7") { intervals = { 0, 4, 7, 10 }; chord.noteCount = 4; }
        else if (quality == "maj7") { intervals = { 0, 4, 7, 11 }; chord.noteCount = 4; }
        else if (quality == "m7" || quality == "min7") { intervals = { 0, 3, 7, 10 }; chord.noteCount = 4; }
        else if (quality == "dim7") { intervals = { 0, 3, 6, 9 }; chord.noteCount = 4; }
        else if (quality == "m7b5") { intervals = { 0, 3, 6, 10 }; chord.noteCount = 4; }
        for (int i = 0; i < chord.noteCount; ++i)
        {
            const int note = (next.octave + 1) * 12 + pitch + intervals[static_cast<size_t> (i)];
            if (note < 0 || note > 127)
                return juce::Result::fail ("The chord falls outside MIDI notes 0-127. Choose another octave.");
            chord.notes[static_cast<size_t> (i)] = note;
        }
        juce::String duration (match[4].str());
        if (duration.startsWithChar ('@')) duration = duration.substring (1);
        double bars = 1.0;
        if (duration.isNotEmpty())
        {
            bars = duration.getDoubleValue();
            if (duration.containsChar ('/'))
            {
                const auto denominator = duration.fromFirstOccurrenceOf ("/", false, false).getDoubleValue();
                if (denominator <= 0.0)
                    return juce::Result::fail ("A duration denominator must be greater than zero.");
                bars /= denominator;
            }
        }
        if (! std::isfinite (bars) || bars < 1.0 / 16.0 || bars > 16.0)
            return juce::Result::fail ("Chord durations must be between 1/16 and 16 bars.");
        chord.beats = bars * 4.0;
        token.copyToUTF8 (chord.label.data(), chord.label.size());
    }
    if (next.count == 0)
        return juce::Result::fail ("Enter a progression, for example C - G - Am - F.");
    destination = next;
    return juce::Result::ok();
}
}
