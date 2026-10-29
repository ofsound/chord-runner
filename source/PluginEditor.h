#pragma once
#include "PluginProcessor.h"
#include "ui/NativeEditor.h"

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override { resized(); }
    void chooseProject (int action, std::function<void (juce::String)>);

private:
   #if JUCE_IOS
    void iosProjectAction (int, const juce::File&, std::function<void (juce::String)>);
   #endif
    void confirmDiscardingChanges (std::function<void()>);
    juce::Array<juce::File> projectFiles() const;
    PluginProcessor& processorRef;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<chordrunner::ui::NativeEditor> nativeView;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
