# Controlled USB OTA installation: actual hardware evidence

## Outcome

**The approved OTA partition migration, initial OTA-A/fallback installation and private provisioning are complete. USB-only firmware startup is verified. Wireless OTA and automatic rollback have NOT been physically exercised.** No panel reconnection/power-on was performed by the agent. Do not seal the enclosure or infer full acceptance from this installation.

Installed firmware is the reviewed `a118c84`/implementation `5f748c1` build, **`ota-v1-a`**, application SHA256 `dceb86d456e40da132cefaf5a0e9e4baa9304c92d2b3d5e80886b74016209682`,1,583,904 bytes at0x10000 (`ota_0`). Documentation follow-up does not alter that firmware.

## Authorization and identity

The owner explicitly approved the documented partition/metadata/application/fallback migration and confirmed panels unpowered, HUB75 disconnected while unpowered, ESP32 exclusively laptop-USB-powered and no external5V. Retry/resume retained that isolated setup. All flash writes occurred only after the full backup and remaining preflight checks passed.

Fresh identity: **COM11**, CH340 VID:PID1A86:7523/location1-1; **ESP32-D0WD-V3 revision3.1**, MAC **`3c:8a:1f:a1:ba:28`**,40MHz crystal, detected **4MB** flash. Read-only security inspection showed Secure Boot V1/V2 unset, flash encryption disabled and crypt count0. These remain unchanged after installation. No eFuses, security overrides or irreversible settings were written.

## Full backup and USB recovery

The default esptool full-read stream repeatedly returned short frames at115200 baud. A conservative read-only adapter used the same installed stub protocol at57600 baud,1024-byte frames and one inflight block. It did not weaken integrity checks: a64KB probe matched, the full4,194,304-byte stream passed the stub MD5, and a separate device flash MD5 agreed. The ordinary esptool full-image `verify_flash` then independently returned **verify OK (digest matched)** before migration.

- Full backup SHA256: **`707031c171e1efb59492ed1f4c861628956f8a65fbbacb1de490b9506ebaa60d`**.
- MD5: `6ed53af2d1045a0db476352f8c0f04d8` (transfer/device comparison, not firmware authenticity).
- Original `pre-ota-full-flash.bin` and separate verified `recovery/pre-ota-full-flash.bin` are retained. They are complete4MB images, unlike older partial/application backups.
- Exact bootloader, old partition table and original application in the backup match the preserved working production artifacts. Original NVS/OTA metadata/coredump slices and old component recovery files are also retained.
- Session `USB_RECOVERY.md` describes complete identity/isolation/hash checks, full-image restoration at0x0, independent verification and original serial-boot checks. Recovery is prepared, not destructively exercised. Restoring the full image later requires explicit restoration approval/fresh isolation and discards new credentials/settings.

## Approved writes and independent verification

| Address | Written object | Exact bytes | Sector erase end (exclusive) |
|---|---|---:|---|
|0x8000|Reviewed OTA partition table|3,072|0x9000|
|0xe000|Initial OTA metadata|8,192|0x10000|
|0x10000|OTA-A `ota-v1-a`|1,583,904|0x193000|
|0x200000|Original production fallback|1,156,448|0x31b000|

Each write reported **Hash of data verified**. A separate esptool operation independently verified all four ranges, and the final post-provisioning check verified them again. Targeted sector erasures are precisely the documented migration; no erase-all was used. The new table has two0x1f0000-byte application slots and ends at4MB; NVS0x9000/0x5000 and coredump0x3f0000/0x10000 remain at their original addresses.

**No bootloader replacement was necessary or performed**: actual device bytes exactly matched the inspected rollback-capable bootloader. Bootloader/NVS/coredump were independently verified unchanged immediately after migration. Provisioning subsequently wrote only its intentional transactional NVS blob. Final digest checks also proved all other sectors outside approved write/credential ranges match the full backup (0x0..0x8000,0x193000..0x200000,0x31b000..0x400000), including the original bootloader/coredump.

The fallback at0x200000 exactly retains production SHA256 `14e77d5d48aee05652276a2757277439e89dff6d92614b76a0882294638228ae`. It boots scoring but is not OTA-capable. The first wireless update overwrites this inactive fallback while preserving running OTA-A, as designed.

## Signing trust and provisioning

A new unique production AP password, independent maintenance secret and P-256 signing key were generated outside Git. Files are ACL-restricted to the current owner and SYSTEM. Private contents were never printed, included in source/Git archives or stored in public update packages. A temporary ACL-readability problem was corrected using direct owner/SYSTEM file permissions before preflight passed; no trust/credential replacement was needed.

- SSID: **`PickleScore-Court1`**.
- Public-point SHA256 fingerprint: `db5853d81d7037492fa9fb09fe66707cd0c34cd8fcf0ba10dcfef0a2367b04b4`.
- Offline verification proved the private key, public PEM and164-byte provisioning record contain matching trust; the record's admin hash/SSID/PSK match the private owner file.
- USB provisioning returned **`OTA PROVISIONED; reboot manually to open maintenance AP`**.
- Final read-only NVS readback contains the **exact** provisioned record; that sensitive readback is kept only in the protected private directory, not public archives.
- Signed real A/B packages were generated with that production key and independently verified by the offline tool. No disposable TEST ONLY key was used on the real device.

## Observed startup, memory and boot state

Unprovisioned first boot correctly reported no private provisioning and left maintenance disabled while BLE/scoring initialized; free heap113,176 bytes. The expected NVS namespace-not-found diagnostic disappeared after provisioning.

Two separate200-second provisioned boot captures reported:

```text
OTA build=ota-v1-a ABI=PKSCORE-OTA-ABI1 slot=ota_0 pending=0
DISPLAY ready 64x16, chain=2, brightness=12/255
BLE advertising PickleScore; state owner=Arduino loop
OTA AP ready ssid=PickleScore-Court1 address=192.168.4.1 window=180s ...
SCOREBOARD ready state=0,0,L,2,0,D ...
```

| Observation | First capture | Repeat capture |
|---|---:|---:|
|AP-start free heap|59,808|59,760|
|Scoreboard-ready free heap|59,732|59,684|
|Free heap reported after idle maintenance closure|104,092|107,192|
|Minimum heap reported at closure|59,032|59,240|
|Idle closure time after controlled reset|~182.02s|~182.02s|

There was exactly one intentional reset per capture and no unexpected reset, panic, assertion, brownout or watchdog. This is idle USB-only evidence, not transfer-load/fragmentation stability or physical light/RF proof. All serial operations ended with **COM11 released**; the final AP window has expired and needs a normal power-cycle for owner testing.

Actual OTA metadata readback: sector0xe000 has sequence1, state0xffffffff (UNDEFINED), CRC0x4743989a; the other sector has sequence0/state0xffffffff. This selects `ota_0` and agrees with `pending=0`. **The initial USB installation is not a pending OTA trial**, so no `OTA boot confirmed` message is expected yet. The reviewed strong delayed-confirmation override and rollback-enabled bootloader are installed/unchanged, but NEW→PENDING_VERIFY→VALID behavior and rollback will only be proven by later controlled wireless/recovery tests.

## Outstanding SDK shutdown warning — do not omit

Both captures emitted this error-level SDK line immediately after the application reported idle maintenance closure:

```text
wifi:timeout when WiFi un-init, type=4
```

The capture classifier now records it separately and reports `cleanDiagnostics=false`; successful startup must not be represented as an entirely clean log. The board did not reboot and substantial heap returned. The closure log does not prove every driver resource/return code/radio shutdown completed perfectly. Similar SDK shutdown reports exist externally, but the exact cause/effect on this firmware is **not established**. No speculative timing/driver/source change, warning suppression or unreviewed reflash was performed. Before sealing, exercise AP timeout/Exit plus continuing BLE control and record whether this warning affects operation; if necessary diagnose with a narrowly instrumented, separately reviewed firmware update. This remains a commissioning finding, not waived acceptance.

## Artifacts and next physical tests

Session root:

```text
D:\Documents\tryPython\_AgentArtifacts\pickleball_scoreboard\sessions\20261116-pi-ota-install-retry\
```

- `preflight-passed.json`, full-backup verification/read/write logs, `migration-independent-verify.log`, `preserved-regions-verify.log`, `postinstall-readonly-verification.json`, exact readback `postinstall-otadata.bin`.
- `initial-unprovisioned-*`, `provisioned-boot-*`, `provisioned-repeat-*`, including raw/timestamped serial logs.
- `release/` contains the frozen approved images/recovery set; `signed-updates/` contains the owner-key-signed `.pksota` files.
- **Private** directory `private-owner-court1/`: open `owner-credentials.json` locally to obtain `apPassword` and `maintenanceKey`. Do not paste them into chat or put them in Git. Back up this folder securely before sealing; it contains the only new signing private key.

Next, with panels/HUB75 still isolated, close/disable the Android scoring connection and normally power-cycle the USB board to reopen its180-second window. Connect the laptop to **PickleScore-Court1** using `apPassword`; stay connected despite “no internet”. Open **http://192.168.4.1**, sign in with `maintenanceKey`, and require `ota-v1-a`/`ota_0` on the page. Do not change panel wiring live.

For controlled later wireless acceptance use `signed-updates/PickleScore-ota-v1-b.pksota`, explicitly install and require `ota-v1-b`/`ota_1`; then use `PickleScore-ota-v1-a.pksota` and require `ota-v1-a`/`ota_0`, without USB flashing. Follow the [full browser/recovery procedure](../ota-maintenance.md#two-consecutive-wireless-updates--owner-acceptance), including rejection/interruption and the unresolved shutdown warning. No wireless update or rollback is claimed performed in this installation session.

Before checking both panels again, unplug USB and keep panel supply disconnected/discharged; reconnect accepted HUB75/common-ground/regulated5V wiring only while everything is unpowered, then restore the accepted power arrangement. Never apply upstream12V directly. Real OTA/panel/BLE coexistence and sustained transfer-load memory behavior remain owner-observed acceptance gates.
