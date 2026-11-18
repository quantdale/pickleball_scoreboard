#ifndef PICKLESCORE_BLE_ACTIVATION_H
#define PICKLESCORE_BLE_ACTIVATION_H
#include <cstdint>
class BLEService;
// Spec04 §8. Callbacks only authorize/queue; loop owns AP lifecycle.
void addBleActivationCharacteristic(BLEService* service);
void bleActivationConnected();
void bleActivationDisconnected(uint32_t activity);
void cancelBleActivation();
bool takeBleActivation(uint32_t activity);
#endif
