#include "ChordCompletion.h"

namespace chordrunner::ui
{
namespace
{
struct Entry
{
    juce::String suffix, name, degrees, family;
    juce::StringArray spellings;
};
const std::vector<Entry>& entries()
{
    static const auto cached = []
    {
        std::vector<Entry> result;
        for (const auto& chord : *chordVocabulary().getArray())
        {
            Entry entry { chord["suffix"].toString(), chord["name"].toString(), {}, chord["family"].toString(), {} };
            entry.spellings.add (entry.suffix);
            if (const auto* aliases = chord["aliases"].getArray())
                for (const auto& alias : *aliases) entry.spellings.add (alias.toString());
            juce::StringArray degrees;
            if (const auto* values = chord["degrees"].getArray())
                for (const auto& degree : *values) degrees.add (degree.toString());
            entry.degrees = degrees.joinIntoString (" ");
            result.push_back (std::move (entry));
        }
        return result;
    }();
    return cached;
}
}

bool ChordInput::keyPressed (const juce::KeyPress& key)
{
    if (completionKey && completionKey (key)) return true;
    const bool result = juce::TextEditor::keyPressed (key);
    if (caretMoved) caretMoved (false);
    return result;
}
void ChordInput::mouseUp (const juce::MouseEvent& event)
{
    juce::TextEditor::mouseUp (event);
    if (caretMoved) caretMoved (true);
}
ChordCompletion::Context ChordCompletion::suggestions (const juce::String& text, int caret)
{
    Context result;
    if (caret < 0 || caret > text.length()) return result;
    const auto separator = [] (juce::juce_wchar c) { return juce::CharacterFunctions::isWhitespace (c) || c == '|' || c == '-'; };
    int begin = caret;
    while (begin > 0 && ! separator (text[begin - 1])) --begin;
    int end = caret;
    while (end < text.length() && ! separator (text[end])) ++end;
    const auto token = text.substring (begin, end);
    if (token.isEmpty() || ! juce::String ("ABCDEFGabcdefg").containsChar (token[0])) return result;
    int rootLength = 1;
    if (token.length() > 1 && (token[1] == '#' || token[1] == 'b')) ++rootLength;
    const auto root = token.substring (0, rootLength);
    const auto tail = token.substring (rootLength);
    int qualityEnd = token.length();
    for (int i = rootLength; i < token.length(); ++i)
        if (token[i] == '@' || token[i] == 'x' || token[i] == 'X') { qualityEnd = i; break; }
    // Recognise existing implicit duration tails with the same longest-suffix rule
    // as the parser, so completing a chord never consumes its bar duration.
    if (qualityEnd == token.length())
    {
        int longest = -1;
        for (const auto& entry : entries())
            for (const auto& suffix : entry.spellings)
            {
                const auto remaining = tail.substring (suffix.length());
                if (tail.startsWith (suffix) && (remaining.isEmpty() ||
                    (juce::CharacterFunctions::isDigit (remaining[0]) && remaining.containsOnly ("0123456789./"))))
                    longest = juce::jmax (longest, suffix.length());
            }
        if (longest >= 0) qualityEnd = rootLength + longest;
    }
    if (caret > begin + qualityEnd) return result;
    const auto query = text.substring (begin + rootLength, juce::jmax (begin + rootLength, caret));
    result.begin = begin;
    result.end = begin + qualityEnd;
    struct Ranked { Match match; int rank; };
    std::vector<Ranked> ranked;
    for (const auto& entry : entries())
    {
        juce::String matched;
        int rank = 100;
        for (int i = 0; i < entry.spellings.size(); ++i)
        {
            const auto& spelling = entry.spellings[i];
            if (! spelling.startsWith (query)) continue;
            const int score = spelling == query ? 0 : i == 0 ? 1 : 2;
            if (score < rank) { rank = score; matched = spelling; }
        }
        if (rank == 100 && query.length() >= 3 && entry.name.startsWithIgnoreCase (query))
        {
            rank = 3;
            matched = entry.suffix;
        }
        if (rank < 100) ranked.push_back ({ { root + matched, entry.name, entry.degrees, entry.family }, rank });
    }
    std::stable_sort (ranked.begin(), ranked.end(), [] (const Ranked& a, const Ranked& b) { return a.rank < b.rank; });
    for (auto& item : ranked) result.matches.push_back (std::move (item.match));
    return result;
}
ChordCompletion::ChordCompletion (juce::TextEditor& editor) : input (editor)
{
    setComponentID ("progression.completion");
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
    list.setComponentID ("progression.completion.list");
    list.setTitle ("Chord suggestions");
    list.setModel (this);
    list.setRowHeight (52);
    list.setMultipleSelectionEnabled (false);
    list.setWantsKeyboardFocus (false);
    list.setMouseClickGrabsKeyboardFocus (false);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (list);
}
void ChordCompletion::refresh (bool reopen)
{
    if (reopen) { suppressedCaret = -1; sourceCaret = -1; }
    if (accepting) return;
    if (isVisible() && source == input.getText() && sourceCaret == input.getCaretPosition()) return;
    if (input.getText() == suppressedText && input.getCaretPosition() == suppressedCaret) return;
    if (! input.getHighlightedRegion().isEmpty()) { dismiss(); return; }
    source = input.getText();
    sourceCaret = input.getCaretPosition();
    context = suggestions (source, sourceCaret);
    if (context.matches.empty()) { dismiss(); return; }
    list.updateContent();
    list.selectRow (0);
    setVisible (true);
    position();
    toFront (false);
}
void ChordCompletion::dismiss()
{
    suppressedText = input.getText();
    suppressedCaret = input.getCaretPosition();
    setVisible (false);
}
void ChordCompletion::position()
{
    auto* parent = getParentComponent();
    if (parent == nullptr || ! isVisible()) return;
    if (! input.isVisible()) { dismiss(); return; }
    const auto entry = parent->getLocalArea (&input, input.getLocalBounds());
    const int width = juce::jmin (620, parent->getWidth() - 20);
    const auto caret = input.getCaretRectangleForCharIndex (context.begin);
    const int tokenX = parent->getLocalPoint (&input, caret.getTopLeft()).getX();
    const int left = juce::jlimit (10, juce::jmax (10, parent->getWidth() - width - 10), tokenX);
    const int top = entry.getBottom() + 3;
    const int height = juce::jmin (getNumRows() * 52 + 28, juce::jmin (340, parent->getHeight() - top - 46));
    if (height < 80) { dismiss(); return; }
    setBounds (left, top, width, height);
}
bool ChordCompletion::handleKey (const juce::KeyPress& key)
{
    if (! isVisible())
    {
        if (key.getModifiers().isCtrlDown() && key.getKeyCode() == juce::KeyPress::spaceKey) { refresh (true); return true; }
        return false;
    }
    if (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()) return false;
    if (key == juce::KeyPress::escapeKey) { dismiss(); return true; }
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        list.selectRow (juce::jlimit (0, getNumRows() - 1, list.getSelectedRow() + (key == juce::KeyPress::downKey ? 1 : -1)));
        return true;
    }
    if (key == juce::KeyPress::tabKey || key == juce::KeyPress::returnKey) { accept (list.getSelectedRow()); return true; }
    return false;
}
void ChordCompletion::accept (int row)
{
    if (! juce::isPositiveAndBelow (row, getNumRows())) return;
    // A caret move or outside edit must not apply an obsolete replacement span.
    if (input.getText() != source || input.getCaretPosition() != sourceCaret) { refresh(); return; }
    const auto chord = context.matches[static_cast<size_t> (row)].chord;
    accepting = true;
    input.setHighlightedRegion ({ context.begin, context.end });
    input.insertTextAtCaret (chord);
    accepting = false;
    dismiss();
}
int ChordCompletion::getNumRows() { return static_cast<int> (context.matches.size()); }
juce::String ChordCompletion::getNameForRow (int row)
{
    if (! juce::isPositiveAndBelow (row, getNumRows())) return {};
    const auto& item = context.matches[static_cast<size_t> (row)];
    return item.chord + ": " + item.name + ", " + item.family + ", degrees " + item.degrees;
}
void ChordCompletion::listBoxItemClicked (int row, const juce::MouseEvent&) { accept (row); }
void ChordCompletion::resized() { list.setBounds (getLocalBounds().reduced (1).withTrimmedBottom (24)); }
void ChordCompletion::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff1b252d));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 5);
    g.setColour (juce::Colour (0xff657780));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 5, 1);
    g.setFont (uiFont (10.0f));
    g.setColour (juce::Colour (0xff9baab3));
    g.drawText (juce::String (getNumRows()) + " chords   |   Arrows browse   Tab / Enter insert   Esc close", getLocalBounds().removeFromBottom (24).reduced (10, 0), juce::Justification::centredLeft);
}
void ChordCompletion::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, getNumRows())) return;
    const auto& item = context.matches[static_cast<size_t> (row)];
    if (selected) { g.setColour (accentYellow.withAlpha (0.12f)); g.fillRect (0, 0, width, height); }
    g.setColour (selected ? accentYellow : juce::Colour (0xffe3e8eb));
    g.setFont (uiFont (16.0f, juce::Font::bold));
    g.drawText (item.chord, 12, 3, 132, 25, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xffc2cbd2));
    g.setFont (uiFont (13.0f));
    g.drawText (item.name, 150, 3, width - 162, 25, juce::Justification::centredLeft);
    g.setFont (uiFont (11.0f));
    g.setColour (juce::Colour (0xff91a3ad));
    g.drawText (item.family + "    " + item.degrees, 12, 27, width - 24, 21, juce::Justification::centredLeft);
}
}
