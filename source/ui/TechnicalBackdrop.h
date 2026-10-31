#pragma once
#include <juce_graphics/juce_graphics.h>

namespace chordrunner::ui
{
// Static vector geometry, rebuilt only when the editor size changes.
class TechnicalBackdrop final
{
public:
    void resize (int width, int height);
    void paint (juce::Graphics&, juce::Rectangle<int> clip) const;
private:
    int cachedWidth = 0, cachedHeight = 0;
    juce::Path grid, majorGrid, rearMesh, frontMesh, accents;
};
}
