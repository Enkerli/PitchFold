#pragma once

// PitchFoldEditor — AudioProcessorEditor backed by a JUCE 8 WebBrowserComponent.
//
// The React UI is bundled in Source/WebUI/dist/ (index.html + bundle.js).
// C++ → JS : emitEventIfBrowserIsVisible("eventId", juce::var{…})
// JS → C++ : Options::withEventListener("eventId", …)

#include <juce_audio_utils/juce_audio_utils.h>
#include "../PluginProcessor.h"

class PitchFoldEditor : public juce::AudioProcessorEditor,
                        private juce::AudioProcessorValueTreeState::Listener,
                        private juce::Timer
{
public:
    explicit PitchFoldEditor (PitchFoldProcessor& p);
    ~PitchFoldEditor() override;

    void paint   (juce::Graphics&) override {}
    void resized () override;

    void parentHierarchyChanged() override;
    void navigateIfNeeded();

private:
    void parameterChanged (const juce::String& paramID, float newValue) override;
    void timerCallback() override;

    static std::optional<juce::WebBrowserComponent::Resource>
        provideResource (const juce::String& path);

    void sendStateSnapshot();
    static juce::WebBrowserComponent::Options buildOptions (PitchFoldEditor* owner);

    PitchFoldProcessor& proc;
    juce::WebBrowserComponent webView;

    juce::StringArray listenedParams;
    std::atomic<bool> pageReady     { false };
    bool              pageNavigated { false };

    std::unique_ptr<juce::MidiOutput> virtualMidiPort;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchFoldEditor)
};
