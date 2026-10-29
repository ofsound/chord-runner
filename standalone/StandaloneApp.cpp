#include <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>
#include <juce_audio_plugin_client/detail/juce_IncludeSystemHeaders.h>
#include <juce_audio_plugin_client/detail/juce_IncludeModuleHeaders.h>
#include <juce_audio_plugin_client/detail/juce_PluginUtilities.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#undef Component
#if JUCE_MAC
 #undef Point
#endif

#include "../source/PluginProcessor.h"

namespace juce
{
namespace
{
constexpr auto virtualMidiOutputName = "Chord Runner Output";
}

class ChordRunnerStandalonePluginHolder final : public StandalonePluginHolder,
                                                private ChangeListener
{
public:
    ChordRunnerStandalonePluginHolder (PropertySet* settingsToUse,
                                       bool takeOwnershipOfSettings,
                                       const String& preferredDefaultDeviceName,
                                       const AudioDeviceManager::AudioDeviceSetup* preferredSetupOptions,
                                       const Array<PluginInOuts>& channels,
                                       bool shouldAutoOpenMidiDevices)
        : StandalonePluginHolder (settingsToUse,
                                  takeOwnershipOfSettings,
                                  preferredDefaultDeviceName,
                                  preferredSetupOptions,
                                  channels,
                                  shouldAutoOpenMidiDevices)
    {
        if (settingsToUse != nullptr)
            if (auto* plugin = dynamic_cast<PluginProcessor*> (processor.get()))
                plugin->restoreStandaloneSettings (*settingsToUse);
        createVirtualMidiOutput();
        useVirtualMidiOutputIfAvailable();
        deviceManager.addChangeListener (this);
    }

    void saveDocumentAccess()
    {
        if (settings != nullptr)
            if (auto* plugin = dynamic_cast<PluginProcessor*> (processor.get()))
                plugin->saveStandaloneSettings (*settings);
    }

    void flushMidiBeforeShutdown()
    {
        // Detach the audio processor under AudioProcessorPlayer's callback lock
        // before touching the MIDI endpoint. Never call processBlock from the UI.
        saveDocumentAccess();
        stopPlaying();
        if (auto* plugin = dynamic_cast<PluginProcessor*> (processor.get()))
            plugin->panic();
        if (virtualMidiOutput != nullptr)
        {
            virtualMidiOutput->stopBackgroundThread();
            virtualMidiOutput->clearAllPendingMessages();
            for (int channel = 1; channel <= 16; ++channel)
            {
                virtualMidiOutput->sendMessageNow (MidiMessage::controllerEvent (channel, 64, 0));
                virtualMidiOutput->sendMessageNow (MidiMessage::allNotesOff (channel));
                virtualMidiOutput->sendMessageNow (MidiMessage::allSoundOff (channel));
            }
        }
    }

    void deletePlugin() override
    {
        flushMidiBeforeShutdown();
        StandalonePluginHolder::deletePlugin();
        if (virtualMidiOutput != nullptr)
            virtualMidiOutput->startBackgroundThread();
    }

    ~ChordRunnerStandalonePluginHolder() override
    {
        flushMidiBeforeShutdown();
        deviceManager.removeChangeListener (this);
        player.setMidiOutput (nullptr);

        if (virtualMidiOutput != nullptr)
            virtualMidiOutput->stopBackgroundThread();
    }

private:
    void changeListenerCallback (ChangeBroadcaster*) override
    {
        useVirtualMidiOutputIfAvailable();
    }

    void createVirtualMidiOutput()
    {
       #if JUCE_MAC || JUCE_IOS || JUCE_LINUX || JUCE_BSD
        virtualMidiOutput = MidiOutput::createNewDevice (virtualMidiOutputName);
       #endif

        if (virtualMidiOutput != nullptr)
            virtualMidiOutput->startBackgroundThread();
    }

    void useVirtualMidiOutputIfAvailable()
    {
        if (virtualMidiOutput != nullptr)
            player.setMidiOutput (virtualMidiOutput.get());
    }

    std::unique_ptr<MidiOutput> virtualMidiOutput;
};

void flushMidiBeforeShutdown (StandalonePluginHolder* holder)
{
    if (auto* runnerHolder = dynamic_cast<ChordRunnerStandalonePluginHolder*> (holder))
        runnerHolder->flushMidiBeforeShutdown();
}

class ChordRunnerStandaloneApp final : public JUCEApplication
{
public:
    ChordRunnerStandaloneApp()
    {
        PropertiesFile::Options options;

        options.applicationName = CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName = "~/.config";
       #else
        options.folderName = "";
       #endif

        appProperties.setStorageParameters (options);
    }

    const String getApplicationName() override
    {
        return CharPointer_UTF8 (JucePlugin_Name);
    }

    const String getApplicationVersion() override
    {
        return JucePlugin_VersionString;
    }

    bool moreThanOneInstanceAllowed() override
    {
        return false;
    }

    void anotherInstanceStarted (const String&) override {}

    StandaloneFilterWindow* createWindow()
    {
        if (Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            jassertfalse;
            return nullptr;
        }

        return new StandaloneFilterWindow (
            getApplicationName(),
            LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
            createPluginHolder());
    }

    std::unique_ptr<StandalonePluginHolder> createPluginHolder()
    {
        constexpr auto autoOpenMidiDevices =
       #if (JUCE_ANDROID || JUCE_IOS) && ! JUCE_DONT_AUTO_OPEN_MIDI_DEVICES_ON_MOBILE
                true;
       #else
                false;
       #endif

       #ifdef JucePlugin_PreferredChannelConfigurations
        constexpr StandalonePluginHolder::PluginInOuts channels[] { JucePlugin_PreferredChannelConfigurations };
        const Array<StandalonePluginHolder::PluginInOuts> channelConfig (channels, numElementsInArray (channels));
       #else
        const Array<StandalonePluginHolder::PluginInOuts> channelConfig;
       #endif

        return std::make_unique<ChordRunnerStandalonePluginHolder> (appProperties.getUserSettings(),
                                                                    false,
                                                                    String {},
                                                                    nullptr,
                                                                    channelConfig,
                                                                    autoOpenMidiDevices);
    }

    void initialise (const String&) override
    {
        mainWindow = rawToUniquePtr (createWindow());

        if (mainWindow != nullptr)
        {
           #if JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE
            Desktop::getInstance().setKioskModeComponent (mainWindow.get(), false);
           #endif

            mainWindow->setVisible (true);
        }
        else
        {
            pluginHolder = createPluginHolder();
        }
    }

    void shutdown() override
    {
        pluginHolder = nullptr;
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

   #if JUCE_IOS
    void suspended() override
    {
        // The OS may terminate a backgrounded app without calling shutdown().
        auto* holder = mainWindow != nullptr ? mainWindow->pluginHolder.get() : pluginHolder.get();
        if (holder != nullptr)
        {
            if (auto* runner = dynamic_cast<ChordRunnerStandalonePluginHolder*> (holder)) runner->saveDocumentAccess();
            holder->savePluginState();
        }
        appProperties.saveIfNeeded();
    }
   #endif

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            flushMidiBeforeShutdown (mainWindow->pluginHolder.get());
        else if (pluginHolder != nullptr)
            flushMidiBeforeShutdown (pluginHolder.get());

        if (pluginHolder != nullptr)
            pluginHolder->savePluginState();

        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            Timer::callAfterDelay (100, [] {
                if (auto* app = JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

protected:
    ApplicationProperties appProperties;
    std::unique_ptr<StandaloneFilterWindow> mainWindow;

private:
    std::unique_ptr<StandalonePluginHolder> pluginHolder;
};
} // namespace juce

START_JUCE_APPLICATION (juce::ChordRunnerStandaloneApp)
