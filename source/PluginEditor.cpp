#include "PluginEditor.h"
#include "WebViewResourceProvider.h"

namespace
{
class ChordRunnerBrowser final : public juce::WebBrowserComponent
{
public:
    explicit ChordRunnerBrowser (const Options& options) : WebBrowserComponent (options) {}
    bool pageAboutToLoad (const juce::String& url) override
    {
        return url.startsWith (getResourceProviderRoot()) || url == "about:blank"
#if JUCE_DEBUG && CHORD_RUNNER_UI_DEV_SERVER
            || url.startsWith ("http://localhost:5175/")
#endif
            ;
    }
};
}

PluginEditor::PluginEditor (PluginProcessor& p) : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<ChordRunnerBrowser> (WebViewResources::makeBrowserOptions (processorRef, *this));
    addAndMakeVisible (*webView);
    setResizable (true, true);
    setResizeLimits (760, 540, 2560, 1600);
    setSize (1060, 780);
#if JUCE_DEBUG && CHORD_RUNNER_UI_DEV_SERVER
    webView->goToURL ("http://localhost:5175/");
#else
    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
#endif
}

void PluginEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff09090b)); }
void PluginEditor::resized() { if (webView != nullptr) webView->setBounds (getLocalBounds()); }

void PluginEditor::chooseProject (bool save, juce::WebBrowserComponent::NativeFunctionCompletion complete)
{
    if (chooser != nullptr)
    {
        complete ("A file dialog is already open.");
        return;
    }
    const auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    chooser = std::make_unique<juce::FileChooser> (save ? "Save Chord Runner project" : "Open Chord Runner project",
        folder.getChildFile ("Untitled.chordrunner"), "*.chordrunner");
    const int flags = juce::FileBrowserComponent::canSelectFiles
        | (save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                : juce::FileBrowserComponent::openMode);
    chooser->launchAsync (flags, [safe = juce::Component::SafePointer<PluginEditor> (this), save, completion = std::move (complete)] (const juce::FileChooser& dialog)
    {
        if (safe == nullptr)
            return;
        auto file = dialog.getResult();
        juce::String error;
        if (file != juce::File {})
        {
            if (save)
            {
                file = file.withFileExtension ("chordrunner");
                juce::MemoryBlock data;
                safe->processorRef.getStateInformation (data);
                juce::TemporaryFile temporary (file);
                if (! temporary.getFile().replaceWithData (data.getData(), data.getSize())
                    || ! temporary.overwriteTargetFileWithTemporary())
                    error = "Could not save the project.";
            }
            else if (file.getSize() <= 0 || file.getSize() > 65536)
            {
                error = "The file is empty or too large to be a Chord Runner project.";
            }
            else
            {
                const auto result = safe->processorRef.setTimelineState (juce::JSON::parse (file.loadFileAsString()));
                error = result.getErrorMessage();
                if (result.wasOk()) safe->processorRef.setPlaying (false);
            }
        }
        completion (error);
        safe->chooser.reset();
    });
}
