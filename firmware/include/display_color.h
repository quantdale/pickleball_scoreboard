#ifndef DISPLAY_COLOR_H
#define DISPLAY_COLOR_H

#include <cstddef>
#include <cstdint>

// Spec 03 §3: one operator color for every lit pixel. Each palette entry turns
// whole RGB channels fully on or off, so a lit channel gets exactly the bit-plane
// timing white uses today: no grey levels, no new framebuffer. The wire code is
// the color's ASCII letter (Spec 02 §§4–5a); White is the default.
enum class LedColor : char {
    White = 'W',
    Red = 'R',
    Yellow = 'Y',
    Green = 'G',
    Cyan = 'C',
    Blue = 'B',
};
constexpr LedColor LED_COLORS[] = {LedColor::White, LedColor::Red, LedColor::Yellow,
                                   LedColor::Green, LedColor::Cyan, LedColor::Blue};
constexpr LedColor LED_COLOR_DEFAULT = LedColor::White;

constexpr bool hasRed(LedColor c) { return c == LedColor::White || c == LedColor::Red || c == LedColor::Yellow; }
constexpr bool hasGreen(LedColor c) { return c != LedColor::Red && c != LedColor::Blue; }
constexpr bool hasBlue(LedColor c) { return c == LedColor::White || c == LedColor::Cyan || c == LedColor::Blue; }

// Spec 03 §3: full-scale RGB565 channels, which the driver expands to 0xFF.
constexpr uint16_t ledColor565(LedColor c)
{
    return uint16_t((hasRed(c) ? 0xF800 : 0) | (hasGreen(c) ? 0x07E0 : 0) | (hasBlue(c) ? 0x001F : 0));
}

// Spec 03 §2a: lit channels per pixel; LED current scales with it, so White is the worst case.
constexpr unsigned ledColorChannels(LedColor c) { return unsigned(hasRed(c)) + hasGreen(c) + hasBlue(c); }

inline bool isLedColorCode(uint8_t code)
{
    for (const LedColor color : LED_COLORS)
        if (static_cast<uint8_t>(color) == code) return true;
    return false;
}

// Spec 02 §4: exactly 'K' and one palette letter; false = not a color command.
inline bool colorCommand(const char* bytes, std::size_t length, LedColor& color)
{
    if (length != 2 || bytes[0] != 'K' || !isLedColorCode(static_cast<uint8_t>(bytes[1]))) return false;
    color = static_cast<LedColor>(bytes[1]);
    return true;
}

// Spec 03 §3: a missing or unknown record restores White.
inline LedColor restoredLedColor(bool found, uint8_t stored)
{
    return found && isLedColorCode(stored) ? static_cast<LedColor>(stored) : LED_COLOR_DEFAULT;
}

#endif
