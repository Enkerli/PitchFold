#pragma once

/**
 * PCSEngine — real-time pitch-class set quantization.
 *
 * No JUCE dependency.  Header-only, no allocations.
 *
 * Bitmask convention (leftmost-LSB, shared with ScaleData.h — matches the
 * suite-wide convention, @enkerli/theory, and the webapp twin):
 *   bit (interval) = 1  →  interval is active.
 *   bit 0 = unison (interval 0), bit 11 = major 7th (interval 11).
 *
 * All lookups are root-relative: interval = ((pitch - root) % 12 + 12) % 12.
 *
 * (This comment previously documented the OLD MSB-first scheme, from before
 * the 2026-06-28 harmonization — the code below was always correct, only
 * the comment was stale. Fixed as part of docs/PITCHFOLD_AUDIT.md.)
 */

#include <cstdint>
#include <limits>

namespace pf
{

// ── Bit helpers ───────────────────────────────────────────────────────────────

inline bool pcActive (uint16_t mask, int interval) noexcept
{
    interval = ((interval % 12) + 12) % 12;
    return (mask >> (interval)) & 1;
}

// True if pitch class pc is in the scale defined by (mask, root).
inline bool pitchInScale (int pitch, uint16_t mask, int root) noexcept
{
    const int interval = ((pitch - root) % 12 + 12) % 12;
    return pcActive (mask, interval);
}

// ── Snap direction ────────────────────────────────────────────────────────────

enum class SnapDir { Nearest, Up, Down };

// ── Core quantizer ────────────────────────────────────────────────────────────

/**
 * Quantize @p midiNote to the nearest active pitch class.
 *
 * @param midiNote  Input MIDI note (0–127).
 * @param mask      12-bit PCS bitmask.
 * @param root      Root pitch class (0–11, C=0).
 * @param dir       Snap direction.
 * @param loNote    Minimum allowed output note (clamped).
 * @param hiNote    Maximum allowed output note (clamped).
 * @param strength  0.0 = pass-through, 1.0 = full snap.  Intermediate values
 *                  pick the nearest candidate and blend toward the original.
 *                  The returned note is always integral (rounded half-up).
 * @return          Quantized MIDI note, or midiNote if mask is empty.
 */
inline int quantize (int    midiNote,
                     uint16_t mask,
                     int    root,
                     SnapDir dir,
                     int    loNote,
                     int    hiNote,
                     float  strength = 1.0f) noexcept
{
    if (mask == 0u || strength <= 0.0f)
        return midiNote;

    // Chromatic — no snapping needed.
    if (mask == 0x0FFFu)
        return midiNote;

    int best      = -1;
    int bestDist  = std::numeric_limits<int>::max();

    // Search a ±12 semitone window to find the nearest active note.
    for (int delta = -12; delta <= 12; ++delta)
    {
        const int candidate = midiNote + delta;
        if (candidate < loNote || candidate > hiNote) continue;
        if (!pitchInScale (candidate, mask, root))     continue;

        const int dist = delta;  // signed distance
        switch (dir)
        {
            case SnapDir::Up:
                if (dist < 0) continue;
                if (dist < bestDist) { best = candidate; bestDist = dist; }
                break;
            case SnapDir::Down:
                if (dist > 0) continue;
                if (-dist < bestDist) { best = candidate; bestDist = -dist; }
                break;
            case SnapDir::Nearest:
                if (std::abs (dist) < bestDist)
                {
                    best = candidate;
                    bestDist = std::abs (dist);
                }
                break;
        }
    }

    if (best < 0)
        return midiNote;   // no candidate found in range

    if (strength >= 1.0f)
        return best;

    // Partial quantization: blend toward best.
    const float blended = midiNote + (best - midiNote) * strength;
    return static_cast<int> (blended + 0.5f);
}

// ── Chord builder ─────────────────────────────────────────────────────────────

/**
 * Fill @p out with up to @p maxVoices MIDI notes forming a chord from
 * @p rootNote, using the intervals present in @p mask.
 *
 * Voices are assigned to successive active intervals above rootNote,
 * staying within [loNote, hiNote].  Returns the number of notes written.
 */
inline int buildChord (int        rootNote,
                       uint16_t   mask,
                       int        scaleRoot,
                       int        maxVoices,
                       int        loNote,
                       int        hiNote,
                       int*       out) noexcept
{
    if (!out || maxVoices <= 0 || mask == 0u) return 0;

    int count = 0;
    // Add the root itself if it's in range and active.
    if (rootNote >= loNote && rootNote <= hiNote)
        out[count++] = rootNote;

    for (int delta = 1; delta <= 24 && count < maxVoices; ++delta)
    {
        const int candidate = rootNote + delta;
        if (candidate > hiNote) break;
        if (pitchInScale (candidate, mask, scaleRoot))
            out[count++] = candidate;
    }
    return count;
}

// ── Rotary interval chord (Robby Kilgore style) ───────────────────────────────

/**
 * Harmonise @p midiNote by adding stacked intervals from @p intervalsMask
 * (interpreted as semitone offsets above the note).
 *
 * @p intervalsMask uses the same 12-bit convention as the PCS mask but here
 * it encodes which intervals (1–12 semitones) to add above the input note.
 * Bit 11 = interval 0 (unison, always included), bit 10 = +1 semitone, etc.
 *
 * Returns the number of notes written into @p out.
 */
inline int harmonize (int       midiNote,
                      uint16_t  intervalsMask,
                      int       maxVoices,
                      int       loNote,
                      int       hiNote,
                      int*      out) noexcept
{
    if (!out || maxVoices <= 0) return 0;
    int count = 0;
    for (int i = 0; i < 12 && count < maxVoices; ++i)
    {
        if (!((intervalsMask >> (i)) & 1)) continue;
        const int note = midiNote + i;
        if (note >= loNote && note <= hiNote)
            out[count++] = note;
    }
    return count;
}

} // namespace pf
