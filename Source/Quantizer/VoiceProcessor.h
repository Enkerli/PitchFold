#pragma once

/**
 * VoiceProcessor — mono/poly conversion and voice-splitting.
 *
 * Modes
 * ─────
 *   Through    — pass all incoming notes unchanged (only pitch quantization applied upstream)
 *   MonoMerge  — collapse all sounding voices to one (last/lowest/highest/first)
 *   PolySpread — from a single incoming note, generate a chord using the active PCS
 *   VoiceSplit — distribute voices to consecutive MIDI channels (up to 4)
 *   Chordize   — add harmonizing intervals above each incoming note (Robby Kilgore style)
 *
 * No allocation.  Caller owns the output buffer.  Thread-safe if the config is
 * written from the audio thread only.
 */

#include <juce_audio_basics/juce_audio_basics.h>
#include "../PCS/PCSEngine.h"
#include <array>

namespace pf
{

enum class VoiceMode
{
    Through = 0,
    MonoMerge,
    PolySpread,
    VoiceSplit,
    Chordize,
};

enum class MonoSelect { Last = 0, Lowest, Highest, First };

struct VoiceConfig
{
    VoiceMode  mode          { VoiceMode::Through };
    MonoSelect monoSelect    { MonoSelect::Last   };

    // PolySpread / Chordize: PCS used to generate chord tones.
    uint16_t   chordMask     { 0x0FFFu };  // chromatic by default
    int        chordRoot     { 0 };        // C

    // VoiceSplit
    int        splitVoices   { 2 };        // how many channels to spread across
    int        splitChannel  { 1 };        // first MIDI channel (1-based)

    // Output range (shared with pitch quantizer)
    int        loNote        { 0   };
    int        hiNote        { 127 };
};

// ── Active-voice tracker (for MonoMerge) ─────────────────────────────────────

class VoiceProcessor
{
public:
    static constexpr int kMaxActiveVoices = 32;
    static constexpr int kMaxChordVoices  = 8;

    void reset() noexcept
    {
        _activeCount  = 0;
        _firstNote    = -1;
        _splitIndex   = 0;
        std::fill (_activeNotes.begin(), _activeNotes.end(), -1);
        std::fill (_activeChannels.begin(), _activeChannels.end(), 0);
    }

    /**
     * Process a single Note On.
     * Writes zero or more notes into @p outNotes / @p outChannels.
     * Returns the number of notes written.
     */
    int processNoteOn (int note, int channel, const VoiceConfig& cfg,
                       int* outNotes, int* outChannels) noexcept
    {
        switch (cfg.mode)
        {
            case VoiceMode::Through:
                outNotes[0]    = note;
                outChannels[0] = channel;
                trackAdd (note, channel);
                return 1;

            case VoiceMode::MonoMerge:
                return processMono (note, channel, cfg, outNotes, outChannels);

            case VoiceMode::PolySpread:
                return processSpread (note, cfg, outNotes, outChannels);

            case VoiceMode::VoiceSplit:
                return processSplit (note, cfg, outNotes, outChannels);

            case VoiceMode::Chordize:
                return processChordize (note, cfg, outNotes, outChannels);
        }
        return 0;
    }

    /**
     * Process a Note Off.
     * Returns the channel on which that note was originally emitted.
     * Returns -1 if not tracked (caller should use the incoming channel).
     */
    int processNoteOff (int note) noexcept
    {
        for (int i = 0; i < _activeCount; ++i)
        {
            if (_activeNotes[i] == note)
            {
                const int ch = _activeChannels[i];
                // Compact
                for (int j = i; j < _activeCount - 1; ++j)
                {
                    _activeNotes[j]    = _activeNotes[j + 1];
                    _activeChannels[j] = _activeChannels[j + 1];
                }
                --_activeCount;
                return ch;
            }
        }
        return -1;
    }

    int activeCount() const noexcept { return _activeCount; }

private:
    std::array<int, kMaxActiveVoices> _activeNotes    {};
    std::array<int, kMaxActiveVoices> _activeChannels {};
    int _activeCount { 0 };
    int _firstNote   { -1 };
    int _splitIndex  { 0 };

    void trackAdd (int note, int channel) noexcept
    {
        if (_firstNote < 0) _firstNote = note;
        if (_activeCount >= kMaxActiveVoices) return;
        _activeNotes   [_activeCount] = note;
        _activeChannels[_activeCount] = channel;
        ++_activeCount;
    }

    int processMono (int note, int channel, const VoiceConfig& cfg,
                     int* outNotes, int* outChannels) noexcept
    {
        // Steal: turn off the current mono voice first (caller reads outCount=0 for off)
        // For simplicity, just replace.  The caller must send Note Off for the previous note.
        trackAdd (note, channel);
        (void) cfg;  // monoSelect used in the higher-level routing layer
        outNotes[0]    = note;
        outChannels[0] = channel;
        return 1;
    }

    int processSpread (int note, const VoiceConfig& cfg,
                       int* outNotes, int* outChannels) noexcept
    {
        int noteArr[kMaxChordVoices];
        const int n = buildChord (note, cfg.chordMask, cfg.chordRoot,
                                  kMaxChordVoices, cfg.loNote, cfg.hiNote, noteArr);
        for (int i = 0; i < n; ++i)
        {
            outNotes[i]    = noteArr[i];
            outChannels[i] = 1;
            trackAdd (noteArr[i], 1);
        }
        return n;
    }

    int processSplit (int note, const VoiceConfig& cfg,
                      int* outNotes, int* outChannels) noexcept
    {
        const int ch = cfg.splitChannel + (_splitIndex % juce::jmax (1, cfg.splitVoices));
        _splitIndex  = (_splitIndex + 1) % juce::jmax (1, cfg.splitVoices);
        outNotes[0]    = note;
        outChannels[0] = juce::jlimit (1, 16, ch);
        trackAdd (note, outChannels[0]);
        return 1;
    }

    int processChordize (int note, const VoiceConfig& cfg,
                         int* outNotes, int* outChannels) noexcept
    {
        int noteArr[kMaxChordVoices];
        const int n = harmonize (note, cfg.chordMask, kMaxChordVoices,
                                 cfg.loNote, cfg.hiNote, noteArr);
        for (int i = 0; i < n; ++i)
        {
            outNotes[i]    = noteArr[i];
            outChannels[i] = 1;
            trackAdd (noteArr[i], 1);
        }
        return n;
    }
};

} // namespace pf
