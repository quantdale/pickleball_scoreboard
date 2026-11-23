#ifndef DISPLAY_SETTINGS_STORE_H
#define DISPLAY_SETTINGS_STORE_H

#include "display_settings.h"

// Spec 03 §§2a/3: one byte per setting in NVS namespace "pksdisp", apart from
// OTA credentials. Load never fails: a missing or invalid record restores its default.
DisplaySettings loadDisplaySettings();
// Writes only the settings that differ from storage.
bool storeDisplaySettings(const DisplaySettings& settings);

#endif
