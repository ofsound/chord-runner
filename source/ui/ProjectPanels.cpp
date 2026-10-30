#include "NativeEditor.h"

namespace chordrunner::ui
{
namespace
{
class ProjectDetailsPanel final : public juce::Component
{
public:
    explicit ProjectDetailsPanel (PluginProcessor& processor, std::function<void()> callback)
        : target (processor), complete (std::move (callback))
    {
        const auto state = target.getProjectState();
        for (auto* component : std::initializer_list<juce::Component*> { &nameLabel, &authorLabel, &descriptionLabel, &dates, &name, &author, &description, &done })
            addAndMakeVisible (component);
        nameLabel.setText ("Project name", juce::dontSendNotification);
        authorLabel.setText ("Author", juce::dontSendNotification);
        descriptionLabel.setText ("Description", juce::dontSendNotification);
        name.setText (state["projectName"].toString(), false);
        author.setText (state["projectAuthor"].toString(), false);
        description.setMultiLine (true);
        description.setText (state["projectDescription"].toString(), false);
        name.setInputRestrictions (128);
        author.setInputRestrictions (256);
        description.setInputRestrictions (4096);
        dates.setText ("Created  " + state["createdAt"].toString().substring (0, 10)
            + "    /    Modified  " + state["modifiedAt"].toString().substring (0, 10), juce::dontSendNotification);
        dates.setFont (uiFont (11.0f));
        done.onClick = [this]
        {
            target.setProjectMetadata (name.getText(), author.getText(), description.getText());
            if (complete) complete();
            if (auto* callout = findParentComponentOfClass<juce::CallOutBox>()) callout->dismiss();
        };
        setSize (360, 340);
    }
    void resized() override
    {
        auto area = getLocalBounds().reduced (14);
        nameLabel.setBounds (area.removeFromTop (22));
        name.setBounds (area.removeFromTop (32)); area.removeFromTop (10);
        authorLabel.setBounds (area.removeFromTop (22));
        author.setBounds (area.removeFromTop (32)); area.removeFromTop (10);
        descriptionLabel.setBounds (area.removeFromTop (22));
        done.setBounds (area.removeFromBottom (36)); area.removeFromBottom (6);
        dates.setBounds (area.removeFromBottom (24)); area.removeFromBottom (6);
        description.setBounds (area);
    }
private:
    PluginProcessor& target;
    std::function<void()> complete;
    juce::Label nameLabel, authorLabel, descriptionLabel, dates;
    juce::TextEditor name, author, description;
    juce::TextButton done { "Apply details" };
};
}

void NativeEditor::showProjectDetails()
{
    auto panel = std::make_unique<ProjectDetailsPanel> (controller.processor, [safe = juce::Component::SafePointer<NativeEditor> (this)]
    {
        if (safe != nullptr) safe->controller.refresh();
    });
    panel->setSize (juce::jmin (360, getWidth() - 24), juce::jmin (340, getHeight() - 30));
    juce::CallOutBox::launchAsynchronously (std::move (panel), projectTitle.getScreenBounds(), nullptr);
}

void NativeEditor::showProjectMenu()
{
    juce::PopupMenu menu;
    menu.addItem (4, "New project", true);
    menu.addItem (1, "Open...   Cmd+O");
    menu.addItem (2, "Save   Cmd+S");
    menu.addItem (3, "Save As...   Shift+Cmd+S");
    menu.addSeparator();
    menu.addItem (5, "Previous project");
    menu.addItem (6, "Next project");
    menu.addItem (7, "Project library");
    menu.addSeparator();
    menu.addItem (9, "Undo", controller.processor.canUndo());
    menu.addItem (10, "Redo", controller.processor.canRedo());
    menu.addItem (8, "Project details...");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&projectMenu),
        [safe = juce::Component::SafePointer<NativeEditor> (this)] (int result)
        {
            if (safe == nullptr || result == 0) return;
            if (result == 8) { safe->showProjectDetails(); return; }
            if (result == 9) { safe->controller.undo(); return; }
            if (result == 10) { safe->controller.redo(); return; }
            if ((result == 2 || result == 3) && safe->controller.dirty() && ! safe->controller.applyDraft()) return;
            if (safe->chooseProject) safe->chooseProject (result - 1);
        });
}

}
