#include "Timeline.h"
#include "BinaryData.h"
#include <cmath>
#include <regex>

namespace chordrunner
{
const juce::var& chordVocabulary()
{
    static const auto vocabulary = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::chords_json, BinaryData::chords_jsonSize));
    return vocabulary;
}

namespace
{
bool isLegacySuffix (const juce::String& suffix)
{
    static const juce::StringArray suffixes { "", "maj", "m", "min", "7", "maj7", "m7", "min7", "dim", "dim7", "m7b5", "aug", "sus2", "sus4", "5" };
    return suffixes.contains (suffix);
}

juce::Result parse (const juce::String& text, Timeline& destination, juce::String* migratedText)
{
    if (text.length() > 4096)
        return juce::Result::fail ("Keep the progression under 4096 characters.");
    // Map code points: JUCE 9 String::replace compares these UTF-8 accidentals incorrectly.
    const auto tokens = juce::StringArray::fromTokens (text.replaceCharacter (juce::juce_wchar (0x266f), '#').replaceCharacter (juce::juce_wchar (0x266d), 'b'), " \t\r\n|-", "");
    auto next = destination;
    next.count = 0;
    juce::StringArray migratedTokens;
    bool migrated = false;
    static const std::regex durationSyntax (R"(^[0-9]+(?:\.[0-9]+)?(?:/[0-9]+(?:\.[0-9]+)?)?$)");
    const auto* vocabulary = chordVocabulary().getArray();
    if (vocabulary == nullptr)
        return juce::Result::fail ("The chord vocabulary could not be loaded.");
    for (const auto& token : tokens)
    {
        if (token.isEmpty())
            continue;
        if (next.count == maxChords)
            return juce::Result::fail ("Use at most 64 chords.");
        const auto root = juce::String::charToString (token[0]).toUpperCase()[0];
        if (token.getNumBytesAsUTF8() >= Chord {}.label.size() || root < 'A' || root > 'G')
            return juce::Result::fail ("Cannot read '" + token + "'. Open Chord glossary for supported suffixes.");
        constexpr std::array<int, 7> pitchClasses { 9, 11, 0, 2, 4, 5, 7 };
        int pitch = pitchClasses[static_cast<size_t> (root - 'A')];
        int rootLength = 1;
        if (token[1] == '#' || token[1] == 'b')
        {
            pitch += token[1] == '#' ? 1 : -1;
            ++rootLength;
        }
        const auto tail = token.substring (rootLength);
        const auto at = tail.indexOfAnyOf ("@xX");
        const auto explicitQuality = at >= 0 ? tail.substring (0, at) : juce::String {};
        const auto explicitDuration = at >= 0 ? tail.substring (at + 1) : juce::String {};
        const juce::var* quality = nullptr;
        juce::String duration;
        int suffixLength = -1;
        const auto consider = [&] (const juce::var& entry, const juce::String& suffix)
        {
            if (suffix.length() <= suffixLength || (migratedText != nullptr && ! isLegacySuffix (suffix)))
                return;
            // Compare whole code points: JUCE 9 startsWith uses a character count
            // as a UTF-8 byte count, which can confuse ° with °7 or Δ7 with Δ9.
            if (at >= 0 ? explicitQuality != suffix : tail.substring (0, suffix.length()) != suffix)
                return;
            const auto remaining = at >= 0 ? explicitDuration : tail.substring (suffix.length());
            if ((at >= 0 || remaining.isNotEmpty()) && ! std::regex_match (remaining.toStdString(), durationSyntax))
                return;
            quality = &entry;
            duration = remaining;
            suffixLength = suffix.length();
        };
        for (const auto& entry : *vocabulary)
        {
            consider (entry, entry["suffix"].toString());
            if (const auto* aliases = entry["aliases"].getArray())
                for (const auto& alias : *aliases)
                    consider (entry, alias.toString());
        }
        if (quality == nullptr)
            return juce::Result::fail ("Cannot read '" + token + "'. Open Chord glossary; use @, x or X for durations (Dm9x2). Slash bass chords are not supported.");

        auto& chord = next.chords[static_cast<size_t> (next.count++)];
        chord = Chord {};
        const auto* intervals = (*quality)["intervals"].getArray();
        if (intervals == nullptr || intervals->isEmpty() || intervals->size() > maxNotes)
            return juce::Result::fail ("Invalid chord vocabulary entry.");
        chord.noteCount = intervals->size();
        for (int i = 0; i < chord.noteCount; ++i)
        {
            const int note = (next.octave + 1) * 12 + pitch + static_cast<int> ((*intervals)[i]);
            if (note < 0 || note > 127)
                return juce::Result::fail ("The chord falls outside MIDI notes 0-127. Choose another octave.");
            chord.notes[static_cast<size_t> (i)] = note;
        }
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
        auto label = token;
        if (migratedText != nullptr && at < 0 && duration.isNotEmpty())
        {
            label = token.substring (0, rootLength + suffixLength) + "@" + duration;
            migrated = true;
        }
        label.copyToUTF8 (chord.label.data(), chord.label.size());
        migratedTokens.add (label);
    }
    if (next.count == 0)
        return juce::Result::fail ("Enter a progression, for example C - G - Am - F.");
    for (auto& chord : next.chords) chord.toneOffsets.fill (0);
    applyVoiceLeading (next);
    destination = next;
    if (migratedText != nullptr)
        *migratedText = migrated ? migratedTokens.joinIntoString (" - ") : text;
    return juce::Result::ok();
}
}

juce::Result parseProgression (const juce::String& text, Timeline& destination)
{
    return parse (text, destination, nullptr);
}

juce::Result migrateLegacyProgression (const juce::String& text, Timeline& destination, juce::String& migratedText)
{
    return parse (text, destination, &migratedText);
}
}
