#ifndef PICKLESCORE_OTA_MAINTENANCE_H
#define PICKLESCORE_OTA_MAINTENANCE_H

// Spec 04: only the opt-in OTA profiles call this small lifecycle interface.
void prepareOtaBootValidation();
bool startOtaMaintenance();
void pollOtaMaintenance();
void failOtaBootValidation();

#endif
