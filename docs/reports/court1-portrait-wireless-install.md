# Court 1 portrait OTA — install accepted, running image unverified

**Stop: do not repeat deployment.** On 2026-11-19 the single authorized attempt
staged `portrait-cw-001` in `ota_1` with device-verified readback. Court 1
acknowledged the one install request. The host stopped before independently
verifying what booted. Do not treat the install acknowledgment as a confirmed boot.

| Checkpoint | Observation |
| --- | --- |
| Authorization and recovery | Owner confirmed idle court, Android disconnected, panels unpowered/discharged, HUB75 disconnected while unpowered, ESP32 USB-only with no external positive 5 V, and physical recovery access. Candidate and archived recovery signatures checked with registered Court 1 trust; two protected historical 4 MiB backups rehashed; 38 offline CLI tests passed. |
| BLE and source AP | One maintenance proof bound to base MAC `3c:8a:1f:a1:ba:28` was acknowledged. A fresh pinned `3c:8a:1f:a1:ba:29` BSSID was observed 33.056 s after BLE disconnect, followed by one WPA2/CCMP association and on-link DHCP. |
| Live source and memory | After authenticated login, `/api/info` reported `court1-return-002`, `ota_0`, exact SHA-256 `077d888af21625335b6f3790b6b179928f2c1852c0f42b3444e3c0688917a00e`, `valid`, `confirmed=true`, `healthy=true`. Idle session reported 62,848 B current heap versus the 49,152 B operation floor; authenticated info reported 62,844 B. Firmware enforced pre-erase current/contiguous-memory admission; no exact contiguous size or stack watermark was captured. |
| One upload | Package SHA-256 `1864b8a42767f8b70f06472a6c3723fe26e2625d037e477fac8d43c7f7104f81`; application SHA-256 `b039c3ca6e6e0d4a41077facb21802698c91de5f0e6b477d8ddf164ccadee5dd`. Upload response arrived after 25.996 s; device readback verified all 1,590,736 bytes. Application write scope was `ota_1` payload `[0x200000,0x3845D0)`, erase extent `[0x200000,0x385000)`. |
| One install | Install acknowledged at about 06:26:03.961 UTC. `ota_0` was not targeted. Normal OTA boot metadata may have changed; it was not independently read. |
| Post-install stop | The guard required the old Windows AP association to report disconnected before treating a later connection as fresh. It did not observe a disconnected link and stopped with `old-ap-link-did-not-drop`. Windows logged security stop at 06:26:06.380 and security restart/success at 06:26:13.886/13.905 for the same SSID. This shows why a clean observed link break cannot be inferred from polling, but does not prove a reboot or establish build, slot, image hash, or health. There was no post-install HTTP identity check or BLE recovery check. |
| Host restoration | Windows Wi-Fi restored to its original disconnected state; temporary profile removed and radio state restored. Source-bound Ethernet TLS passed before, during, and after. |

No second upload/install, no serial access, USB fallback, forced reboot,
rollback, recovery write, or physical change. The authorization is spent. Before
mounting or physical acceptance, obtain separate authorization for one no-write
verification of the actual running image: authenticate, verify exact build/hash,
slot and `VALID`/confirmed/healthy, close maintenance, and check BLE recovery.
Do not force a reboot to make the transition observable.

Evidence outside Git: `20261119-pi-court1-portrait-live/`, including
`cli-deploy/result.json`, `guard-result.json`, `console.log`,
`source-network/`, and `wlan-post-install-events.json`.
