#include "WebViewResourceProvider.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#include <cstring>

std::optional<juce::WebBrowserComponent::Resource> WebViewResources::getResource (const juce::String& url)
{
    auto input = std::make_unique<juce::MemoryInputStream> (BinaryData::ui_zip, BinaryData::ui_zipSize, false);
    juce::ZipFile zip (std::move (input));
    const auto path = url == "/" ? juce::String ("index.html") : url.trimCharactersAtStart ("/");
    const auto index = zip.getIndexOfFileName (path);
    if (index < 0)
        return std::nullopt;
    std::unique_ptr<juce::InputStream> stream (zip.createStreamForEntry (index));
    if (stream == nullptr)
        return std::nullopt;
    juce::MemoryBlock bytes;
    stream->readIntoMemoryBlock (bytes);
    juce::WebBrowserComponent::Resource resource;
    resource.mimeType = path.endsWith (".html") ? "text/html"
                      : path.endsWith (".js") ? "text/javascript"
                      : path.endsWith (".css") ? "text/css"
                      : path.endsWith (".svg") ? "image/svg+xml"
                      : "application/octet-stream";
    resource.data.resize (bytes.getSize());
    std::memcpy (resource.data.data(), bytes.getData(), bytes.getSize());
    return resource;
}

juce::WebBrowserComponent::Options WebViewResources::makeBrowserOptions (PluginProcessor& processor, PluginEditor& editor)
{
    using Browser = juce::WebBrowserComponent;
    auto options = Browser::Options {}
        .withNativeIntegrationEnabled()
        .withInitialisationData ("pluginName", "Chord Runner")
        .withNativeFunction ("getState", [&processor] (const juce::Array<juce::var>&, Browser::NativeFunctionCompletion complete)
        {
            complete (processor.getTimelineState());
        })
        .withNativeFunction ("getTransport", [&processor] (const juce::Array<juce::var>&, Browser::NativeFunctionCompletion complete)
        {
            complete (processor.getTransportState());
        })
        .withNativeFunction ("setState", [&processor] (const juce::Array<juce::var>& args, Browser::NativeFunctionCompletion complete)
        {
            const auto result = args.size() == 1 ? processor.setTimelineState (args[0]) : juce::Result::fail ("Missing progression.");
            complete (result.getErrorMessage());
        })
        .withNativeFunction ("setPlaying", [&processor] (const juce::Array<juce::var>& args, Browser::NativeFunctionCompletion complete)
        {
            if (args.size() == 1 && args[0].isBool())
                processor.setPlaying (static_cast<bool> (args[0]));
            complete (processor.getTransportState());
        })
        .withNativeFunction ("panic", [&processor] (const juce::Array<juce::var>&, Browser::NativeFunctionCompletion complete)
        {
            processor.panic();
            complete (true);
        })
        .withNativeFunction ("saveProject", [&editor] (const juce::Array<juce::var>&, Browser::NativeFunctionCompletion complete)
        {
            editor.chooseProject (true, std::move (complete));
        })
        .withNativeFunction ("loadProject", [&editor] (const juce::Array<juce::var>&, Browser::NativeFunctionCompletion complete)
        {
            editor.chooseProject (false, std::move (complete));
        });
#if JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE
    options = options.withResourceProvider (getResource, juce::URL ("http://localhost:5175").getOrigin());
#endif
#if JUCE_WINDOWS
    options = options.withBackend (Browser::Options::Backend::webview2)
        .withWinWebView2Options (Browser::Options::WinWebView2().withUserDataFolder (
            juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ChordRunner-WebView")));
#endif
    return options;
}
