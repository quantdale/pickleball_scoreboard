// Spec 03 §2a: persisted operator brightness. initArduino() has already
// initialized NVS; Preferences holds its handle only for the call.
#include "brightness_store.h"
#include "display_brightness.h"
#include <Preferences.h>

namespace {
constexpr char NAMESPACE[] = "pksdisp";
constexpr char KEY[] = "level";
}

uint8_t loadBrightnessLevel()
{
    Preferences storage;
    if (!storage.begin(NAMESPACE, true)) return restoredBrightnessLevel(false, 0);
    const bool found = storage.isKey(KEY);
    const uint8_t stored = found ? storage.getUChar(KEY, 0) : 0;
    storage.end();
    return restoredBrightnessLevel(found, stored);
}

bool storeBrightnessLevel(uint8_t level)
{
    Preferences storage;
    if (!storage.begin(NAMESPACE, false)) return false;
    const bool ok = storage.putUChar(KEY, level) == 1;
    storage.end();
    return ok;
}
