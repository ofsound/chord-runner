#pragma once
#include "TimelineEngine.h"
#include <vector>

namespace chordrunner
{
struct PreviewNote
{
    int note = 0, velocity = 0;
    double start = 0.0, end = 0.0;
};
// Non-realtime visual audition through the production schedulers. Random and
// humanized phrases illustrate one realization, not a prediction of future takes.
std::vector<PreviewNote> renderOutputPreview (const Timeline&, double maximumBeats = 64.0);
}
