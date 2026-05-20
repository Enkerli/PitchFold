#pragma once

/**
 * TimeQuantizer — grid-snap and humanize for MIDI timestamps.
 *
 * Given host PPQ position and a beat-subdivision grid, computes the
 * nearest grid boundary and applies configurable random jitter
 * ("humanization") in milliseconds and velocity scaling.
 *
 * No allocations; header-only; no JUCE dependency beyond juce_core for Random.
 */

#include <juce_core/juce_core.h>
#include <cmath>

namespace pf
{

enum class TimeGrid
{
    Off = 0,
    T32,      // 1/32
    T16T,     // 1/16 triplet
    T16,      // 1/16
    T8T,      // 1/8 triplet
    T8,       // 1/8
    T4T,      // 1/4 triplet
    T4,       // 1/4 (quarter)
};

struct TimeConfig
{
    TimeGrid grid          { TimeGrid::Off };
    float    strength      { 1.0f };    // 0=off, 1=full snap
    float    humanizeMs    { 0.0f };    // max random timing offset in ms
    float    humanizeVel   { 0.0f };    // max random velocity scale factor (0–1)
    float    swing         { 0.0f };    // 0=straight, 1=full triplet swing on even 16ths
};

class TimeQuantizer
{
public:
    TimeQuantizer() : _rng (juce::Random::getSystemRandom().nextInt()) {}

    void prepare (double sampleRate, double bpm) noexcept
    {
        _sampleRate = sampleRate;
        _bpm        = bpm;
    }

    void setBpm (double bpm) noexcept { _bpm = bpm; }

    /** Returns a sample-offset adjustment to apply to the event's position.
     *  Negative = move earlier; positive = move later.
     *
     *  @param samplePos     Original position in the block.
     *  @param ppqPos        Host PPQ position at the start of the current block.
     *  @param blockSize     Block size in samples.
     *  @param cfg           Current time quantizer config.
     *  @param velocity      Input velocity (0–127); modified by humanization.
     */
    int applyGrid (int           samplePos,
                   double        ppqPos,
                   int           blockSize,
                   const TimeConfig& cfg,
                   int&          velocity) noexcept
    {
        if (cfg.grid == TimeGrid::Off && cfg.humanizeMs <= 0.0f)
            return 0;

        int offset = 0;

        if (cfg.grid != TimeGrid::Off && cfg.strength > 0.0f && _bpm > 0.0)
        {
            const double gridBeats = gridSizeBeats (cfg.grid);
            const double samplesPerBeat = _sampleRate * 60.0 / _bpm;
            const double samplesPerGrid = samplesPerBeat * gridBeats;

            const double eventSample = ppqToSample (ppqPos, samplePos, samplesPerBeat);
            const double nearestGrid = std::round (eventSample / samplesPerGrid) * samplesPerGrid;
            const double rawOffset   = nearestGrid - eventSample;

            offset = static_cast<int> (rawOffset * cfg.strength);
        }

        // Humanize time
        if (cfg.humanizeMs > 0.0f)
        {
            const double maxSamples = cfg.humanizeMs * _sampleRate * 0.001;
            offset += static_cast<int> ((_rng.nextDouble() * 2.0 - 1.0) * maxSamples);
        }

        // Humanize velocity
        if (cfg.humanizeVel > 0.0f)
        {
            const float scale = 1.0f + (_rng.nextFloat() * 2.0f - 1.0f) * cfg.humanizeVel;
            velocity = juce::jlimit (1, 127, static_cast<int> (velocity * scale));
        }

        return offset;
    }

private:
    juce::Random _rng;
    double       _sampleRate { 44100.0 };
    double       _bpm        { 120.0 };

    static double gridSizeBeats (TimeGrid g) noexcept
    {
        switch (g)
        {
            case TimeGrid::T32:  return 1.0 / 8.0;
            case TimeGrid::T16T: return 1.0 / 6.0;
            case TimeGrid::T16:  return 1.0 / 4.0;
            case TimeGrid::T8T:  return 1.0 / 3.0;
            case TimeGrid::T8:   return 1.0 / 2.0;
            case TimeGrid::T4T:  return 2.0 / 3.0;
            case TimeGrid::T4:   return 1.0;
            default:             return 1.0;
        }
    }

    double ppqToSample (double ppqAtBlockStart, int samplePos, double samplesPerBeat) const noexcept
    {
        return ppqAtBlockStart * samplesPerBeat + samplePos;
    }
};

} // namespace pf
