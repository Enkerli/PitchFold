#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PCS/PCSEngine.h"
#include "Quantizer/DelayBuffer.h"
#include "Quantizer/TimeQuantizer.h"
#include "Quantizer/VoiceProcessor.h"
#include "Pads/ChordPadBank.h"
#include <array>

namespace juce { class MidiOutput; }

// ── Parameter IDs ─────────────────────────────────────────────────────────────
namespace ParamID
{
    // PCS
    inline const juce::String pcsRoot      { "pcsRoot"      };  // 0–11
    inline const juce::String pcsMask      { "pcsMask"      };  // 0–4095
    // quantDir: 0=Auto 1=Nearest 2=Up 3=Down
    inline const juce::String quantDir     { "quantDir"     };
    // quantStrength kept for future probability/histogram use; not shown in UI yet
    inline const juce::String quantStrength{ "quantStrength"};
    inline const juce::String outputLo     { "outputLo"     };  // 0–127
    inline const juce::String outputHi     { "outputHi"     };  // 0–127
    inline const juce::String useFlats     { "useFlats"     };

    // Time
    inline const juce::String timeGrid     { "timeGrid"     };
    inline const juce::String timeStrength { "timeStrength" };
    inline const juce::String humanizeTime { "humanizeTime" };
    inline const juce::String humanizeVel  { "humanizeVel"  };
    inline const juce::String swing        { "swing"        };

    // Delay / look-ahead
    inline const juce::String lookAheadMs  { "lookAheadMs"  };

    // Voice
    inline const juce::String voiceMode    { "voiceMode"    };
    inline const juce::String monoSelect   { "monoSelect"   };
    inline const juce::String splitVoices  { "splitVoices"  };
    inline const juce::String splitChannel { "splitChannel" };

    // Chord pads
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
    void selectPad   (int pad)                noexcept;
    int  selectedPad()                 const  noexcept;

    const pf::ChordPadBank& padBank() const noexcept { return _pads; }

    // ── MIDI panic ────────────────────────────────────────────────────────────
    void sendPanic() noexcept;

    // ── Standalone MIDI output ─────────────────────────────────────────────────
    void setVirtualMidiOutput (juce::MidiOutput* out) noexcept;
    void setDirectMidiOutput  (juce::MidiOutput* out) noexcept;
    juce::MidiOutput* getDirectMidiOutput() const noexcept;

private:
    // ── Active-note map — fixes NoteOff matching after pitch quantization ─────
    // Indexed by the *input* note (0-127).  Stores every output note that was
    // generated for that input, so NoteOff can silence the right pitches even
    // when the quantizer remapped them to a different pitch class.
    struct OutNote { int note { -1 }; int channel { 1 }; };
    struct NoteRecord
    {
        bool    active { false };
        OutNote outputs[pf::VoiceProcessor::kMaxChordVoices] {};
        int     count  { 0 };
    };
    std::array<NoteRecord, 128> _noteMap {};

    void clearNoteMap (juce::MidiBuffer& out, int samplePos) noexcept;

    // ── Sub-systems ───────────────────────────────────────────────────────────
    pf::DelayBuffer    _delay;
    pf::TimeQuantizer  _timeQ;
    pf::VoiceProcessor _voices;
    pf::ChordPadBank   _pads;

    int _lastInputNote { -1 };  // for auto snap direction

    std::atomic<bool>              _panicNeeded { false };
    std::atomic<juce::MidiOutput*> _virtualOut  { nullptr };
    std::atomic<juce::MidiOutput*> _directOut   { nullptr };

    double _sampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchFoldProcessor)
};
