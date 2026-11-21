// Spec 03 §§2a/3: persisted operator display settings. initArduino() has already
// initialized NVS; Preferences holds its handle only for the call.
#include "display_settings_store.h"
#include <Preferences.h>

namespace {
constexpr char NAMESPACE[] = "pksdisp";
constexpr char LEVEL_KEY[] = "level";
constexpr char COLOR_KEY[] = "color";

bool readByte(Preferences& storage, const char* key, uint8_t& value)
{
    if (!storage.isKey(key)) return false;
    value = storage.getUChar(key, 0);
    return true;
}
}

DisplaySettings loadDisplaySettings()
{
    Preferences storage;
    uint8_t level = 0, color = 0;
    bool levelFound = false, colorFound = false;
    if (storage.begin(NAMESPACE, true)) {
        levelFound = readByte(storage, LEVEL_KEY, level);
        colorFound = readByte(storage, COLOR_KEY, color);
        storage.end();
    }
    return DisplaySettings(restoredBrightnessLevel(levelFound, level), restoredLedColor(colorFound, color));
}

bool storeDisplaySettings(const DisplaySettings& settings)
{
    Preferences storage;
    if (!storage.begin(NAMESPACE, false)) return false;
    bool ok = true;
    if (settings.levelUnsaved()) ok = storage.putUChar(LEVEL_KEY, settings.level()) == 1;
    if (ok && settings.colorUnsaved()) ok = storage.putUChar(COLOR_KEY, static_cast<uint8_t>(settings.color())) == 1;
    storage.end();
    return ok;
}
