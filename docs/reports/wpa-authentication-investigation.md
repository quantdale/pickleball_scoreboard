# SoftAP authentication investigation — candidate awaiting isolated-USB validation

## Status

**The failure is reproduced and localized; the actual WPA root cause is not yet physically proven.** A concrete pinned-SDK configuration mismatch has a minimal candidate correction, regression test and signed build. No firmware installation, serial reset, provisioning, partition change or successful wireless update occurred in this investigation. The owner has been asked for fresh explicit panel/HUB75/external5V isolation confirmation before USB operations; subsequent “resume” messages were not treated as electrical confirmation.

Current branch: `feat/agent-wireless-deployment`, starting from `9f2dd99`. Last physically verified application remains `agent-v2-a` on `ota_0`; this investigation did not reset/read flash to re-identify that application. The real base/AP/BLE identities remain pinned by the existing controller configuration and fresh BLE/RSN observations.

## Evidence captured before changes

Private session directory:

```text
D:\Documents\tryPython\_AgentArtifacts\pickleball_scoreboard\sessions\20261117-pi-wpa-root-cause\
```

Read-only evidence includes all requested `netsh wlan show drivers`, `interfaces`, `networks mode=bssid`, `profiles`, Windows WLAN AutoConfig XML events, adapter/advanced properties and network-adapter inventory. `netsh wlan show wlanreport` was attempted and denied because this process is not elevated; the denial is preserved. No elevation bypass, profile reset, driver/settings change or credential export was attempted.

- Wi-Fi: Killer AX1650i/Intel201NGW, driver **23.150.0.4**, date2025-06-12, INFoem185.inf, interface15.
- Driver advertises WPA2-Personal/CCMP,802.11b/g/n and PMF support.
- Ethernet interface7 remains online; explicit source-bound, certificate-verifying TLS probes pass before/after wireless reproduction.
- Only one physical WLAN adapter is present. ADB reports no independent Android devices. No independent-client pass is claimed.

## Exact failure stage

`reproduce_with_rsn.py` uses existing authenticated BLE maintenance activation, then the existing Native WLAN connection code. It does **not** open USB, upload firmware or change ESP32 settings. It records timestamped native connection states, current target BSSID and actual beacon/probe information elements via `WlanGetNetworkBssList`, then restores networking.

Observed red result: one association-state5 sample followed by29 state7 (“authenticating”) samples; no state1 (“connected”). Target BSSID **3c:8a:1f:a1:ba:29**, RSSI **−34dBm**, channel1/2412MHz, beacon interval **100TU**. Windows interface output reports WPA2-Personal/CCMP and authenticating, not a connected network awaiting HTTP.

WLAN events record successful association11001, security startup11010, repeated security stop/restart11004/11010 and eventual operator-bounded cancellation8002. Historical events contain24 successful associations and126 security starts, but no successful security/connection completion for this target. Cancellation reason229377 is the tool ending its bounded attempt, not proof that the AP supplied a particular WPA rejection code. No precise failing four-way-handshake message/reason is invented from these events.

Actual RSN IE:

```text
01 00 00 0f ac 04 01 00 00 0f ac 04 01 00 00 0f ac 02 00 00
```

Decoded: RSN version1, group cipherCCMP4, one pairwise cipherCCMP4, one AKMPSK2, RSN capabilities0 (PMF not required/advertised). This rules out the earlier event text's GCMP label as reliable evidence of the on-air cipher: netsh's current selected suite and actual RSN both showCCMP. HT IEs are preserved and indicate802.11n/HT40 advertisement. No changes to HT/bandwidth were bundled with the current candidate.

**Conclusion supported by evidence:** association succeeds; wireless security does not complete, so this is before DHCP/IP/HTTP. It is not labelled a DHCP or HTTP defect. Current captures do not independently prove which four-way message fails or establish Windows-vs-ESP32 causality.

## Concrete SDK mismatch and smallest candidate

Actual installed framework files, not assumed newer APIs:

- Arduino2.0.17 `libraries/WiFi/src/WiFiGeneric.cpp` dynamic-buffer initialization sets `tx_buf_type=1`, `static_tx_buf_num=0`, `cache_tx_buf_num=4` with the explicit comment **“can't be zero!”**.
- Its ESP-IDF4.4 `esp_wifi.h` defines `CONFIG_FEATURE_CACHE_TX_BUF_BIT` as bit1 and initializes `feature_caps` from `g_wifi_feature_caps`.
- The preserved installed `agent-v2-a.elf` contains **`g_wifi_feature_caps=0x03`**, including the TX-cache bit.
- Production OTA code instead sets dynamic TX mode with **`cache_tx_buf_num=0`**, leaving the cache feature enabled.

The candidate changes only that zero count to the framework-compatible bounded value **4**, through a named constant shared with a host regression test. No WPA/cipher/PMF/PSK/maintenance/signing/partition/scoring/display changes, no CLI rewrite, and no heap-threshold reduction. It is the leading SDK configuration defect to test, **not yet claimed as the proven cause of authentication or shutdown warnings**.

A red host run with value0 fails `test_pinned_sdk_dynamic_tx_cache_is_nonzero_and_bounded`; the corrected value4 passes. The real red/green hardware loop still needs to run against the corrected installed image before root-cause closure.

## Candidate verification

Actual command:

```text
firmware/.venv/Scripts/python.exe tools/ota_deploy.py build --device court1 --build-id agent-wpa1 --output <session>/candidate-build
```

Results:78 firmware Unity tests,29 CLI tests, glyph/layout checks and48 C++ pixel snapshots pass; the real PlatformIO application builds and owner-key signing plus independent package verification pass.15 OpenSSL/signing regression tests also pass. No Android/scoring/rendering code changed; fresh42 Android tests passed in the preceding commissioning session, not falsely represented as rerun here. Active C++ LSP probes timed out; successful compilation/tests are the verification evidence, not a clean LSP claim.

Candidate image **1,587,696 bytes**, SHA256 **0185d1b46e16cc69f9f6e646e3f03d193b99154cafbc9fb395fdef544b535bb7**. Package SHA256 **8fb4bb6ae1eac4d7a5ec39b8d4eccee77a752713cc004ff1c8131e55072d1a0d**. Files are outside Git at `candidate-build/firmware.bin`, `firmware.elf`, `agent-wpa1.pksota`. Existing owner secrets/signing key are reused, not regenerated or exposed.

Runtime heap/AP startup, connection reliability and TX-cache effect cannot be inferred from build success; original memory admission checks remain intact.

## Safe next steps

1. Obtain current explicit isolation confirmation: panels unpowered/discharged, HUB75 disconnected while unpowered, external5V absent, ESP32 laptopUSB-only. Confirm application-only install authorization if not already explicit for the resumed operation.
2. Re-identify original MAC/revision/4MB/security state and verify a complete current recovery image before a targeted application-only installation. Stop on mismatch. Do not repeat migration/bootloader/provisioning or full erase.
3. Supervise candidate startup, heap and exact config; rerun the same RSN/connection loop. Require state1, correct BSSID/on-link WLAN route, real API identity/authentication and Ethernet internet.
4. Only after connection succeeds, perform the two explicitly authorized signed wireless updates through the existing CLI, verifying opposite slot/exact image/VALID confirmed boot and network restoration. Do not use USB as a hidden substitute for those tests.
5. Recheck repeated BLE activation/scoring/idle shutdown and the SDK type4/5 warning. Preserve failure evidence if the candidate does not resolve WPA; pursue independent-client or targeted on-air evidence rather than guessing or weakening security.

No end-to-end OTA success, resolved shutdown warning, physical rollback or illuminated-panel acceptance is claimed by this report.
