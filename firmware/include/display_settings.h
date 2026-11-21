#ifndef DISPLAY_SETTINGS_H
#define DISPLAY_SETTINGS_H

#include <cstdint>
#include "display_brightness.h"
#include "display_color.h"

// Spec 03 §§2a/3: loop-owned operator display settings, never part of GameState
// or Undo. They are saved only after SAVE_DELAY_MS without any change, and only
// the setting that differs from storage is written, bounding flash writes.
class DisplaySettings {
public:
    static constexpr uint32_t SAVE_DELAY_MS = 3000;
    explicit DisplaySettings(uint8_t level = BRIGHTNESS_DEFAULT_LEVEL, LedColor color = LED_COLOR_DEFAULT)
        : currentLevel(level), storedLevel(level), currentColor(color), storedColor(color) {}
    uint8_t level() const { return currentLevel; }
    LedColor color() const { return currentColor; }
    bool applyLevel(uint8_t level, uint32_t now) { return change(currentLevel, level, now); }
    bool applyColor(LedColor color, uint32_t now) { return change(currentColor, color, now); }
    bool levelUnsaved() const { return currentLevel != storedLevel; }
    bool colorUnsaved() const { return currentColor != storedColor; }
    bool saveDue(uint32_t now) const
    {
        return (levelUnsaved() || colorUnsaved()) && uint32_t(now - changedAt) >= SAVE_DELAY_MS;
    }
    // A failed write is retried after another full delay, never in a tight loop.
    void saved(bool ok, uint32_t now)
    {
        if (ok) {
            storedLevel = currentLevel;
            storedColor = currentColor;
        } else {
            changedAt = now;
        }
    }

private:
    template <typename T>
    bool change(T& current, T next, uint32_t now)
    {
        if (next == current) return false;
        current = next;
        changedAt = now;
        return true;
    }
    uint8_t currentLevel;
    uint8_t storedLevel;
    LedColor currentColor;
    LedColor storedColor;
    uint32_t changedAt = 0;
};

#endif
