#include "PitchFoldEditor.h"
#include <BinaryDataWebUI.h>

// ── Helpers ───────────────────────────────────────────────────────────────────

static juce::var makeObj (std::initializer_list<std::pair<juce::String, juce::var>> pairs)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    for (auto& [k, v] : pairs)
        obj->setProperty (k, v);
    return juce::var (obj.release());
}

// ── Resource provider ─────────────────────────────────────────────────────────

std::optional<juce::WebBrowserComponent::Resource>
PitchFoldEditor::provideResource (const juce::String& path)
{
    struct Entry { const char* data; int size; juce::String mime; };
    static const std::array<std::pair<const char*, Entry>, 3> table {{
        { "/",           { BinaryData::index_html, BinaryData::index_htmlSize, "text/html; charset=utf-8" } },
        { "/index.html", { BinaryData::index_html, BinaryData::index_htmlSize, "text/html; charset=utf-8" } },
        { "/bundle.js",  { BinaryData::bundle_js,  BinaryData::bundle_jsSize,  "application/javascript"   } },
    }};

    for (auto& [key, entry] : table)
    {
        if (path == key)
        {
            std::vector<std::byte> bytes (static_cast<std::size_t> (entry.size));
            std::memcpy (bytes.data(), entry.data, static_cast<std::size_t> (entry.size));
            return juce::WebBrowserComponent::Resource { std::move (bytes), entry.mime };
        }
    }
    return std::nullopt;
}

// ── Options builder ───────────────────────────────────────────────────────────

juce::WebBrowserComponent::Options PitchFoldEditor::buildOptions (PitchFoldEditor* owner)
{
    using juce::WebBrowserComponent;
    using juce::var;
    using juce::Array;

    return WebBrowserComponent::Options{}
        .withResourceProvider ([] (const juce::String& path)
            { return PitchFoldEditor::provideResource (path); },
            WebBrowserComponent::getResourceProviderRoot())
        .withNativeIntegrationEnabled()
       #if JUCE_MAC
        .withKeepPageLoadedWhenBrowserIsHidden()
       #endif

        .withEventListener ("log", [] (const Array<var>& args)
        {
            if (args.isEmpty()) return;
            std::fprintf (stderr, "[js:%s] %s\n",
                          args[0]["level"].toString().toRawUTF8(),
                          args[0]["msg"].toString().toRawUTF8());
        })

        .withEventListener ("uiReady", [owner] (const Array<var>&)
        {
            owner->pageReady = true;
            // SafePointer: queued lambdas can outlive the editor (hosts and
            // pluginval open/close it rapidly while processing).
            juce::Component::SafePointer<PitchFoldEditor> safe (owner);
            juce::MessageManager::callAsync ([safe] { if (safe) safe->sendStateSnapshot(); });
        })

        // "setParam" — normalised 0–1 value
        .withEventListener ("setParam", [owner] (const Array<var>& args)
        {
            if (args.isEmpty()) return;
            const auto& obj = args[0];
            const auto id   = obj["id"].toString();
            const float val = static_cast<float> (static_cast<double> (obj["value"]));
            if (auto* p = owner->proc.apvts.getParameter (id))
                p->setValueNotifyingHost (val);
        })

        // "setParamActual" — actual-domain value (C++ normalises)
        .withEventListener ("setParamActual", [owner] (const Array<var>& args)
        {
            if (args.isEmpty()) return;
            const auto id  = args[0]["id"].toString();
            const float val= static_cast<float> (static_cast<double> (args[0]["value"]));
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                              owner->proc.apvts.getParameter (id)))
                p->setValueNotifyingHost (p->convertTo0to1 (val));
        })

        // "selectPad" — { pad: N }  (-1 = deselect)
        .withEventListener ("selectPad", [owner] (const Array<var>& args)
        {
            if (args.isEmpty()) return;
            owner->proc.selectPad (static_cast<int> (args[0]["pad"]));
        })

        // "setPadData" — { pad, mask, root, label }
        .withEventListener ("setPadData", [owner] (const Array<var>& args)
        {
            if (args.isEmpty()) return;
            const auto& obj = args[0];
            const int pad = static_cast<int> (obj["pad"]);
            if (obj.hasProperty ("mask"))
                owner->proc.setPadMask (pad, static_cast<uint16_t> (static_cast<int> (obj["mask"])));
            if (obj.hasProperty ("root"))
                owner->proc.setPadRoot (pad, static_cast<int> (obj["root"]));
            if (obj.hasProperty ("label"))
                owner->proc.setPadLabel (pad, obj["label"].toString());
            // Echo change back to UI
            juce::Component::SafePointer<PitchFoldEditor> safe (owner);
            juce::MessageManager::callAsync ([safe] { if (safe) safe->sendStateSnapshot(); });
        })

        // "panic"
        .withEventListener ("panic", [owner] (const Array<var>&)
        {
            owner->proc.sendPanic();
        });
}

// ── Constructor / Destructor ──────────────────────────────────────────────────

PitchFoldEditor::PitchFoldEditor (PitchFoldProcessor& p)
    : AudioProcessorEditor (p),
      proc (p),
      webView (buildOptions (this))
{
    addAndMakeVisible (webView);
    setSize (900, 650);
    setResizable (true, false);

    const auto& params = proc.apvts.processor.getParameters();
    for (auto* param : params)
        if (auto* pwid = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
        {
            proc.apvts.addParameterListener (pwid->paramID, this);
            listenedParams.add (pwid->paramID);
        }

    if (juce::JUCEApplicationBase::isStandaloneApp())
    {
        virtualMidiPort = juce::MidiOutput::createNewDevice ("PitchFold");
        if (virtualMidiPort)
            proc.setVirtualMidiOutput (virtualMidiPort.get());
    }

    juce::Component::SafePointer<PitchFoldEditor> safe (this);
    juce::MessageManager::callAsync ([safe] { if (safe) safe->navigateIfNeeded(); });
    startTimer (33);   // 30 Hz
}

PitchFoldEditor::~PitchFoldEditor()
{
    stopTimer();
    for (const auto& id : listenedParams)
        proc.apvts.removeParameterListener (id, this);

    proc.setVirtualMidiOutput (nullptr);
    proc.setDirectMidiOutput  (nullptr);
}

// ── Navigation ────────────────────────────────────────────────────────────────

void PitchFoldEditor::navigateIfNeeded()
{
    if (pageNavigated) return;
    pageNavigated = true;
    webView.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
}

void PitchFoldEditor::parentHierarchyChanged()
{
    AudioProcessorEditor::parentHierarchyChanged();
    if (getParentComponent() != nullptr)
        navigateIfNeeded();
}

// ── Layout ────────────────────────────────────────────────────────────────────

void PitchFoldEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

// ── Timer ─────────────────────────────────────────────────────────────────────

void PitchFoldEditor::timerCallback()
{
    // Nothing to push at 30 Hz in this version — future: active-voice display
}

// ── Parameter listener ────────────────────────────────────────────────────────

void PitchFoldEditor::parameterChanged (const juce::String& paramID, float newValue)
{
    if (!pageReady) return;
    // Fires from any thread (host automation, audio thread); the editor may
    // be gone by the time the message thread runs this — SafePointer guards.
    juce::Component::SafePointer<PitchFoldEditor> safe (this);
    juce::MessageManager::callAsync ([safe, paramID, newValue]
    {
        if (safe == nullptr || !safe->pageReady) return;
        safe->webView.emitEventIfBrowserIsVisible ("paramChange",
            makeObj ({ { "id", paramID }, { "value", newValue } }));
    });
}

// ── State snapshot ────────────────────────────────────────────────────────────

void PitchFoldEditor::sendStateSnapshot()
{
    const auto raw = [&] (const juce::String& id) -> float
    {
        if (auto* p = proc.apvts.getRawParameterValue (id)) return p->load();
        return 0.0f;
    };

    // Pads array
    juce::var padsArr (juce::Array<juce::var>{});
    auto& padsRef = *padsArr.getArray();
    for (int i = 0; i < pf::kNumPads; ++i)
    {
        const auto& pad = proc.padBank().pad(i);
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty ("index",    i);
        obj->setProperty ("mask",     (int) pad.mask);
        obj->setProperty ("root",     pad.root);
        obj->setProperty ("label",    juce::String::fromUTF8 (pad.label));
        obj->setProperty ("selected", proc.selectedPad() == i);
        padsRef.add (juce::var (obj.release()));
    }

    const auto snap = makeObj ({
        { "pcsRoot",       (int)   raw (ParamID::pcsRoot)       },
        { "pcsMask",       (int)   raw (ParamID::pcsMask)        },
        { "quantDir",      (int)   raw (ParamID::quantDir)       },
        { "quantStrength",         raw (ParamID::quantStrength)  },
        { "outputLo",      (int)   raw (ParamID::outputLo)       },
        { "outputHi",      (int)   raw (ParamID::outputHi)       },
        { "useFlats",      raw (ParamID::useFlats) > 0.5f        },
        { "timeGrid",      (int)   raw (ParamID::timeGrid)       },
        { "timeStrength",          raw (ParamID::timeStrength)   },
        { "humanizeTime",          raw (ParamID::humanizeTime)   },
        { "humanizeVel",           raw (ParamID::humanizeVel)    },
        { "swing",                 raw (ParamID::swing)          },
        { "lookAheadMs",           raw (ParamID::lookAheadMs)    },
        { "voiceMode",     (int)   raw (ParamID::voiceMode)      },
        { "monoSelect",    (int)   raw (ParamID::monoSelect)     },
        { "splitVoices",   (int)   raw (ParamID::splitVoices)    },
        { "splitChannel",  (int)   raw (ParamID::splitChannel)   },
        { "pads",          padsArr                                },
    });

    webView.emitEventIfBrowserIsVisible ("stateSnapshot", snap);
}
