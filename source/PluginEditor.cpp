#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p) : AudioProcessorEditor (&p), processorRef (p)
{
    nativeView = std::make_unique<chordrunner::ui::NativeEditor> (p);
    addAndMakeVisible (*nativeView);
    nativeView->chooseProject = [this] (int action)
    {
        chooseProject (action, [safe = juce::Component::SafePointer<PluginEditor> (this)] (juce::String error)
        {
            if (safe == nullptr)
                return;
            safe->nativeView->controller.error = error;
            safe->nativeView->controller.refresh();
        });
    };
    setResizable (true, true);
   #if JUCE_IOS
    setResizeLimits (280, 240, 2560, 2560);
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        setSize (juce::jmin (1120, juce::roundToInt (display->userBounds.getWidth())), juce::jmin (900, juce::roundToInt (display->userBounds.getHeight())));
    else
        setSize (390, 700);
   #else
    setResizeLimits (760, 540, 2560, 1600);
    setSize (1120, 800);
   #endif
}

PluginEditor::~PluginEditor()
{
   #if JUCE_IOS
    processorRef.getIOSProjectLibrary()->cancel();
   #endif
}

void PluginEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff101114)); }
void PluginEditor::resized()
{
    auto bounds = getLocalBounds();
   #if JUCE_IOS
    if (processorRef.isStandalone())
        if (const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
        {
            const auto safeScreen = display->safeAreaInsets.subtractedFrom (display->userBounds.toNearestInt());
            bounds = bounds.getIntersection (getLocalArea (nullptr, safeScreen));
        }
   #endif
    if (nativeView != nullptr)
        nativeView->setBounds (bounds);
}


juce::Array<juce::File> PluginEditor::projectFiles() const
{
    const auto current = processorRef.getProjectFile();
   #if JUCE_IOS
    const auto library = processorRef.getIOSProjectLibrary();
    if (current == juce::File {} || current.getParentDirectory() == library->directory())
        return library->files();
   #endif
    const auto folder = current.existsAsFile() ? current.getParentDirectory()
        : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("ofsound/Chord Runner");
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.chordrunner");
    std::sort (files.begin(), files.end(), [] (const auto& a, const auto& b)
    {
        return a.getFileName().compareNatural (b.getFileName()) < 0;
    });
    return files;
}

void PluginEditor::confirmDiscardingChanges (std::function<void()> proceed)
{
    if (! processorRef.isDocumentModified() && ! nativeView->controller.dirty())
    {
        proceed();
        return;
    }
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
        .withIconType (juce::MessageBoxIconType::QuestionIcon)
        .withTitle ("Save project changes?")
        .withMessage ("Save your changes before replacing this project?")
        .withButton ("Save").withButton ("Discard").withButton ("Cancel")
        .withAssociatedComponent (this),
        [safe = juce::Component::SafePointer<PluginEditor> (this), continuation = std::move (proceed)] (int result)
        {
            if (safe == nullptr) return;
            if (result == 2) continuation();
            else if (result == 1 && (! safe->nativeView->controller.dirty() || safe->nativeView->controller.applyDraft()))
                safe->chooseProject (1, [safe, continuation] (juce::String error)
                {
                    if (safe == nullptr) return;
                    safe->nativeView->controller.error = error;
                    safe->nativeView->controller.refresh();
                    if (error.isEmpty() && ! safe->processorRef.isDocumentModified()) continuation();
                });
        });
}

void PluginEditor::chooseProject (int action, std::function<void (juce::String)> complete)
{
    if (chooser != nullptr)
    {
        complete ("A project dialog is already open.");
        return;
    }
    if (action == 3)
    {
        confirmDiscardingChanges ([safe = juce::Component::SafePointer<PluginEditor> (this), complete]
        {
            if (safe == nullptr) return;
            safe->processorRef.newProject();
            safe->nativeView->controller.refresh (true);
            complete ({});
        });
        return;
    }
    if (action >= 4)
    {
        const auto files = projectFiles();
        auto load = [safe = juce::Component::SafePointer<PluginEditor> (this), complete] (juce::File file)
        {
            if (safe == nullptr) return;
           #if JUCE_IOS
            safe->iosProjectAction (0, file, complete);
           #else
            safe->confirmDiscardingChanges ([safe, file, complete]
            {
                if (safe == nullptr) return;
                const auto result = safe->processorRef.loadProject (file);
                if (result.wasOk()) safe->nativeView->controller.refresh (true);
                complete (result.getErrorMessage());
            });
           #endif
        };
        if (files.isEmpty()) { complete ("No projects in this library yet. Use Save As to add one."); return; }
        if (action == 6)
        {
            juce::PopupMenu menu;
            for (int i = 0; i < files.size(); ++i)
                menu.addItem (i + 1, files[i].getFileNameWithoutExtension(), true, files[i] == processorRef.getProjectFile());
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (nativeView.get()), [files, load] (int result)
            {
                if (juce::isPositiveAndBelow (result - 1, files.size())) load (files[result - 1]);
            });
        }
        else
        {
            const int current = files.indexOf (processorRef.getProjectFile());
            const int index = current < 0 ? 0 : (current + (action == 4 ? files.size() - 1 : 1)) % files.size();
            load (files[index]);
        }
        return;
    }
   #if JUCE_IOS
    iosProjectAction (action, {}, std::move (complete));
    return;
   #else
    const bool save = action == 1 || action == 2;
    if (action == 1 && processorRef.getProjectFile().existsAsFile())
    {
        complete (processorRef.saveProject (processorRef.getProjectFile()).getErrorMessage());
        return;
    }
    const auto current = processorRef.getProjectFile();
    const auto folder = current.existsAsFile() ? current.getParentDirectory()
        : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("ofsound/Chord Runner");
    const auto name = juce::File::createLegalFileName (processorRef.getProjectState()["projectName"].toString());
    chooser = std::make_unique<juce::FileChooser> (save ? "Save Chord Runner project" : "Open Chord Runner project",
        current.existsAsFile() ? current : folder.getChildFile (name + ".chordrunner"), "*.chordrunner");
    const int flags = juce::FileBrowserComponent::canSelectFiles
        | (save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                : juce::FileBrowserComponent::openMode);
    chooser->launchAsync (flags, [safe = juce::Component::SafePointer<PluginEditor> (this), save, completion = std::move (complete)] (const juce::FileChooser& dialog)
    {
        if (safe == nullptr) return;
        const auto file = dialog.getResult();
        safe->chooser.reset();
        if (file == juce::File {}) { completion ({}); return; }
        if (save)
            completion (safe->processorRef.saveProject (file.withFileExtension ("chordrunner")).getErrorMessage());
        else
            safe->confirmDiscardingChanges ([safe, file, completion]
            {
                if (safe == nullptr) return;
                const auto result = safe->processorRef.loadProject (file);
                if (result.wasOk()) safe->nativeView->controller.refresh (true);
                completion (result.getErrorMessage());
            });
    });
   #endif
}

#if JUCE_IOS
void PluginEditor::iosProjectAction (int action, const juce::File& file, std::function<void (juce::String)> complete)
{
    const auto library = processorRef.getIOSProjectLibrary();
    if (library->busy()) { complete ("A project operation is already in progress."); return; }
    const auto before = juce::JSON::toString (processorRef.getProjectState(), true);
    const auto safe = juce::Component::SafePointer<PluginEditor> (this);
    if (action == 0)
    {
        auto loaded = [safe, before, completion = std::move (complete)] (chordrunner::IOSProjectLibrary::Result result)
        {
            if (safe == nullptr) return;
            if (result.cancelled || result.error.isNotEmpty()) { completion (result.error); return; }
            if (juce::JSON::toString (safe->processorRef.getProjectState(), true) != before)
            {
                completion ("This project changed while opening the file. Open it again to continue.");
                return;
            }
            safe->confirmDiscardingChanges ([safe, loadedResult = std::move (result), completion]
            {
                if (safe == nullptr) return;
                const auto document = juce::JSON::parse (juce::String::fromUTF8 (
                    static_cast<const char*> (loadedResult.data.getData()), static_cast<int> (loadedResult.data.getSize())));
                const auto restored = safe->processorRef.setProjectState (document);
                if (restored.wasOk())
                {
                    safe->processorRef.getIOSProjectLibrary()->acceptFile (loadedResult.file);
                    safe->processorRef.setProjectFile (loadedResult.file);
                    safe->nativeView->controller.refresh (true);
                }
                completion (restored.getErrorMessage());
            });
        };
        if (file == juce::File {}) library->chooseOpen (*this, std::move (loaded));
        else library->read (file, std::move (loaded));
        return;
    }
    auto document = processorRef.getProjectState();
    const auto now = juce::Time::getCurrentTime().toISO8601 (true);
    document.getDynamicObject()->setProperty ("modifiedAt", now);
    if (document["createdAt"].toString().isEmpty()) document.getDynamicObject()->setProperty ("createdAt", now);
    const auto json = juce::JSON::toString (document);
    juce::MemoryBlock data (json.toRawUTF8(), json.getNumBytesAsUTF8());
    library->save (*this, processorRef.getProjectFile(), document["projectName"].toString(), std::move (data), action == 2,
        [safe, before, document, completion = std::move (complete)] (chordrunner::IOSProjectLibrary::Result result)
        {
            if (safe == nullptr) return;
            if (! result.cancelled && result.error.isEmpty())
            {
                safe->processorRef.getIOSProjectLibrary()->acceptFile (result.file);
                safe->processorRef.setProjectFile (result.file);
                if (juce::JSON::toString (safe->processorRef.getProjectState(), true) == before)
                    safe->processorRef.completeProjectSave (result.file, document);
            }
            completion (result.error);
        });
}
#endif
