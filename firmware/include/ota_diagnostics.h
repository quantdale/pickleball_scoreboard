#ifndef PICKLESCORE_OTA_DIAGNOSTICS_H
#define PICKLESCORE_OTA_DIAGNOSTICS_H
// Spec04 Sections3–5/8: bench observation only, absent from release OTA profiles.
namespace ota { namespace diag {
#ifdef PICKLE_OTA_DIAGNOSTICS
void startOwnership();
void ownershipCheckpoint(const char* stage);
bool budgetBegin(const char* stage);
void budgetPoint(const char* stage);
void budgetEnd(const char* stage);
#else
inline void startOwnership() {}
inline void ownershipCheckpoint(const char*) {}
inline bool budgetBegin(const char*) { return false; }
inline void budgetPoint(const char*) {}
inline void budgetEnd(const char*) {}
#endif

// Emit matching checkpoints around a single operation; ESP32 watermark belongs to the calling task.
struct Budget {
    const char* stage;
    bool ownsSample;
    explicit Budget(const char* name) { stage = name; ownsSample = budgetBegin(name); }
    ~Budget() { if (ownsSample) budgetEnd(stage); }
    Budget(const Budget&) = delete;
    Budget& operator=(const Budget&) = delete;
};
}}
#endif
