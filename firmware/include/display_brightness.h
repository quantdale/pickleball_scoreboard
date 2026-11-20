#ifndef DISPLAY_BRIGHTNESS_H
#define DISPLAY_BRIGHTNESS_H

#include <cstddef>
#include <cstdint>

// Spec 03 §2a: operator brightness levels for the pinned HUB75 DMA driver
// (3.0.15, 64-clock rows, latch blanking 2, transition bit 0). setBrightness8
// only rewrites OE bits in the existing DMA buffer. Every value 1..12 gives the
// same two-clock OE window, so 12 (level 1) is already the dimmest lit output;
// each later value lights a distinct multiple of it, verified against the
// driver arithmetic by scripts/check_brightness_levels.py.
constexpr uint8_t BRIGHTNESS_LADDER[] = {12, 16, 24, 32, 48, 64, 96, 128}; // x1 x2 x3 x4 x6 x8 x12 x16
constexpr uint8_t BRIGHTNESS_LADDER_LEVELS = sizeof(BRIGHTNESS_LADDER);
constexpr uint8_t BRIGHTNESS_DEFAULT_LEVEL = 1;
static_assert(BRIGHTNESS_LADDER[BRIGHTNESS_DEFAULT_LEVEL - 1] == 12, "Spec 03 §2a: the default stays 12/255");

// Spec 03 §2a: build-time cap, NOT electrically certified. Raise it only after
// the physical acceptance test; the ladder itself never exceeds 128/255.
#ifndef PICKLE_BRIGHTNESS_MAX_LEVEL
#define PICKLE_BRIGHTNESS_MAX_LEVEL 4
#endif
static_assert(PICKLE_BRIGHTNESS_MAX_LEVEL >= 1 && PICKLE_BRIGHTNESS_MAX_LEVEL <= BRIGHTNESS_LADDER_LEVELS,
              "PICKLE_BRIGHTNESS_MAX_LEVEL must select a ladder level");
constexpr uint8_t BRIGHTNESS_MAX_LEVEL = PICKLE_BRIGHTNESS_MAX_LEVEL;

// Only call with a level from brightnessCommandLevel or restoredBrightnessLevel.
constexpr uint8_t brightnessValue(uint8_t level) { return BRIGHTNESS_LADDER[level - 1]; }

// Spec 02 §4: exactly 'B' and one digit 1..BRIGHTNESS_MAX_LEVEL; 0 = not a brightness command.
inline uint8_t brightnessCommandLevel(const char* bytes, std::size_t length)
{
    if (length != 2 || bytes[0] != 'B') return 0;
    const int level = bytes[1] - '0';
    return level >= 1 && level <= BRIGHTNESS_MAX_LEVEL ? static_cast<uint8_t>(level) : 0;
}

// Spec 03 §2a: a missing or corrupt record restores the default; a record above
// this build's cap is lowered to the cap.
constexpr uint8_t restoredBrightnessLevel(bool found, uint8_t stored)
{
    return !found || stored < 1 || stored > BRIGHTNESS_LADDER_LEVELS ? BRIGHTNESS_DEFAULT_LEVEL
         : stored > BRIGHTNESS_MAX_LEVEL ? BRIGHTNESS_MAX_LEVEL : stored;
}

// Spec 03 §2a: loop-owned display setting, never part of GameState or Undo.
// A level is saved only after SAVE_DELAY_MS without change, bounding flash writes.
class BrightnessSetting {
public:
    static constexpr uint32_t SAVE_DELAY_MS = 3000;
    explicit BrightnessSetting(uint8_t restored = BRIGHTNESS_DEFAULT_LEVEL) : current(restored), stored(restored) {}
    uint8_t level() const { return current; }
    bool apply(uint8_t level, uint32_t now)
    {
        if (level == current) return false;
        current = level;
        changedAt = now;
        return true;
    }
    bool saveDue(uint32_t now) const { return current != stored && uint32_t(now - changedAt) >= SAVE_DELAY_MS; }
    // A failed write is retried after another full delay, never in a tight loop.
    void saved(bool ok, uint32_t now)
    {
        if (ok) stored = current;
        else changedAt = now;
    }

private:
    uint8_t current;
    uint8_t stored;
    uint32_t changedAt = 0;
};

#endif
