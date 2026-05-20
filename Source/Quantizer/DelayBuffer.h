#pragma once

/**
 * DelayBuffer — fixed-latency MIDI event queue for retroactive quantization.
 *
 * Events are pushed with their original sample position.  pop() returns all
 * events whose age exceeds the configured delay, adjusted so the output
 * sample positions are correct for the current block.
 *
 * Thread safety: single producer (audio thread), single consumer (audio thread).
 * All operations are O(kCapacity) and allocation-free after construction.
 */

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cstdint>

namespace pf
{

struct DelayedEvent
{
    juce::MidiMessage msg;
    int               samplePos  { 0 };  // position in the block when pushed
    int64_t           pushSample { 0 };  // absolute sample counter at push time
};

class DelayBuffer
{
public:
    static constexpr int kCapacity = 2048;

    DelayBuffer() = default;

    // ── Configuration (call from prepareToPlay, not audio thread) ─────────────

    void prepare (double sampleRate, double delayMs) noexcept
    {
        _sampleRate  = sampleRate;
        _delaySamples = static_cast<int64_t> (delayMs * sampleRate * 0.001);
        clear();
    }

    void setDelayMs (double ms) noexcept
    {
        _delaySamples = static_cast<int64_t> (ms * _sampleRate * 0.001);
    }

    void clear() noexcept { _head = _tail = 0; _totalSamples = 0; }

    bool isEmpty() const noexcept { return _head == _tail; }

    // ── Audio-thread API ──────────────────────────────────────────────────────

    /**
     * Push one incoming MIDI event.
     * @param msg       The raw MIDI message.
     * @param samplePos Position within the current block (0-based).
     */
    void push (const juce::MidiMessage& msg, int samplePos) noexcept
    {
        const int next = (_tail + 1) % kCapacity;
        if (next == _head) return;   // full — drop oldest conceptually; just skip

        _buf[_tail] = { msg, samplePos, _totalSamples + samplePos };
        _tail = next;
    }

    /**
     * Drain events that are old enough (age >= delaySamples) into @p output.
     *
     * @param blockSize      Current block size (samples).
     * @param output         Destination MidiBuffer; events appended with
     *                       adjusted sample positions clamped to [0, blockSize-1].
     */
    void pop (int blockSize, juce::MidiBuffer& output) noexcept
    {
        const int64_t horizon = _totalSamples - _delaySamples;

        while (_head != _tail)
        {
            const auto& ev = _buf[_head];
            if (ev.pushSample > horizon) break;

            // Map original position back into the current block.
            const int64_t age  = _totalSamples - ev.pushSample;
            const int     pos  = static_cast<int> (
                juce::jlimit<int64_t> (0, blockSize - 1,
                                       static_cast<int64_t> (ev.samplePos) + _delaySamples - age));
            output.addEvent (ev.msg, pos);
            _head = (_head + 1) % kCapacity;
        }

        _totalSamples += blockSize;
    }

    // ── Accessors ─────────────────────────────────────────────────────────────

    double delayMs() const noexcept
    {
        return _sampleRate > 0.0 ? (_delaySamples * 1000.0 / _sampleRate) : 0.0;
    }

private:
    std::array<DelayedEvent, kCapacity> _buf;
    int      _head         { 0 };
    int      _tail         { 0 };
    int64_t  _totalSamples { 0 };
    int64_t  _delaySamples { 0 };
    double   _sampleRate   { 44100.0 };
};

} // namespace pf
