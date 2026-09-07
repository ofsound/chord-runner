#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
class PluginProcessor;
class PluginEditor;
namespace WebViewResources
{
std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String&);
juce::WebBrowserComponent::Options makeBrowserOptions (PluginProcessor&, PluginEditor&);
}
