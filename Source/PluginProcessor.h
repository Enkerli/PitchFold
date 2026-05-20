#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PCS/PCSEngine.h"
#include "Quantizer/DelayBuffer.h"
#include "Quantizer/TimeQuantizer.h"
#include "Quantizer/VoiceProcessor.h"
#include "Pads/ChordPadBank.h"

namespace juce { class MidiOutput; }

// ── Parameter IDs ─────────────────────────────────────────────────────────────
// These are the stable serialized ABI.  Never rename a shipped ID.
namespace ParamID
{
    // PCS
    inline const juce::String pcsRoot      { "pcsRoot"      };  // 0–11
    inline const juce::String pcsMask      { "pcsMask"      };  // 0–4095
    inline const juce::String quantDir     { "quantDir"     };  // 0=Nearest 1=Up 2=Down
    inline const juce::String quantStrength{ "quantStrength"};  // 0–1
    inline const juce::String outputLo     { "outputLo"     };  // 0–127
    inline const juce::String outputHi     { "outputHi"     };  // 0–127
    inline const juce::String useFlats     { "useFlats"     };  // bool

    // Time
    inline const juce::String timeGrid     { "timeGrid"     };  // TimeGrid enum index
    inline const juce::String timeStrength { "timeStrength" };  // 0–1
    inline const juce::String humanizeTime { "humanizeTime" };  // 0–200 ms
    inline const juce::String humanizeVel  { "humanizeVel"  };  // 0–1
    inline const juce::String swing        { "swing"        };  // 0–1

    // Delay / look-ahead
    inline const juce::String lookAheadMs  { "lookAheadMs"  };  // 0–500 ms

    // Voice
    inline const juce::String voiceMode    { "voiceMode"    };  // VoiceMode index
    inline const juce::String monoSelect   { "monoSelect"   };  // MonoSelect index
    inline const juce::String splitVoices  { "splitVoices"  };  // 1–4
    inline const juce::String splitChannel { "splitChannel" };  // 1–13

    // Chord pads — pad N mask/root stored as padMaskN / padRootN
    inline juce::String padMask (int n) { return "padMask" + juce::String (n); }
    inline juce::String padRoot (int n) { return "padRoot" + juce::String (n); }
}

// ── Processor ─────────────────────────────────────────────────────────────────

class PitchFoldProcessor : public juce::AudioProcessor
{
public:
    PitchFoldProcessor();
    ~PitchFoldProcessor() override;

    // ── Identity ──────────────────────────────────────────────────────────────
    const juce::String getName() const override { return "PitchFold"; }
    bool acceptsMidi()  const override { return true;  }
    bool producesMidi() const override { return true;  }
    bool isMidiEffect() const override { return true;  }
    double getTailLengthSeconds() const override { return 0.0; }

    // ── Programs ──────────────────────────────────────────────────────────────
    int  getNumPrograms()                             override { return 1; }
    int  getCurrentProgram()                          override { return 0; }
    void setCurrentProgram (int)                      override {}
    const juce::String getProgramName (int)           override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    // ── Editor ───────────────────────────────────────────────────────────────
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    // ── State ─────────────────────────────────────────────────────────────────
    void getStateInformation (juce::MemoryBlock& destData)       override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ── APVTS ─────────────────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();

    // ── Pad management (UI thread) ────────────────────────────────────────────
    void setPadMask  (int pad, uint16_t mask) noexcept;
    void setPadRoot  (int pad, int root)      noexcept;
    void setPadLabel (int pad, const juce::String& label) noexcept;
    void selectPad   (int pad)                noexcept;   // -1 = deselect
    int  selectedPad()                 const  noexcept;

    const pf::ChordPadBank& padBank() const noexcept { return _pads; }

    // ── MIDI panic ────────────────────────────────────────────────────────────
    void sendPanic() noexcept;

    // ── Standalone MIDI output ─────────────────────────────────────────────────
    void setVirtualMidiOutput (juce::MidiOutput* out) noexcept;
    void setDirectMidiOutput  (juce::MidiOutput* out) noexcept;
    juce::MidiOutput* getDirectMidiOutput() const noexcept;

private:
    // ── Processing helpers ────────────────────────────────────────────────────
    void processNoteOn  (const juce::MidiMessage& msg, int samplePos,
                         juce::MidiBuffer& out);
    void processNoteOff (const juce::MidiMessage& msg, int samplePos,
                         juce::MidiBuffer& out);

    // ── Sub-systems ───────────────────────────────────────────────────────────
    pf::DelayBuffer   _delay;
    pf::TimeQuantizer _timeQ;
    pf::VoiceProcessor _voices;
    pf::ChordPadBank  _pads;

    // Pending MIDI for standalone virtual output
    juce::MidiBuffer _pendingOut;
    juce::SpinLock   _pendingLock;

    std::atomic<bool>          _panicNeeded  { false };
    std::atomic<juce::MidiOutput*> _virtualOut { nullptr };
    std::atomic<juce::MidiOutput*> _directOut  { nullptr };

    double _sampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchFoldProcessor)
};
