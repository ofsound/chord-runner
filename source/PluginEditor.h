#pragma once
#include "PluginProcessor.h"
#include "ui/NativeEditor.h"

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override { resized(); }
    void chooseProject (bool save, std::function<void (juce::String)>);

private:
    PluginProcessor& processorRef;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<chordrunner::ui::NativeEditor> nativeView;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
