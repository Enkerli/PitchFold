#pragma once

/**
 * ChordPadBank — 16 performance pads, each storing a PCS + root + label.
 *
 * A pad is "active" when selected by the user (via UI or MIDI note).
 * Only one pad is active at a time (radio behaviour); activating pad -1
 * reverts to the main PCS params.
 *
 * No JUCE dependency.  Header-only.
 */

#include <cstdint>
#include <cstring>
#include <array>
#include <atomic>

namespace pf
{

static constexpr int kNumPads = 16;

struct Pad
{
    uint16_t mask  { 0x0AD5u };  // Ionian (major) default
    int      root  { 0 };        // C
    char     label[32] = {};     // short display name, UTF-8

    void setLabel (const char* s) noexcept
    {
        std::strncpy (label, s, sizeof (label) - 1);
        label[sizeof (label) - 1] = '\0';
    }
};

class ChordPadBank
{
public:
    ChordPadBank()
    {
        // Populate with the 7 diatonic modes as defaults.
        static const struct { uint16_t mask; const char* name; } kDefaults[kNumPads] =
        {
            { 0x0AD5, "Ionian"      }, { 0x0B56, "Dorian"     },
            { 0x0D5A, "Phrygian"    }, { 0x0AB5, "Lydian"     },
            { 0x0AD6, "Mixolydian"  }, { 0x0B5A, "Aeolian"    },
            { 0x0D6A, "Locrian"     }, { 0x0A94, "Pent. Maj"  },
            { 0x0952, "Pent. Min"   }, { 0x0AAA, "Whole Tone" },
            { 0x0B6D, "Dim. WH"     }, { 0x0890, "Maj Triad"  },
            { 0x0910, "Min Triad"   }, { 0x0891, "Maj 7"      },
            { 0x0912, "Min 7"       }, { 0x0892, "Dom 7"      },
        };

        for (int i = 0; i < kNumPads; ++i)
        {
            _pads[i].mask = kDefaults[i].mask;
            _pads[i].root = 0;
            _pads[i].setLabel (kDefaults[i].name);
        }
    }

    // ── Pad access ────────────────────────────────────────────────────────────

    const Pad& pad (int i) const noexcept { return _pads[juce_clamp (i)]; }
    Pad&       pad (int i)       noexcept { return _pads[juce_clamp (i)]; }

    void setPadMask  (int i, uint16_t mask)  noexcept { _pads[juce_clamp(i)].mask = mask; }
    void setPadRoot  (int i, int root)       noexcept { _pads[juce_clamp(i)].root = root; }
    void setPadLabel (int i, const char* s)  noexcept { _pads[juce_clamp(i)].setLabel(s); }

    // ── Selection (radio, -1 = none) ─────────────────────────────────────────

    void select (int i) noexcept   { _selected.store (i, std::memory_order_relaxed); }
    void deselect() noexcept       { _selected.store (-1, std::memory_order_relaxed); }
    int  selected() const noexcept { return _selected.load (std::memory_order_relaxed); }
    bool isSelected (int i) const noexcept { return selected() == i; }

    // Retrieve the currently active mask/root (returns mainMask/mainRoot when nothing selected).
    uint16_t activeMask (uint16_t mainMask) const noexcept
    {
        const int s = selected();
        return (s >= 0 && s < kNumPads) ? _pads[s].mask : mainMask;
    }

    int activeRoot (int mainRoot) const noexcept
    {
        const int s = selected();
        return (s >= 0 && s < kNumPads) ? _pads[s].root : mainRoot;
    }

    // ── MIDI-trigger mapping ───────────────────────────────────────────────────
    // Optionally map pad N to a MIDI note (e.g. C3–D#4 for 16 pads).
    // triggerNote < 0 means disabled.

    void setTriggerNote (int padIndex, int midiNote) noexcept
    {
        if (padIndex >= 0 && padIndex < kNumPads)
            _triggerNotes[padIndex] = midiNote;
    }

    // Returns the pad index for this MIDI note, or -1 if none.
    int padForNote (int midiNote) const noexcept
    {
        for (int i = 0; i < kNumPads; ++i)
            if (_triggerNotes[i] == midiNote) return i;
        return -1;
    }

private:
    std::array<Pad, kNumPads>  _pads;
    std::atomic<int>           _selected { -1 };
    std::array<int, kNumPads>  _triggerNotes;

    static int juce_clamp (int i) noexcept
    {
        return i < 0 ? 0 : (i >= kNumPads ? kNumPads - 1 : i);
    }
};

} // namespace pf
