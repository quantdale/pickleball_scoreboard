#ifndef P10_COLOR_ISOLATION_H
#define P10_COLOR_ISOLATION_H

#include <stdint.h>

namespace colorIsolation {
// Spec 03 §2: raw 64x16, eight addresses, two RGB banks per address.
// Identical panel-local masks, but stagger the four 8-sample groups and the
// two RGB banks. A one-step phase change walks each physical LED through
// both ends of a lit run without increasing the number of lit positions.
constexpr bool lit(int rawX, int rawY, int phase)
{
    return (rawX + 3 * ((rawX % 32) / 8) + rawY + 4 * (rawY / 8) + phase) % 8 < 2;
}

struct Frame {
    int phase = 0;
    char color = 'k';
    char block = 'a'; // All, near (raw x32..63), far (raw x0..31).

    bool select(int command)
    {
        if (command >= '0' && command <= '7') {
            phase = command - '0';
            return true;
        }
        if (command == 'a' || command == 'n' || command == 'f') {
            block = static_cast<char>(command);
            return true;
        }
        if (command != 'k' && command != 'w' && command != 'r' &&
            command != 'g' && command != 'b') return false;
        color = static_cast<char>(command);
        return true;
    }

    uint16_t pixel(int rawX, int rawY) const
    {
        if ((block == 'n' && rawX < 32) || (block == 'f' && rawX >= 32) ||
            !lit(rawX, rawY, phase)) return 0;
        if (color == 'w') return 0xFFFF;
        if (color == 'r') return 0xF800;
        if (color == 'g') return 0x07E0;
        if (color == 'b') return 0x001F;
        return 0;
    }
};
}

#endif
