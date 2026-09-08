
#include <juce_core/juce_core.h>
#include <iostream>

int main() {
    juce::String s1 = juce::String::charToString ((juce::juce_wchar) 0x266f);
    juce::String s2 = juce::String::charToString ((juce::juce_wchar) 0x266d);
    std::cout << "s1 len=" << s1.length() << " utf8 len=" << s1.getNumBytesAsUTF8() << "\n";
    std::cout << "s2 len=" << s2.length() << " utf8 len=" << s2.getNumBytesAsUTF8() << "\n";
    juce::String input = juce::String::fromUTF8 ("C\xe2\x99\xafm B\xe2\x99\xad");
    std::cout << "input: " << input << "\n";
    juce::String rep1 = input.replace (s1, "#");
    std::cout << "after rep1: " << rep1 << "\n";
    juce::String rep2 = rep1.replace (s2, "b");
    std::cout << "after rep2: " << rep2 << "\n";
    return 0;
}
