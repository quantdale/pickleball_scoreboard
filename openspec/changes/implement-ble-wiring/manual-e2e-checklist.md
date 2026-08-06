# Manual End-to-End Test Checklist: BLE Wiring

Perform these steps with a real ESP32 board and Android phone. Automated
end-to-end BLE testing is not practical in this repo, so this checklist is the
final verification that the Spec 02 protocol works across real hardware.

## Setup

- [ ] ESP32 is wired to the HUB75 panel and powered from a reliable USB source.
- [ ] Android phone has Bluetooth enabled and is within a few meters of the board.
- [ ] Phone location services are enabled (required for BLE scan on some Android versions).
- [ ] Firmware flashed with the latest build (includes the singles-mode protocol amendment: three-byte reset and six-field state payload).
- [ ] Android app installed from the latest debug APK.

Note: the firmware does not log received commands or state payloads to the
serial monitor. Verify notified payloads by subscribing to the State
characteristic with the BLE scanner app (nRF Connect), or by the state shown in
the app preview; use the serial monitor only for boot and error checks.

## 1. Boot and advertising

- [ ] Open a serial monitor for the ESP32 at 115200 baud.
- [ ] Reset the ESP32 and confirm it boots without errors.
- [ ] Use a BLE scanner app (e.g., nRF Connect) on the phone to verify a peripheral named `"PickleScore"` is advertising service UUID `4fafc201-1fb5-459e-8fcc-c5c9c331914b`.
- [ ] Confirm only one `"PickleScore"` peripheral is visible at a time (single connection for v1).

## 2. App permissions and scan

- [ ] Launch the Android app.
- [ ] Tap **"Scan for PickleScore"**.
- [ ] Grant BLE permissions when prompted (BLUETOOTH_SCAN / BLUETOOTH_CONNECT on Android 12+, or LOCATION / BLUETOOTH on older versions).
- [ ] Confirm the app lists the `"PickleScore"` peripheral in the scan results.

## 3. Connect and initial sync

- [ ] Tap the `"PickleScore"` device row to connect.
- [ ] Confirm the app shows **"Connected"** and the scoreboard preview shows the initial state `(0,0,L,2)`.
- [ ] Confirm the serial monitor shows no errors during service discovery or CCCD write.

## 4. Rally commands

- [ ] Tap **"Rally Left"**.
- [ ] Confirm the app preview updates to `(1,0,L,2)`.
- [ ] Confirm the notified payload is `"1,0,L,2,0,D"` (doubles mode).
- [ ] Tap **"Rally Right"** twice.
- [ ] Confirm the app preview updates to `(1,0,R,2)` after the first tap (side-out) and to `(1,1,R,2)` after the second tap (score up).
- [ ] Confirm the corresponding payloads notify, ending in `,D`.

## 5. Switch courts

- [ ] Tap **"Switch Courts"**.
- [ ] Confirm the app preview swaps the scores and serving side, e.g. from `(1,1,R,2)` to `(1,1,L,2)`.
- [ ] Confirm the notified payload is `"1,1,L,2,0,D"`.

## 6. Undo

- [ ] Tap **"Undo"**.
- [ ] Confirm the app preview reverts one step, e.g. from `(1,1,L,2)` back to `(1,1,R,2)`.
- [ ] Tap **"Undo"** again immediately.
- [ ] Confirm nothing changes and no new notify appears (Spec 01 Section 5a: you cannot undo an undo).

## 7. End game and reset

- [ ] Tap **"End Game"**.
- [ ] Confirm the app preview shows `gameEnded = true` (no visible change other than the flag, but the state payload's fifth field is `1`).
- [ ] Tap **"Rally Left"**, **"Rally Right"**, and **"Switch Courts"**.
- [ ] Confirm none of these change the score or serving side while ended.
- [ ] Tap **"Reset"**.
- [ ] Confirm the `ResetSideDialog` asks for the game mode (**"Singles or doubles?"**) and the 0-0-2 side (**"Which side is 0-0-2?"**).
- [ ] Choose **"Doubles"** and **"Right"**.
- [ ] Confirm the app preview updates to `(0,0,R,2)` and the notified payload is `"0,0,R,2,0,D"` (three-byte reset `0RD`).
- [ ] Tap **"Reset"**, choose **"Singles"** and **"Left"**, and confirm the preview updates to `(0,0,L,2)` with the notified payload `"0,0,L,2,0,S"` (three-byte reset `0LS`).

## 8. Reconnect sync

- [ ] Force-close the Android app (swipe away).
- [ ] On the ESP32, change the score a few times using the app on another phone, or leave it as-is.
- [ ] Reopen the Android app and reconnect to `"PickleScore"`.
- [ ] Confirm the app preview immediately shows the current ESP32 state (not the default `0,0,L,2`) — this verifies Spec 02 Section 6's connect-time notify.

## 9. Disconnect behavior

- [ ] While connected, tap **"Rally Left"** and confirm the score updates.
- [ ] Turn off Bluetooth on the phone or walk out of range.
- [ ] Wait for the app to show **"Disconnected"**.
- [ ] Look at the physical board and confirm the score remains displayed.
- [ ] Confirm the ESP32 continues running and advertising; the last score is not lost.
- [ ] Reconnect and confirm the state is still the last score.

## 10. Error handling

- [ ] Using a BLE scanner app or custom script, write an unrecognized byte (e.g., `'X'`) to the Command characteristic.
- [ ] Confirm the ESP32 does not crash and does not send a state Notify.
- [ ] Write byte `'0'` alone, or `'0'` followed by a side byte without a mode byte (the old two-byte reset format).
- [ ] Confirm the reset does not execute and no Notify is sent.

## Expected final state

- Firmware compiles and runs on ESP32.
- Android app builds and installs.
- All checklist steps above pass.
- No crashes or exceptions in serial monitor or Android logcat.
