#if JucePlugin_Build_Standalone

#include <unistd.h>
#include "PluginProcessor.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

// CVDisplayLink / mutex crash guard (same pattern as DrawnQurve).
static void installDisplayLinkCrashGuard()
{
    static auto prev = std::get_terminate();
    std::set_terminate ([]
    {
        try { if (auto e = std::current_exception()) std::rethrow_exception (e); }
        catch (const std::system_error& e)
        {
            if (e.code().value() == EINVAL) _exit (0);
        }
        catch (...) {}
        if (prev) prev();
        std::abort();
    });
}

//==============================================================================
class PitchFoldApp final : public juce::JUCEApplication
{
public:
    PitchFoldApp()
    {
        juce::PropertiesFile::Options opts;
        opts.applicationName     = juce::CharPointer_UTF8 (JucePlugin_Name);
        opts.filenameSuffix      = ".settings";
        opts.osxLibrarySubFolder = "Application Support";
        appProperties.setStorageParameters (opts);
    }

    const juce::String getApplicationName()    override { return juce::CharPointer_UTF8 (JucePlugin_Name); }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed()          override { return true; }
    void anotherInstanceStarted (const juce::String&) override {}

    void initialise (const juce::String&) override
    {
        installDisplayLinkCrashGuard();

        mainWindow.reset (new juce::StandaloneFilterWindow (
            getApplicationName(),
            juce::LookAndFeel::getDefaultLookAndFeel()
                .findColour (juce::ResizableWindow::backgroundColourId),
            appProperties.getUserSettings(), false));

        mainWindow->setVisible (true);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow) mainWindow->pluginHolder->savePluginState();
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
            juce::Timer::callAfterDelay (100, [] { juce::JUCEApplicationBase::getInstance()->systemRequestedQuit(); });
        else
            quit();
    }

private:
    juce::ApplicationProperties                   appProperties;
    std::unique_ptr<juce::StandaloneFilterWindow> mainWindow;
};

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new PitchFoldApp(); }

#endif
