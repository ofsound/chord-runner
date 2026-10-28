#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p) : AudioProcessorEditor (&p), processorRef (p)
{
    nativeView = std::make_unique<chordrunner::ui::NativeEditor> (p);
    addAndMakeVisible (*nativeView);
    nativeView->chooseProject = [this] (bool save)
    {
        chooseProject (save, [safe = juce::Component::SafePointer<PluginEditor> (this)] (juce::String error)
        {
            if (safe == nullptr)
                return;
            safe->nativeView->controller.error = error;
            safe->nativeView->controller.refresh();
        });
    };
    setResizable (true, true);
    setResizeLimits (760, 540, 2560, 1600);
    setSize (1120, 900);
}

void PluginEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff101114)); }
void PluginEditor::resized() { if (nativeView != nullptr) nativeView->setBounds (getLocalBounds()); }

void PluginEditor::chooseProject (bool save, std::function<void (juce::String)> complete)
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
                if (result.wasOk())
                {
                    safe->processorRef.setPlaying (false);
                    safe->nativeView->controller.refresh (true);
                }
            }
        }
        completion (error);
        safe->chooser.reset();
    });
}
