#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;
    void chooseProject (bool save, juce::WebBrowserComponent::NativeFunctionCompletion);

private:
    PluginProcessor& processorRef;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::WebBrowserComponent> webView;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
