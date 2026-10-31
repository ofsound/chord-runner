#pragma once
#include "../engine/Timeline.h"
#include "UiPalette.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace chordrunner::ui
{
class ChordInput final : public juce::TextEditor
{
public:
    std::function<bool (const juce::KeyPress&)> completionKey;
    std::function<void (bool)> caretMoved;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseUp (const juce::MouseEvent& event) override;
};

class ChordCompletion final : public juce::Component, private juce::ListBoxModel
{
public:
    struct Match { juce::String chord, name, degrees, family; };
    struct Context
    {
        int begin = 0, end = 0;
        std::vector<Match> matches;
    };
    static Context suggestions (const juce::String&, int caret);
    explicit ChordCompletion (juce::TextEditor&);
    void refresh (bool reopen = false);
    void dismiss();
    void position();
    bool handleKey (const juce::KeyPress&);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    juce::TextEditor& input;
    juce::ListBox list;
    Context context;
    juce::String source;
    int sourceCaret = -1, suppressedCaret = -1;
    juce::String suppressedText;
    bool accepting = false;
    int getNumRows() override;
    juce::String getNameForRow (int) override;
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override;
    void listBoxItemClicked (int, const juce::MouseEvent&) override;
    void accept (int);
};
}
