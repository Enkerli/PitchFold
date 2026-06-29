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
        juce::ParameterID { ParamID::pcsMask, 1 }, "PCS Mask", 0, 4095, 0x0AB5));

    // 0=Auto 1=Nearest 2=Up 3=Down
    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamID::quantDir, 1 }, "Snap Direction",
        juce::StringArray { "Auto", "Nearest", "Up", "Down" }, 0));

    // Not exposed in UI yet — reserved for probability/histogram features.
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

// ── Constructor / Destructor ──────────────────────────────────────────────────

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
    _noteMap = {};
    _lastInputNote = -1;
}

void PitchFoldProcessor::releaseResources()
{
    _delay.clear();
    _voices.reset();
    _noteMap = {};
    _lastInputNote = -1;
}

// ── Note-map helpers ──────────────────────────────────────────────────────────

// Helper visible only in this TU.
namespace {
void releaseRecord (std::array<PitchFoldProcessor::NoteRecord, 128>& map,
                    int inputNote, int samplePos, juce::MidiBuffer& out) noexcept
{
    if (inputNote < 0 || inputNote > 127) return;
    auto& rec = map[static_cast<std::size_t> (inputNote)];
    if (!rec.active) return;
    for (int i = 0; i < rec.count; ++i)
    {
        const auto& on = rec.outputs[i];
        if (on.note >= 0)
            out.addEvent (juce::MidiMessage::noteOff (on.channel, on.note), samplePos);
    }
    rec = {};
}
} // namespace

void PitchFoldProcessor::clearNoteMap (juce::MidiBuffer& out, int samplePos) noexcept
{
    for (int n = 0; n < 128; ++n)
        releaseRecord (_noteMap, n, samplePos, out);
}

// ── processBlock ──────────────────────────────────────────────────────────────

void PitchFoldProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer& midi)
{
    // ── Panic ─────────────────────────────────────────────────────────────────
    // Send tracked NoteOffs first (so hosts that ignore AllNotesOff still work),
    // then AllNotesOff on every channel as a belt-and-suspenders sweep.
    // Forward to virtual output BEFORE returning.
    if (_panicNeeded.exchange (false))
    {
        juce::MidiBuffer panicBuf;
        clearNoteMap (panicBuf, 0);
        for (int ch = 1; ch <= 16; ++ch)
            panicBuf.addEvent (juce::MidiMessage::allNotesOff (ch), 0);

        midi.swapWith (panicBuf);

        if (auto* out = _virtualOut.load (std::memory_order_relaxed))
            for (const auto meta : midi) out->sendMessageNow (meta.getMessage());
        if (auto* out = _directOut.load (std::memory_order_relaxed))
            for (const auto meta : midi) out->sendMessageNow (meta.getMessage());

        _voices.reset();
        _lastInputNote = -1;
        return;
    }

    // ── Read parameters (once per block) ──────────────────────────────────────
    const int root    = static_cast<int> (apvts.getRawParameterValue (ParamID::pcsRoot)->load());
    const int maskRaw = static_cast<int> (apvts.getRawParameterValue (ParamID::pcsMask)->load());
    const auto mask   = static_cast<uint16_t> (maskRaw);
    const int dirIdx  = static_cast<int> (apvts.getRawParameterValue (ParamID::quantDir)->load());
    const int loNote  = static_cast<int> (apvts.getRawParameterValue (ParamID::outputLo)->load());
    const int hiNote  = static_cast<int> (apvts.getRawParameterValue (ParamID::outputHi)->load());
    const float delayMs = apvts.getRawParameterValue (ParamID::lookAheadMs)->load();

    const uint16_t activeMask = _pads.activeMask (mask);
    const int      activeRoot = _pads.activeRoot (root);

    _delay.setDelayMs (static_cast<double> (delayMs));

    if (const auto* ph = getPlayHead())
        if (auto info = ph->getPosition(); info.hasValue())
            if (auto bpm = info->getBpm(); bpm.hasValue())
                _timeQ.setBpm (*bpm);

    const int gridIdx = static_cast<int> (apvts.getRawParameterValue (ParamID::timeGrid)->load());
    TimeConfig timeCfg;
    timeCfg.grid        = static_cast<TimeGrid> (gridIdx);
    timeCfg.strength    = apvts.getRawParameterValue (ParamID::timeStrength)->load();
    timeCfg.humanizeMs  = apvts.getRawParameterValue (ParamID::humanizeTime)->load();
    timeCfg.humanizeVel = apvts.getRawParameterValue (ParamID::humanizeVel)->load();
    timeCfg.swing       = apvts.getRawParameterValue (ParamID::swing)->load();

    const int voiceModeIdx = static_cast<int> (apvts.getRawParameterValue (ParamID::voiceMode)->load());
    VoiceConfig voiceCfg;
    voiceCfg.mode         = static_cast<VoiceMode> (voiceModeIdx);
    voiceCfg.monoSelect   = static_cast<MonoSelect> (
        static_cast<int> (apvts.getRawParameterValue (ParamID::monoSelect)->load()));
    voiceCfg.chordMask    = activeMask;
    voiceCfg.chordRoot    = activeRoot;
    voiceCfg.splitVoices  = static_cast<int> (apvts.getRawParameterValue (ParamID::splitVoices)->load());
    voiceCfg.splitChannel = static_cast<int> (apvts.getRawParameterValue (ParamID::splitChannel)->load());
    voiceCfg.loNote       = loNote;
    voiceCfg.hiNote       = hiNote;

    // ── Delay buffer ──────────────────────────────────────────────────────────
    const int blockSize = getBlockSize();
    double ppqAtStart = 0.0;
    if (const auto* ph = getPlayHead())
        if (auto info = ph->getPosition(); info.hasValue())
            if (auto ppq = info->getPpqPosition(); ppq.hasValue())
                ppqAtStart = *ppq;

    if (delayMs > 0.0f)
    {
        for (const auto meta : midi)
            _delay.push (meta.getMessage(), meta.samplePosition);
        midi.clear();
        _delay.pop (blockSize, midi);
    }

    // ── Process events ────────────────────────────────────────────────────────
    juce::MidiBuffer processed;

    for (const auto meta : midi)
    {
        const auto& msg = meta.getMessage();
        int samplePos = meta.samplePosition;

        if (msg.isNoteOn())
        {
            const int inputNote = msg.getNoteNumber();
            int vel             = msg.getVelocity();

            // Auto snap direction: follow pitch motion
            SnapDir dir;
            if (dirIdx == 0)  // Auto
            {
                if      (_lastInputNote < 0)          dir = SnapDir::Nearest;
                else if (inputNote > _lastInputNote)  dir = SnapDir::Up;
                else if (inputNote < _lastInputNote)  dir = SnapDir::Down;
                else                                  dir = SnapDir::Nearest;
            }
            else
            {
                dir = (dirIdx == 2) ? SnapDir::Up
                    : (dirIdx == 3) ? SnapDir::Down
                                    : SnapDir::Nearest;
            }
            _lastInputNote = inputNote;

            // Time quantization
            if (timeCfg.grid != TimeGrid::Off || timeCfg.humanizeMs > 0.0f)
            {
                const int offset = _timeQ.applyGrid (samplePos, ppqAtStart, blockSize, timeCfg, vel);
                samplePos = juce::jlimit (0, blockSize - 1, samplePos + offset);
            }

            // Pitch quantization
            const int qNote = quantize (inputNote, activeMask, activeRoot, dir, loNote, hiNote);

            // Voice processing
            int outNotes   [VoiceProcessor::kMaxChordVoices];
            int outChannels[VoiceProcessor::kMaxChordVoices];
            const int n = _voices.processNoteOn (qNote, msg.getChannel(), voiceCfg,
                                                 outNotes, outChannels);

            // Record output notes so NoteOff can match them
            // If there was already a record for this input note, release it first.
            releaseRecord (_noteMap, inputNote, samplePos, processed);

            auto& rec = _noteMap[static_cast<std::size_t> (inputNote)];
            rec.active = true;
            rec.count  = std::min (n, static_cast<int> (VoiceProcessor::kMaxChordVoices));
            for (int i = 0; i < rec.count; ++i)
            {
                rec.outputs[i].note    = outNotes[i];
                rec.outputs[i].channel = outChannels[i];
            }

            for (int i = 0; i < n; ++i)
                processed.addEvent (
                    juce::MidiMessage::noteOn (outChannels[i], outNotes[i], (juce::uint8) vel),
                    samplePos);
        }
        else if (msg.isNoteOff())
        {
            const int inputNote = msg.getNoteNumber();
            releaseRecord (_noteMap, inputNote, samplePos, processed);
        }
        else
        {
            processed.addEvent (msg, samplePos);
        }
    }

    midi.swapWith (processed);

    // ── Standalone virtual output ─────────────────────────────────────────────
    if (auto* out = _virtualOut.load (std::memory_order_relaxed))
        for (const auto meta : midi) out->sendMessageNow (meta.getMessage());
    if (auto* out = _directOut.load (std::memory_order_relaxed))
        for (const auto meta : midi) out->sendMessageNow (meta.getMessage());
}

// ── Editor ────────────────────────────────────────────────────────────────────

juce::AudioProcessorEditor* PitchFoldProcessor::createEditor()
{
    return new PitchFoldEditor (*this);
}

// ── State ─────────────────────────────────────────────────────────────────────

void PitchFoldProcessor::getStateInformation (juce::MemoryBlock& dest)
{
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

        if (auto* labels = xml->getChildByName ("PadLabels"))
            for (auto* el : labels->getChildIterator())
            {
                const int idx = el->getIntAttribute ("index", -1);
                if (idx >= 0 && idx < pf::kNumPads)
                    _pads.setPadLabel (idx, el->getStringAttribute ("label").toRawUTF8());
            }

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
        p->setValueNotifyingHost (static_cast<float> (mask) / 4095.0f);
}

void PitchFoldProcessor::setPadRoot (int pad, int root) noexcept
{
    _pads.setPadRoot (pad, root);
    if (auto* p = apvts.getParameter (ParamID::padRoot (pad)))
        p->setValueNotifyingHost (static_cast<float> (root) / 11.0f);
}

void PitchFoldProcessor::setPadLabel (int pad, const juce::String& label) noexcept
{
    _pads.setPadLabel (pad, label.toRawUTF8());
}

void PitchFoldProcessor::selectPad (int pad) noexcept { _pads.select (pad); }
int  PitchFoldProcessor::selectedPad() const noexcept { return _pads.selected(); }

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

// ── Plugin factory ────────────────────────────────────────────────────────────

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchFoldProcessor();
}
