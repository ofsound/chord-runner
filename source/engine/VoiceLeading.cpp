#include "Timeline.h"
#include <algorithm>
#include <limits>
#include <vector>

namespace chordrunner
{
namespace
{
using Voicing = std::array<int, maxNotes>;
double centre (const Voicing& notes, int count)
{
    double sum = 0.0;
    for (int i = 0; i < count; ++i) sum += notes[static_cast<size_t> (i)];
    return sum / count;
}
double movement (const Voicing& a, int na, const Voicing& b, int nb)
{
    const int count = juce::jmax (na, nb);
    double cost = 0.0;
    for (int i = 0; i < count; ++i)
    {
        const int ia = count == 1 ? 0 : juce::roundToInt (static_cast<double> (i * (na - 1)) / (count - 1));
        const int ib = count == 1 ? 0 : juce::roundToInt (static_cast<double> (i * (nb - 1)) / (count - 1));
        const double delta = a[static_cast<size_t> (ia)] - b[static_cast<size_t> (ib)];
        cost += delta * delta;
    }
    return cost / count;
}
}
void applyVoiceLeading (Timeline& timeline)
{
    if (timeline.voiceLeading == 0 || timeline.count < 2) return;
    struct Candidate { Voicing notes; double cost = 0.0; int previous = -1; };
    std::vector<std::vector<Candidate>> layers (static_cast<size_t> (timeline.count));
    layers[0].push_back ({ timeline.chords[0].notes, 0.0, -1 });
    const double anchor = centre (timeline.chords[0].notes, timeline.chords[0].noteCount);
    const double direction = timeline.voiceLeading == 2 ? 12.0 : timeline.voiceLeading == 3 ? -12.0 : 0.0;
    for (int index = 1; index < timeline.count; ++index)
    {
        const auto& chord = timeline.chords[static_cast<size_t> (index)];
        auto compact = chord.notes;
        for (int n = 1; n < chord.noteCount; ++n)
            while (compact[static_cast<size_t> (n)] >= compact[0] + 12) compact[static_cast<size_t> (n)] -= 12;
        std::sort (compact.begin(), compact.begin() + chord.noteCount);
        auto inversion = chord.notes;
        auto& layer = layers[static_cast<size_t> (index)];
        const auto& previous = layers[static_cast<size_t> (index - 1)];
        const double target = juce::jlimit (0.0, 127.0, anchor + direction * index / (timeline.count - 1));
        for (int rotation = -1; rotation < chord.noteCount; ++rotation)
        {
            for (int octaveShift = -10; octaveShift <= 10; ++octaveShift)
            {
                auto notes = inversion;
                for (int n = 0; n < chord.noteCount; ++n) notes[static_cast<size_t> (n)] += octaveShift * 12;
                if (notes[0] < 0 || notes[static_cast<size_t> (chord.noteCount - 1)] > 127) continue;
                Candidate candidate { notes, std::numeric_limits<double>::max(), -1 };
                const double delta = centre (notes, chord.noteCount) - target;
                const double registerCost = delta * delta * (direction == 0.0 ? 0.05 : 4.0);
                for (size_t p = 0; p < previous.size(); ++p)
                {
                    const double cost = previous[p].cost + registerCost
                        + movement (previous[p].notes, timeline.chords[static_cast<size_t> (index - 1)].noteCount, notes, chord.noteCount);
                    if (cost < candidate.cost)
                    {
                        candidate.cost = cost;
                        candidate.previous = static_cast<int> (p);
                    }
                }
                layer.push_back (candidate);
            }
            if (chord.noteCount == 1) break;
            if (rotation == -1) { inversion = compact; continue; }
            const int bass = inversion[0];
            std::move (inversion.begin() + 1, inversion.begin() + chord.noteCount, inversion.begin());
            int raised = bass;
            while (raised <= inversion[static_cast<size_t> (chord.noteCount - 2)]) raised += 12;
            inversion[static_cast<size_t> (chord.noteCount - 1)] = raised;
        }
    }
    // Smooth also considers the return to the first chord. Directional modes
    // intentionally omit that edge, preserving the ramp's loop reset.
    if (timeline.voiceLeading == 1)
        for (auto& candidate : layers.back())
            candidate.cost += movement (candidate.notes, timeline.chords[static_cast<size_t> (timeline.count - 1)].noteCount,
                timeline.chords[0].notes, timeline.chords[0].noteCount);
    const auto& last = layers.back();
    int choice = static_cast<int> (std::min_element (last.begin(), last.end(), [] (const auto& a, const auto& b) { return a.cost < b.cost; }) - last.begin());
    for (int i = timeline.count - 1; i > 0; --i)
    {
        const auto& candidate = layers[static_cast<size_t> (i)][static_cast<size_t> (choice)];
        timeline.chords[static_cast<size_t> (i)].notes = candidate.notes;
        choice = candidate.previous;
    }
}
}
