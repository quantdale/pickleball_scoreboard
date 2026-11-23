# Court 1 installed wireless panel-order OTA — 2026-11-19

**Pass:** Court 1 (`3c:8a:1f:a1:ba:28`) is running `portrait-order-004` on `ota_1`, SHA-256 `683136620020b4f5f9c7706800b34fce1e0dab0ad8e7df17c8182dc86a45ee35`, `VALID`, confirmed and healthy. Illuminated acceptance is pending an owner photo.

- With owner-confirmed idle court, Android disconnected, stable installed power and safe recovery access, one no-write session verified a preferred DHCP lease `192.168.4.2/24` on Windows WLAN interface 15, the pinned on-link route, TCP port 80, and the exact `portrait-r180-002` digest on `ota_0` with confirmed valid health. Maintenance closed, BLE initial sync passed, and host networking was restored. This does not establish the cause of the [earlier timeout](court1-windows-dhcp-route.md).
- The signed package matched SHA-256 `fbd748f4c3a8143dde28d8dfdb9345d082f17a1b4cf29ddab83a1de73c5686a6`. The CLI reauthenticated the live source with 62,492 bytes of pre-upload heap. One upload and one install; the device staged and read back all 1,590,816 image bytes. Authorized inactive app erase `[0x200000, 0x385000)`, app write `[0x200000, 0x384620)`, and normal OTA metadata updates in `[0xE000, 0x10000)`; metadata was not independently read. `ota_0` was not targeted and remains the expected trial fallback.
- After the normal reboot, a separate authenticated HTTP read confirmed the exact new digest, build, slot and health. Close was acknowledged and Windows observed the pinned link drop before restoring its previous Wi-Fi state. BLE initial sync and normal disconnect passed; Ethernet TLS was verified before, during and after. No independent RF shutdown measurement.
- Both panels stayed connected on their normal installed supplies. No USB/serial, forced reset, retry, rollback, rollback exercise or scoring writes. No firmware rebuild. The one-time authorization is consumed.

Owner photo requested: `0 / 0 / 2 / ←`, top to bottom, both zeros above the visible seam. Do not run the asymmetric test without separate approval.

Local evidence: `20261119-pi-court1-portrait-order-ota/verify/result.json`, `deploy/result.json`, `witness.json`, and `package-hashes.log`.
