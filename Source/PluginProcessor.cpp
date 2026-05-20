#include "PluginProcessor.h"
#include "WebUI/PitchFoldEditor.h"
#include "PCS/ScaleData.h"

using namespace pf;

// ── Parameter layout ──────────────────────────────────────────────────────────

juce::AudioProcessorValueTreeState::ParameterLayout PitchFoldProcessor::createParams()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // PCS
    layout.add (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { ParamID::pcsRoot, 1 }, "Root", 0, 11, 0));

    layout.add (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { ParamID::pcsMask, 1 }, "PCS Mask", 0, 4095, 0x0AD5));

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamID::quantDir, 1 }, "Snap Direction",
        juce::StringArray { "Nearest", "Up", "Down" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::quantStrength, 1 }, "Snap Strength",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { ParamID::outputLo, 1 }, "Output Low",  0, 127, 0));

    layout.add (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { ParamID::outputHi, 1 }, "Output High", 0, 127, 127));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamID::useFlats, 1 }, "Use Flats", false));

    // Time
    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamID::timeGrid, 1 }, "Time Grid",
        juce::StringArray { "Off", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::timeStrength, 1 }, "Grid Strength",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::humanizeTime, 1 }, "Humanize Time (ms)",
        juce::NormalisableRange<float> (0.0f, 200.0f, 0.5f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::humanizeVel, 1 }, "Humanize Velocity",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::swing, 1 }, "Swing",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    // Look-ahead / delay
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::lookAheadMs, 1 }, "Look-ahead (ms)",
        juce::NormalisableRange<float> (0.0f, 500.0f, 1.0f), 0.0f));

    // Voice
    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamID::voiceMode, 1 }, "Voice Mode",
        juce::StringArray { "Through", "Mono Merge", "Poly Spread", "Voice Split", "Chordize" }, 0));

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamID::monoSelect, 1 }, "Mono Select",
        juce::StringArray { "Last", "Lowest", "Highest", "First" }, 0));

    layout.add (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { ParamID::splitVoices, 1 }, "Split Voices", 1, 4, 2));

    layout.add (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { ParamID::splitChannel, 1 }, "Split Base Channel", 1, 13, 1));

    // Chord pads
    for (int i = 0; i < pf::kNumPads; ++i)
    {
        layout.add (std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { ParamID::padMask (i), 1 },
            "Pad " + juce::String (i + 1) + " Mask", 0, 4095,
            pf::ChordPadBank{}.pad(i).mask));

        layout.add (std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { ParamID::padRoot (i), 1 },
            "Pad " + juce::String (i + 1) + " Root", 0, 11, 0));
    }

    return layout;
}

// ── Constructor ───────────────────────────────────────────────────────────────

PitchFoldProcessor::PitchFoldProcessor()
    : AudioProcessor (BusesProperties()),
      apvts (*this, nullptr, "PitchFold", createParams())
{
}

PitchFoldProcessor::~PitchFoldProcessor() = default;

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void PitchFoldProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    _sampleRate = sampleRate;

    const float delayMs = apvts.getRawParameterValue (ParamID::lookAheadMs)->load();
    _delay.prepare (sampleRate, static_cast<double> (delayMs));

    _timeQ.prepare (sampleRate, 120.0);
    _voices.reset();
}

void PitchFoldProcessor::releaseResources()
{
    _delay.clear();
    _voices.reset();
}

// ── processBlock ──────────────────────────────────────────────────────────────

void PitchFoldProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer& midi)
{
    // ── Panic sweep ───────────────────────────────────────────────────────────
    if (_panicNeeded.exchange (false))
    {
        midi.clear();
        for (int ch = 1; ch <= 16; ++ch)
            midi.addEvent (juce::MidiMessage::allNotesOff (ch), 0);
        _voices.reset();
        return;
    }

    // ── Read parameters ───────────────────────────────────────────────────────
    const int root      = static_cast<int> (apvts.getRawParameterValue (ParamID::pcsRoot)->load());
    const int maskRaw   = static_cast<int> (apvts.getRawParameterValue (ParamID::pcsMask)->load());
    const auto mask     = static_cast<uint16_t> (maskRaw);
    const int dirIdx    = static_cast<int> (apvts.getRawParameterValue (ParamID::quantDir)->load());
    const float strength= apvts.getRawParameterValue (ParamID::quantStrength)->load();
    const int loNote    = static_cast<int> (apvts.getRawParameterValue (ParamID::outputLo)->load());
    const int hiNote    = static_cast<int> (apvts.getRawParameterValue (ParamID::outputHi)->load());
    const float delayMs = apvts.getRawParameterValue (ParamID::lookAheadMs)->load();

    // Resolve pad override
    const uint16_t activeMask = _pads.activeMask (mask);
    const int      activeRoot = _pads.activeRoot (root);

    const SnapDir dir = (dirIdx == 1) ? SnapDir::Up
                      : (dirIdx == 2) ? SnapDir::Down
                                      : SnapDir::Nearest;

    // Update delay if param changed
    _delay.setDelayMs (static_cast<double> (delayMs));

    // Update BPM from host
    if (const auto* ph = getPlayHead())
        if (auto info = ph->getPosition(); info.hasValue())
            if (auto bpm = info->getBpm(); bpm.hasValue())
                _timeQ.setBpm (*bpm);

    // Time config
    const int gridIdx = static_cast<int> (apvts.getRawParameterValue (ParamID::timeGrid)->load());
    TimeConfig timeCfg;
    timeCfg.grid       = static_cast<TimeGrid> (gridIdx);
    timeCfg.strength   = apvts.getRawParameterValue (ParamID::timeStrength)->load();
    timeCfg.humanizeMs = apvts.getRawParameterValue (ParamID::humanizeTime)->load();
    timeCfg.humanizeVel= apvts.getRawParameterValue (ParamID::humanizeVel)->load();
    timeCfg.swing      = apvts.getRawParameterValue (ParamID::swing)->load();

    // Voice config
    const int voiceModeIdx  = static_cast<int> (apvts.getRawParameterValue (ParamID::voiceMode)->load());
    const int monoSelIdx    = static_cast<int> (apvts.getRawParameterValue (ParamID::monoSelect)->load());
    VoiceConfig voiceCfg;
    voiceCfg.mode        = static_cast<VoiceMode> (voiceModeIdx);
    voiceCfg.monoSelect  = static_cast<MonoSelect> (monoSelIdx);
    voiceCfg.chordMask   = activeMask;
    voiceCfg.chordRoot   = activeRoot;
    voiceCfg.splitVoices = static_cast<int> (apvts.getRawParameterValue (ParamID::splitVoices)->load());
    voiceCfg.splitChannel= static_cast<int> (apvts.getRawParameterValue (ParamID::splitChannel)->load());
    voiceCfg.loNote      = loNote;
    voiceCfg.hiNote      = hiNote;

    // ── Push incoming MIDI through delay buffer ───────────────────────────────
    const int blockSize = getBlockSize();
    double ppqAtStart = 0.0;
    if (const auto* ph = getPlayHead())
        if (auto info = ph->getPosition(); info.hasValue())
            if (auto ppq = info->getPpqPosition(); ppq.hasValue())
                ppqAtStart = *ppq;

    if (delayMs > 0.0f)
    {
        // Push all incoming events into the delay buffer
        for (const auto meta : midi)
            _delay.push (meta.getMessage(), meta.samplePosition);
        midi.clear();
        // Pop events that are old enough
        _delay.pop (blockSize, midi);
    }

    // ── Process events ────────────────────────────────────────────────────────
    juce::MidiBuffer processed;

    for (const auto meta : midi)
    {
        const auto& msg = meta.getMessage();
        int samplePos   = meta.samplePosition;

        if (msg.isNoteOn())
        {
            int note = msg.getNoteNumber();
            int vel  = msg.getVelocity();

            // Time quantize
            if (timeCfg.grid != TimeGrid::Off || timeCfg.humanizeMs > 0.0f)
            {
                const int offset = _timeQ.applyGrid (samplePos, ppqAtStart, blockSize, timeCfg, vel);
                samplePos = juce::jlimit (0, blockSize - 1, samplePos + offset);
            }

            // Pitch quantize
            note = quantize (note, activeMask, activeRoot, dir, loNote, hiNote, strength);

            // Voice processing
            int outNotes[VoiceProcessor::kMaxChordVoices];
            int outChannels[VoiceProcessor::kMaxChordVoices];
            const int n = _voices.processNoteOn (
                note, msg.getChannel(), voiceCfg, outNotes, outChannels);

            for (int i = 0; i < n; ++i)
                processed.addEvent (
                    juce::MidiMessage::noteOn (outChannels[i], outNotes[i], (juce::uint8) vel),
                    samplePos);
        }
        else if (msg.isNoteOff())
        {
            const int note = msg.getNoteNumber();
            const int ch   = _voices.processNoteOff (note);
            processed.addEvent (
                juce::MidiMessage::noteOff (ch > 0 ? ch : msg.getChannel(), note),
                samplePos);
        }
        else
        {
            // Pass everything else (CC, PB, etc.) through unchanged.
            processed.addEvent (msg, samplePos);
        }
    }

    midi.swapWith (processed);

    // ── Standalone virtual output ─────────────────────────────────────────────
    if (auto* out = _virtualOut.load (std::memory_order_relaxed))
        for (const auto meta : midi)
            out->sendMessageNow (meta.getMessage());
    if (auto* out = _directOut.load (std::memory_order_relaxed))
        for (const auto meta : midi)
            out->sendMessageNow (meta.getMessage());
}

// ── Editor ────────────────────────────────────────────────────────────────────

juce::AudioProcessorEditor* PitchFoldProcessor::createEditor()
{
    return new PitchFoldEditor (*this);
}

// ── State ─────────────────────────────────────────────────────────────────────

void PitchFoldProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    // Persist APVTS + pad labels (labels are not APVTS params).
    auto state = apvts.copyState();

    auto labels = juce::XmlElement ("PadLabels");
    for (int i = 0; i < pf::kNumPads; ++i)
    {
        auto* el = labels.createNewChildElement ("Pad");
        el->setAttribute ("index", i);
        el->setAttribute ("label", _pads.pad(i).label);
    }
    state.appendChild (juce::ValueTree::fromXml (labels), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void PitchFoldProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        apvts.replaceState (state);

        // Restore pad labels
        if (auto* labels = xml->getChildByName ("PadLabels"))
        {
            for (auto* el : labels->getChildIterator())
            {
                const int idx = el->getIntAttribute ("index", -1);
                if (idx >= 0 && idx < pf::kNumPads)
                    _pads.setPadLabel (idx, el->getStringAttribute ("label").toRawUTF8());
            }
        }

        // Sync pad masks/roots from APVTS to ChordPadBank.
        for (int i = 0; i < pf::kNumPads; ++i)
        {
            if (auto* p = apvts.getRawParameterValue (ParamID::padMask (i)))
                _pads.setPadMask (i, static_cast<uint16_t> (p->load()));
            if (auto* p = apvts.getRawParameterValue (ParamID::padRoot (i)))
                _pads.setPadRoot (i, static_cast<int> (p->load()));
        }
    }
}

// ── Pad management ────────────────────────────────────────────────────────────

void PitchFoldProcessor::setPadMask (int pad, uint16_t mask) noexcept
{
    _pads.setPadMask (pad, mask);
    if (auto* p = apvts.getParameter (ParamID::padMask (pad)))
        p->setValueNotifyingHost (
            static_cast<float> (mask) / 4095.0f);
}

void PitchFoldProcessor::setPadRoot (int pad, int root) noexcept
{
    _pads.setPadRoot (pad, root);
    if (auto* p = apvts.getParameter (ParamID::padRoot (pad)))
        p->setValueNotifyingHost (
            static_cast<float> (root) / 11.0f);
}

void PitchFoldProcessor::setPadLabel (int pad, const juce::String& label) noexcept
{
    _pads.setPadLabel (pad, label.toRawUTF8());
}

void PitchFoldProcessor::selectPad (int pad) noexcept
{
    _pads.select (pad);
}

int PitchFoldProcessor::selectedPad() const noexcept
{
    return _pads.selected();
}

// ── Panic ─────────────────────────────────────────────────────────────────────

void PitchFoldProcessor::sendPanic() noexcept
{
    _panicNeeded.store (true, std::memory_order_relaxed);
}

// ── MIDI output ───────────────────────────────────────────────────────────────

void PitchFoldProcessor::setVirtualMidiOutput (juce::MidiOutput* out) noexcept
{
    _virtualOut.store (out, std::memory_order_relaxed);
}

void PitchFoldProcessor::setDirectMidiOutput (juce::MidiOutput* out) noexcept
{
    _directOut.store (out, std::memory_order_relaxed);
}

juce::MidiOutput* PitchFoldProcessor::getDirectMidiOutput() const noexcept
{
    return _directOut.load (std::memory_order_relaxed);
}

// ── JUCE plugin factory ───────────────────────────────────────────────────────

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchFoldProcessor();
}
