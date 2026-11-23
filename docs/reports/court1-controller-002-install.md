# Court 1: court-controller-002 installed wirelessly

Court 1 (`3c:8a:1f:a1:ba:28`) authenticated after reboot as `court-controller-002` on `ota_1`, `VALID`, confirmed and healthy. Running image SHA-256: `1707f73e2de98ab92d69538ce01ffd7c6dc16e2e47a77f4c660df35856992ffd`.

- The verified signed package matched SHA-256 `3cc11b2815cd54ba3a9b0aa26569ccc5539bbfdd5164bdf7f27479fbbb7ca2fe`. Immediately before writing, the authenticated source matched `doubles-serve-001` on `ota_0`, VALID/confirmed/healthy, SHA-256 `aee46f3d25e4069fec3bde571cb2399194cbb49f5176e7dfed5c554f9eca101b`. Independent admission sampled 61,188 B current heap and a 38,900 B largest internal block.
- One upload and one install POST. The board verified flash readback of the full 1,592,272-byte image before install. The approved app write range was `[0x200000, 0x384BD0)`, sector erase range `[0x200000, 0x385000)`, plus normal OTA metadata updates. No raw flash dump was taken. `ota_0` was not targeted and remains the expected rollback image.
- Both source and candidate sessions used DHCP on the pinned AP. The static fallback was available but not needed. Maintenance close was acknowledged, Windows observed the link drop, and pinned BLE initial sync passed. Windows Wi-Fi and Ethernet were restored; Ethernet TLS worked before, during and after the update.
- Post-boot authenticated maintenance reported 61,084 B current heap, 49,436 B minimum heap, and a 38,900 B largest internal block. This is a maintenance-mode sample, not a measurement of Undo history capacity during play.

No scoring commands, additional reboot, USB flashing or wiring changes. Singles, Doubles, Undo and physical display acceptance are left to the owner. This one-write authorization is consumed.

Local evidence: `20261120-pi-court1-authorized-ota/` in the session artifacts, especially `deploy-1/result.json` and `witness-1.json`.
