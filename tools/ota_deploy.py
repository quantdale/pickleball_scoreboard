"""Explicit signed deployment, Spec04 §8. Windows Python owns physical adapters.

Build and verify a package offline, then deploy the exact owner-approved bytes
from the exact approved source. See docs/agent-ota-deployment.md.
"""

import argparse
import asyncio
import contextlib
import hashlib
import hmac
import http.client
import importlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
ota_tool = importlib.import_module("ota_tool")

MAINTENANCE_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26aa"
STATE_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a9"
DOMAIN = b"PKS-MAINT-1\0"
# Spec 04 §8: early operator-side checks; firmware rechecks at upload and erase.
OPERATION_FREE_HEAP = 49152
FLASH_SECTOR_BYTES = 4096
EMERGENCY_FREE_HEAP = 32768
# Spec 04 §8: measured upload demand (docs/reports/ota-2.2-reliability.md). About
# 7.5 KB is consumed between the idle session and the on-device flash-begin
# check (signature, first body bytes), and the body dipped up to 24,616 B below
# the session heap. Refuse before any upload unless both firmware floors would
# still hold with a 3 KiB margin. This adds a host gate; it never lowers a floor.
UPLOAD_PREGATE_DEMAND = 7680
UPLOAD_BODY_DEMAND = 24616
UPLOAD_MARGIN = 3072
UPLOAD_SESSION_HEAP = max(
    OPERATION_FREE_HEAP + UPLOAD_PREGATE_DEMAND, EMERGENCY_FREE_HEAP + UPLOAD_BODY_DEMAND
) + UPLOAD_MARGIN
# Spec 04 §4: fixed firmware rejection literals (ota_maintenance.cpp, ota_core.h).
# Only these, or an esp_err_to_name() constant, are persisted; any other body text is dropped.
DEVICE_REASONS = frozenset({
    "authorization/origin/nonce rejected",
    "authorization/origin/nonce/body rejected",
    "BLE control is active",
    "BLE controller/commands active; maintenance refused",
    "boot selection failed; not rebooting",
    "flash readback hash mismatch",
    "flash readback SHA256 failed",
    "flash write failed",
    "header truncated, expired or interrupted",
    "image size outside inactive-slot limit",
    "image/hash validation failed",
    "incompatible scoreboard target",
    "insufficient or fragmented free heap",
    "invalid build identifier",
    "invalid image extent/trailing data",
    "invalid local host/path",
    "invalid login origin/path/body",
    "invalid maintenance key",
    "invalid package format",
    "invalid signature length",
    "invalid/incompatible ESP32 image header",
    "maintenance unavailable or already claimed",
    "no active transfer",
    "no verified image or BLE control active",
    "noncanonical signature padding",
    "OTA begin failed",
    "oversized image",
    "package length mismatch",
    "package outside size limit",
    "partition layout mismatch; authorized USB migration required",
    "raw signed package required",
    "readback SHA256 start failed",
    "received image SHA256 mismatch",
    "running application is not confirmed",
    "scoreboard ABI/build marker mismatch",
    "session expired or BLE activity detected",
    "session unavailable",
    "SHA256 allocation/start failed",
    "SHA256 update failed",
    "transfer truncated, expired or interrupted",
    "truncated image",
    "unsafe inactive partition",
    "untrusted firmware signature",
    "update already active/staged or BLE busy",
    "verification interrupted by expiry/BLE activity",
    "window expired or BLE activity detected",
})


class DeploymentError(RuntimeError):
    def __init__(self, code, message, device_reason=None):
        super().__init__(message)
        self.code = code
        self.device_reason = device_reason


def device_reason(value):
    if isinstance(value, str) and (
        value in DEVICE_REASONS or re.fullmatch(r"ESP_(?:FAIL|ERR_[A-Z0-9_]{1,48})", value)
    ):
        return value
    return None


def require(value, code, message):
    if not value:
        raise DeploymentError(code, message)


def proof(challenge, mac, maintenance_key):
    require(
        len(challenge) == 39
        and challenge[0] == 1
        and challenge[1:7] == bytes.fromhex(mac.replace(":", "")),
        "ble_identity",
        "Wrong BLE challenge target/version",
    )
    key = hashlib.sha256(maintenance_key.encode()).digest()
    return b"\1\1" + hmac.digest(key, DOMAIN + challenge + b"\1", "sha256")


async def find_pinned_ble(bleak, address, active_seconds):
    # Spec 04 §8: use fresh advertisements, never a cached address or device name.
    # After a clean active miss, try one listen-only window before giving up.
    for mode, seconds in (("active", active_seconds), ("passive", 8)):
        target = await bleak.BleakScanner.find_device_by_address(address, timeout=seconds, scanning_mode=mode)
        if target is not None and target.address.casefold() == address.casefold():
            return target
    return None


async def _activate_ble_once(device, owner, cached=True):
    bleak = importlib.import_module("bleak")
    bleak_errors = importlib.import_module("bleak.exc")
    stage = "scan"
    try:
        target = await find_pinned_ble(bleak, device["bleAddress"], active_seconds=15)
        require(
            target is not None,
            "ble_unavailable",
            "Pinned board not seen in active or passive scan; check installed power, range and Board → Disconnect on any scoring controller",
        )
        stage = "connect-services"
        async with bleak.BleakClient(
            target, timeout=30, winrt={"use_cached_services": cached}
        ) as client:
            stage = "challenge-read"
            challenge = bytes(
                await asyncio.wait_for(client.read_gatt_char(MAINTENANCE_UUID), 10)
            )
            stage = "proof-write"
            await asyncio.wait_for(
                client.write_gatt_char(
                    MAINTENANCE_UUID,
                    proof(challenge, device["mac"], owner["maintenanceKey"]),
                    response=True,
                ),
                10,
            )
            stage = "acknowledgement"
            status = bytes(
                await asyncio.wait_for(client.read_gatt_char(MAINTENANCE_UUID), 10)
            )
            require(
                status == b"OK",
                "ble_authorization",
                "Maintenance activation refused (busy/expired/rate limit)",
            )
        # Firmware opens AP only after this authenticated central disconnects.
    except bleak_errors.BleakError as error:
        raise DeploymentError(
            "ble_" + stage,
            "BLE operation failed at " + stage + "; controller may be occupied",
        ) from error
    except TimeoutError as error:
        raise DeploymentError(
            "ble_timeout_" + stage,
            "BLE timed out at " + stage + "; no forced controller disconnection",
        ) from error


async def activate_ble(device, owner):
    # Retry only discovery/read before a proof is submitted. Never replay a proof.
    try:
        await _activate_ble_once(device, owner, cached=True)
    except DeploymentError as error:
        if error.code not in ("ble_timeout_connect-services", "ble_challenge-read"):
            raise
        await asyncio.sleep(3)
        await _activate_ble_once(device, owner, cached=False)


async def verify_ble_recovery(device):
    # Spec 02 §§3, 5: observe the initial Notify after CCCD subscription; no scoring write.
    bleak = importlib.import_module("bleak")
    bleak_errors = importlib.import_module("bleak.exc")

    async def initial_sync():
        target = await find_pinned_ble(bleak, device["bleAddress"], active_seconds=12)
        require(target is not None, "ble_recovery_not_advertising",
                "Pinned BLE device not seen in either recovery scan; advertising recovery is unverified")
        got = asyncio.get_running_loop().create_future()

        def notify(_, data):
            if not got.done():
                got.set_result(bytes(data))

        async with bleak.BleakClient(target, timeout=10) as client:
            await client.start_notify(STATE_UUID, notify)
            state = await asyncio.wait_for(got, 6)
            require(len(state) <= 20 and re.fullmatch(rb"(?:0|[1-9][0-9]*),(?:0|[1-9][0-9]*),[LR],[12],[01],[DS]", state),
                    "ble_recovery_invalid_state", "Initial BLE state was invalid")

    try:
        await asyncio.wait_for(initial_sync(), 40)
    except (TimeoutError, bleak_errors.BleakError) as error:
        raise DeploymentError("ble_recovery_unverified", "Bounded BLE initial sync/disconnect did not complete") from error


class SerialWitness:
    """Optional isolated-USB commissioning only. No flash writes or provisioning."""

    def __init__(self, port, mac, output):
        import serial
        from serial.tools import list_ports

        ports = [
            p
            for p in list_ports.comports()
            if p.device == port and p.vid == 0x1A86 and p.pid == 0x7523
        ]
        require(
            len(ports) == 1, "serial_identity", "Expected CH340 serial port not found"
        )
        packages = Path.home() / ".platformio/packages/tool-esptoolpy"
        sys.path[:0] = [str(packages / "_contrib"), str(packages)]
        esp_module = importlib.import_module("esptool.targets.esp32")
        esp = esp_module.ESP32ROM(port, baud=115200)
        try:
            # esptool chatter is public identity only, keep JSON stdout deterministic.
            with (
                (output / "serial-identity.log").open("x") as log,
                contextlib.redirect_stdout(log),
            ):
                esp.connect()
                actual = ":".join(f"{b:02x}" for b in esp.read_mac())
                require(
                    actual == mac and esp.get_chip_revision() == 301,
                    "serial_identity",
                    "Wrong serial MCU",
                )
                require(
                    not esp.get_secure_boot_enabled()
                    and not esp.get_flash_encryption_enabled(),
                    "serial_security",
                    "Unexpected security configuration",
                )
                esp.flash_spi_attach(0)
                require(
                    1 << ((esp.flash_id() >> 16) & 255) == 4194304,
                    "serial_identity",
                    "Wrong flash capacity",
                )
        finally:
            esp._port.close()
        self.port = serial.Serial(port=None, baudrate=115200, timeout=0.2)
        self.port.port = port
        self.port.dtr = False
        self.port.rts = False
        self.port.open()
        self.raw = bytearray()
        self.stop = threading.Event()
        self.log = (output / "serial-boot.log").open("xb")

        def read():
            while not self.stop.is_set():
                data = self.port.read(1024)
                if data:
                    self.raw.extend(data)
                    self.log.write(data)
                    self.log.flush()

        self.thread = threading.Thread(target=read, daemon=True)
        self.thread.start()

    def reset(self):
        self.port.rts = True
        time.sleep(0.12)
        self.port.rts = False

    def confirmed(self, build, slot):
        text = bytes(self.raw).decode(errors="replace")
        return (
            f"OTA boot confirmed build={build} " in text
            and re.search(
                rf"OTA build={re.escape(build)} ABI=PKSCORE-OTA-ABI1 slot={slot} pending=1",
                text,
            )
            is not None
            # Spec 01 §4: landscape boots at server 1; portrait fallbacks at server 2.
            and re.search(r"SCOREBOARD ready state=0,0,L,[12],0,D ", text) is not None
            and not any(
                s in text
                for s in [
                    "Guru Meditation",
                    "Brownout",
                    "assert failed",
                    "Backtrace",
                    "startup self-check failed",
                ]
            )
        )

    def close(self):
        self.stop.set()
        self.thread.join(timeout=3)
        self.port.close()
        self.log.close()


class Api:
    def __init__(self, address, guard=None):
        self.address = address
        self.guard = guard
        self.token = None
        self.nonce = None

    def request(self, path, method="GET", body=b"", headers=None, timeout=10):
        require(
            path
            in [
                "/api/info",
                "/api/status",
                "/api/session",
                "/api/upload",
                "/api/install",
                "/api/close",
            ],
            "protocol",
            "Unknown endpoint",
        )
        if self.guard is not None:
            self.guard()
        h = {"Host": "192.168.4.1", "Connection": "close"}
        if method == "POST":
            h["Origin"] = "http://192.168.4.1"
        if self.token:
            h["Authorization"] = "Bearer " + self.token
        if method == "POST" and self.token:
            assert self.nonce is not None
            h["X-CSRF-Token"] = self.nonce
        h.update(headers or {})
        connection = http.client.HTTPConnection(
            "192.168.4.1", 80, timeout=timeout, source_address=(self.address, 0)
        )
        stage = "request"
        try:
            connection.request(method, path, body=body, headers=h)
            stage = "response-headers"
            response = connection.getresponse()
            stage = "response-body"
            raw = response.read(8193)
            require(len(raw) <= 8192, "protocol", "Oversized device response")
            try:
                result = json.loads(raw)
                require(
                    isinstance(result, dict),
                    "protocol",
                    "JSON object required from device",
                )
            except ValueError as error:
                raise DeploymentError("protocol", "Invalid device JSON") from error
            if response.status not in (200, 202):
                # Never expose arbitrary response bodies or headers (tokens/secrets);
                # keep only a recognized fixed firmware rejection reason.
                raise DeploymentError(
                    "http_" + str(response.status),
                    f"{path} rejected with HTTP {response.status}",
                    device_reason(result.get("error")),
                )
            return result
        except (OSError, http.client.HTTPException) as error:
            raise DeploymentError(
                "transport", f"{method} {path} failed at {stage} ({type(error).__name__}, errno={getattr(error, 'errno', None)})"
            ) from error
        finally:
            connection.close()

    def public_info(self):
        # Spec 04: retry only the unauthenticated, read-only identity probe.
        require(self.token is None, "protocol", "Public probe requires a fresh session")
        try:
            return self.request("/api/info", timeout=3)
        except DeploymentError as first:
            if first.code != "transport":
                raise
            time.sleep(0.5)
            try:
                return self.request("/api/info", timeout=3)
            except DeploymentError as second:
                raise DeploymentError(second.code, f"Initial probe: {first}; recheck: {second}") from second

    def login(self, key):
        result = self.request("/api/session", "POST", headers={"X-Admin-Key": key})
        require(
            all(
                isinstance(result.get(k), str)
                and re.fullmatch("[0-9a-f]{64}", result[k])
                for k in ["token", "nonce"]
            ),
            "protocol",
            "Invalid session response",
        )
        self.token = result["token"]
        self.nonce = result["nonce"]

    def status(self):
        result = self.request("/api/status")
        require(
            isinstance(result.get("nonce"), str)
            and re.fullmatch("[0-9a-f]{64}", result["nonce"]),
            "protocol",
            "Invalid session nonce",
        )
        self.nonce = result["nonce"]
        return result

    def upload(self, package):
        # Use the nonce from the authenticated pre-write status, not a stale login.
        result = self.request(
            "/api/upload",
            "POST",
            package,
            {"Content-Type": "application/octet-stream"},
            timeout=125,
        )
        require(
            result.get("staged") is True,
            "upload_unconfirmed",
            "Device did not acknowledge the upload",
        )
        return {"transferAccepted": True}

    def verify_staged(self, build, image_bytes):
        status = self.status()
        require(
            status.get("phase") == "staged"
            and status.get("stagedBuild") == build
            and all(
                type(status.get(field)) is int and status[field] == image_bytes
                for field in ("received", "total")
            ),
            "stage_verification",
            "Expected build and exact received extent were not verified/staged",
        )
        return {
            "flashReadbackVerifiedByDevice": True,
            "stagedBuild": build,
            "stagedBytes": image_bytes,
            "memoryAfterReadback": maintenance_evidence(status),
        }

    def install(self):
        # Use the nonce from the immediately preceding verified staged status.
        result = self.request("/api/install", "POST")
        require(
            result.get("installing") is True,
            "protocol",
            "Missing install acknowledgement; outcome unknown",
        )

    def close(self):
        self.status()
        require(
            self.request("/api/close", "POST").get("closed") is True,
            "close_unconfirmed",
            "Maintenance close was not acknowledged",
        )
        self.token = self.nonce = None


def maintenance_evidence(status):
    # Only persist known numeric fields. Older VALID firmware has only `heap`;
    # it still enforces the contiguous allocation floor on-device before erase.
    evidence = {
        key: status[key]
        for key in ("heap", "minimumHeap", "largestInternal8", "httpStackFree", "timerArmedDeletes", "timerRecovered")
        if type(status.get(key)) is int and 0 <= status[key] <= 0xFFFFFFFF
    }
    # 2.2+: whether the inactive slot (the fallback) may have been erased.
    if type(status.get("inactiveWritten")) is bool:
        evidence["inactiveWritten"] = status["inactiveWritten"]
    return evidence


def check_write_readiness(status):
    evidence = maintenance_evidence(status)
    require(status.get("phase") == "session"
            and all(status.get(field, 0) == 0 for field in ("received", "total"))
            and status.get("stagedBuild", "") == "",
            "maintenance_busy", "Expected idle authenticated session before upload")
    require(
        evidence.get("heap", 0) >= OPERATION_FREE_HEAP
        and ("largestInternal8" not in status or evidence.get("largestInternal8", 0) >= FLASH_SECTOR_BYTES),
        "memory_admission",
        "Authenticated status does not meet upload memory floors; no upload attempted",
    )
    require(
        evidence["heap"] >= UPLOAD_SESSION_HEAP,
        "memory_headroom",
        f"Session heap {evidence['heap']} B is below the {UPLOAD_SESSION_HEAP} B the measured upload "
        "needs to keep the device floors; no upload attempted",
    )
    return evidence


def validate_target(info, device):
    require(
        info.get("device") == device["httpDeviceId"],
        "device_identity",
        "HTTP device identity mismatch",
    )
    require(
        info.get("limit") == ota_tool.SLOT_SIZE
        and info.get("slot") in ["ota_0", "ota_1"],
        "partition",
        "Incompatible partition layout",
    )
    # Never persist arbitrary peer-supplied fields. Reject malformed known values.
    build = info.get("build")
    require(
        isinstance(build, str) and re.fullmatch(r"[A-Za-z0-9_.-]{1,31}", build),
        "protocol",
        "Invalid running build identifier",
    )
    result = {key: info[key] for key in ("device", "build", "slot", "limit")}
    for key in ("heap", "uptimeMs", "agentProtocol"):
        if type(info.get(key)) is int and 0 <= info[key] <= 0xFFFFFFFF:
            result[key] = info[key]
    for key in ("confirmed", "healthy"):
        if type(info.get(key)) is bool:
            result[key] = info[key]
    if info.get("bootState") in ("unknown", "undefined", "pending", "valid", "other"):
        result["bootState"] = info["bootState"]
    digest = info.get("imageSha256")
    if isinstance(digest, str) and re.fullmatch(r"[0-9a-f]{64}", digest):
        result["imageSha256"] = digest
    return result


def verify_health(info, build, slot, digest, witness=None):
    require(
        info.get("build") == build,
        "version_mismatch",
        "Wrong running version or rollback observed",
    )
    require(
        info.get("slot") == slot,
        "slot_mismatch",
        "Wrong running partition or rollback observed",
    )
    if info.get("agentProtocol") == 1:
        require(
            info.get("imageSha256") == digest,
            "image_mismatch",
            "Running application digest differs",
        )
        require(
            info.get("healthy") is True
            and info.get("confirmed") is True
            and info.get("bootState") == "valid",
            "health_unconfirmed",
            "Updated application not confirmed healthy",
        )
        return "device-confirmed-image-and-startup"
    require(
        witness is not None and witness.confirmed(build, slot),
        "health_unconfirmed",
        "Legacy firmware requires isolated serial evidence; HTTP version alone does not prove boot confirmation",
    )
    return "legacy-isolated-serial-confirmed-startup"


def finish_network(network, report, api, primary, *, observe_close=False):
    # Spec 04 §8: retain the first failure even if close, WLAN restore or Ethernet also fails.
    first = primary
    if api is None or not api.token:
        report["maintenanceClosure"] = ("no_authenticated_session" if not report.get("authenticated")
                                        else "no_authenticated_session_to_close")
    else:
        report["maintenanceClosure"] = "close_unconfirmed"
        try:
            api.close()
            report["maintenanceCloseAccepted"] = True
            report["maintenanceClosure"] = "acknowledged"
            if observe_close:
                report["maintenanceLinkDownObserved"] = network.observe_close()
                require(report["maintenanceLinkDownObserved"], "maintenance_link_still_up",
                        "Close acknowledged but Windows did not observe the pinned link drop")
        except (RuntimeError, OSError, KeyboardInterrupt) as error:
            report.setdefault("cleanupFailures", {})["close"] = getattr(error, "code", type(error).__name__)
            first = first if first is not None else error
    for step, key, operation in (("wifi", "networkRestoration", network.restore),
                                 ("ethernet", "ethernetAfter", network.internet)):
        try:
            report[key] = operation()
            if step == "wifi":
                require(report[key].get("restored") is True, "network_restoration", "Prior WLAN state was not restored")
        except (RuntimeError, OSError, subprocess.SubprocessError, KeyboardInterrupt) as error:
            if step == "wifi":
                report[key] = {"restored": False}
            report.setdefault("cleanupFailures", {})[step] = getattr(error, "code", type(error).__name__)
            if first is None:
                first = (DeploymentError("network_restoration", "Prior WLAN state was not restored")
                         if step == "wifi" else error)
    if primary is None and first is not None:
        raise first


def deploy_transaction(
    device,
    owner,
    package,
    release,
    network,
    report,
    activate,
    witness=None,
    sleep=time.sleep,
    clock=time.monotonic,
    *,
    expected_source,
    recover_ble=None,
):
    """Deep deployment seam: actual and mock WLAN/HTTP adapters share the lifecycle."""
    api = None
    report["bleRecoveryVerified"] = False
    try:
        report["ethernetBefore"] = network.internet()
        activate()
        report["activationRequested"] = True
        report["stage"] = "maintenance-requested"
        connected = network.connect(
            owner["ssid"], owner["apPassword"], device.get("bssid")
        )
        report["wifi"] = connected
        report["stage"] = "wifi-connected"
        report["ethernetDuring"] = network.internet()
        api = network.api()
        validate_target(api.public_info(), device)
        report["stage"] = "http-authentication"
        api.login(owner["maintenanceKey"])
        report["authenticated"] = True
        previous = validate_target(api.request("/api/info"), device)
        report["before"] = previous
        # Spec 04 §8: no inactive-slot write unless the authorized source is still confirmed.
        try:
            verify_health(
                previous,
                expected_source["build"],
                expected_source["slot"],
                expected_source["imageSha256"],
            )
        except DeploymentError as error:
            raise DeploymentError(
                "source_mismatch",
                "Running build, slot, digest or VALID health does not match the approved source",
            ) from error
        if "maxUptimeMs" in expected_source:
            # Owner-reported restart: no write unless the authenticated uptime proves it.
            require(
                previous.get("uptimeMs", expected_source["maxUptimeMs"] + 1) <= expected_source["maxUptimeMs"],
                "stale_boot",
                "Source uptime does not reflect the reported restart; no upload attempted",
            )
        require(
            previous["build"] != release["build"],
            "same_build",
            "Use a distinguishable new build ID",
        )
        report["destinationSlot"] = "ota_1" if previous["slot"] == "ota_0" else "ota_0"
        report["preUploadMemory"] = check_write_readiness(api.status())
        report["wirelessTransferAttempted"] = True
        report["stage"] = "uploading"
        try:
            report["transfer"] = api.upload(package)
        except DeploymentError as error:
            if error.code in ("transport", "protocol", "upload_unconfirmed"):
                report["uploadOutcomeUncertain"] = True
                # Only observe the same authenticated session. Even successful readback
                # here does not authorize replay or automatic install after an ambiguous POST.
                try:
                    status = api.verify_staged(release["build"], release["imageBytes"])
                    report["observedAfterUncertainUpload"] = status
                except (RuntimeError, OSError) as observed_error:
                    report["uploadObservationFailure"] = getattr(observed_error, "code", type(observed_error).__name__)
            raise
        report["stage"] = "upload-acknowledged"
        report["transfer"].update(
            api.verify_staged(release["build"], release["imageBytes"])
        )
        report["stage"] = "staged"
        try:
            api.install()
            report["installAccepted"] = True
            report["stage"] = "install-accepted"
        except DeploymentError as error:
            if error.code not in ("transport", "protocol"):
                raise
            report["installOutcomeUncertain"] = True
            report["stage"] = "install-uncertain-observing-reboot"
        # Never repeat an install POST whose outcome is uncertain. Drop the
        # old Windows association instead of mistaking it for the rebooted AP.
        api = None
        try:
            network.release_for_reboot()
            report["sourceLinkReleased"] = True
        except (RuntimeError, OSError, subprocess.SubprocessError):
            # Release failure is not permission to replay install. Continue
            # observing: exact image/slot/health, not WLAN state, prove boot.
            report["sourceLinkReleased"] = False
        deadline = clock() + 100
        sleep(5)
        report["candidateWifi"] = network.connect(
            owner["ssid"], owner["apPassword"], device.get("bssid"),
            timeout=min(60, deadline - clock()),
        )
        last = "reboot_timeout"
        candidate = network.api()
        while clock() < deadline:
            try:
                info = validate_target(candidate.public_info(), device)
                slot = report["destinationSlot"]
                report["observedAfterInstall"] = info
                if (
                    info["build"] == release["build"]
                    and info["slot"] == slot
                    and info.get("imageSha256") == release["imageSha256"]
                ):
                    report["stage"] = "expected-image-booted"
                health = verify_health(
                    info, release["build"], slot, release["imageSha256"], witness
                )
                if api is None:
                    candidate.login(owner["maintenanceKey"])
                    api = candidate
                info = validate_target(api.request("/api/info"), device)
                health = verify_health(info, release["build"], slot, release["imageSha256"], witness)
                report["candidateMemory"] = maintenance_evidence(api.status())
                report.update(
                    {
                        "after": info,
                        "healthEvidence": health,
                        "rebootVerified": True,
                        "installationVerified": True,
                        "stage": "health-confirmed",
                    }
                )
                break
            except (DeploymentError, RuntimeError, OSError) as error:
                last = (
                    error.code
                    if isinstance(error, DeploymentError)
                    else "reconnect_timeout"
                )
                sleep(min(2, max(0, deadline - clock())))
        else:
            raise DeploymentError(
                last,
                "Post-install reboot/version/slot/health verification deadline exceeded; do not reflash automatically",
            )
    finally:
        finish_network(network, report, api, sys.exc_info()[1], observe_close=recover_ble is not None)
    if recover_ble is not None:
        recover_ble()
        report["bleRecoveryVerified"] = True
    report["deploymentSuccess"] = True
    return report


class Network:
    def __init__(self, description):
        self.description = description
        self.wifi = None  # BLE activation precedes Native WLAN adapter acquisition
        self.wifi_attempts = []

    @staticmethod
    def internet():
        from ota_windows import ethernet_probe

        try:
            return ethernet_probe()
        except (RuntimeError, OSError, subprocess.SubprocessError) as error:
            raise DeploymentError(
                "ethernet_unavailable",
                "Ethernet routing or source-bound TLS internet probe failed",
            ) from error

    def connect(self, *args, **kwargs):
        from ota_windows import WifiError, WindowsWifi

        try:
            if self.wifi is None:
                self.wifi = WindowsWifi(self.description)
                self.wifi.attempts = self.wifi_attempts
            return self.wifi.connect(*args, **kwargs)
        except WifiError as error:
            raise DeploymentError(error.code, str(error)) from error
        except RuntimeError as error:
            raise DeploymentError("wifi_connection", str(error)) from error

    def release_for_reboot(self):
        assert self.wifi is not None
        self.wifi.release_for_reboot()

    def api(self):
        assert self.wifi is not None
        from ota_windows import WifiError

        wifi = self.wifi
        source, link = wifi.address, wifi.current

        def guard():
            try:
                wifi.guard_http(source, link)
            except WifiError as error:
                raise DeploymentError(error.code, str(error)) from error

        return Api(source, guard=guard)

    def observe_close(self):
        # Spec 04 §8: observe link loss before Windows initiates its own disconnect.
        assert self.wifi is not None
        deadline = time.monotonic() + 17  # prior ACK-to-link-down was ~8–11 s
        while time.monotonic() < deadline:
            link = self.wifi.connection()
            if not link or link["bssid"] != self.wifi.current["bssid"]:
                return True
            time.sleep(.25)
        return False

    def restore(self):
        if self.wifi is None:
            return {"restored": True, "unchanged": True}
        return self.wifi.restore()


def load_device(path, name):
    require(re.fullmatch(r"[A-Za-z0-9_-]{1,32}", name), "config", "Invalid device name")
    require(
        not path.resolve().is_relative_to(ROOT),
        "private_config",
        "Registry must be outside Git",
    )
    try:
        registry = json.loads(path.read_text())
    except (OSError, ValueError) as error:
        raise DeploymentError(
            "config", "Cannot read private device registry"
        ) from error
    device = registry["devices"][name]
    owner_path = Path(device["ownerCredentials"])
    require(
        not owner_path.resolve().is_relative_to(ROOT),
        "private_config",
        "Owner credentials must remain outside Git",
    )
    try:
        owner = json.loads(owner_path.read_text())
    except (OSError, ValueError) as error:
        raise DeploymentError(
            "private_config", "Cannot read protected owner credentials"
        ) from error
    require(
        not owner.get("testOnly", True),
        "private_config",
        "Test trust is forbidden for real deployments",
    )
    require(
        re.fullmatch(r"(?:[0-9a-f]{2}:){5}[0-9a-f]{2}", device["mac"]),
        "config",
        "Invalid target MAC",
    )
    for field in ("bssid", "bleAddress"):
        require(
            re.fullmatch(r"(?:[0-9a-f]{2}:){5}[0-9a-f]{2}", device[field]),
            "config",
            "Invalid radio identity pin",
        )
    require(
        len(owner["apPassword"]) == 32
        and re.fullmatch(r"[0-9a-f]{64}", owner["maintenanceKey"]),
        "private_config",
        "Malformed owner credentials",
    )
    require(owner["ssid"] == device["ssid"], "config", "SSID pin mismatch")
    require(
        device["httpDeviceId"]
        == "PickleScore-" + bytes.fromhex(device["mac"].replace(":", ""))[::-1].hex(),
        "config",
        "Unexpected legacy device identifier",
    )
    point = ota_tool.public_point(
        shutil.which("openssl") or "openssl", Path(device["signingPublicKey"])
    )
    require(
        hashlib.sha256(point).hexdigest() == owner["publicKeySha256"],
        "signing",
        "Signing key differs from installed trust",
    )
    return device, owner


def build_package(args, device):
    require(args.build_id is not None, "build_id", "--build-id required with --build")
    ota_tool.padded_build(args.build_id)
    ota_tool.outside(Path(device["signingPrivateKey"]))
    env = os.environ.copy()
    env["PICKLESCORE_BUILD_ID"] = args.build_id
    env["PYTHONIOENCODING"] = "utf-8"
    commands = [
        [
            sys.executable,
            str(ROOT / "scripts/run_firmware_host_tests.py"),
            "--output-dir",
            str(args.output / "host"),
        ],
        [sys.executable, str(ROOT / "scripts/check_glyphs.py")],
        [
            sys.executable,
            str(ROOT / "scripts/check_display_snapshots.py"),
            "--cpp-tool",
            str(args.output / "host/render-snapshots.exe"),
        ],
        [
            str(ROOT / "firmware/.venv/Scripts/pio.exe"),
            "run",
            "-d",
            str(ROOT / "firmware"),
            "-e",
            "ota",
        ],
    ]
    for index, command in enumerate(commands):
        with (args.output / f"build-step-{index}.log").open("x") as log:
            subprocess.run(
                command,
                cwd=ROOT,
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
                check=True,
                timeout=600,
            )
    image = ROOT / "firmware/.pio/build/ota/firmware.bin"
    ota_tool.validate_image(image.read_bytes(), args.build_id)
    expected = bytes.fromhex(
        "36860d0760552d2e33f63a2b99f0d7b6f2c0dd035618737c6e42c8038eeade66"
    )
    require(
        hashlib.sha256((image.parent / "partitions.bin").read_bytes()).digest()
        == expected,
        "partition",
        "Built partition table changed; no migration allowed",
    )
    shutil.copy2(image, args.output / "firmware.bin")
    shutil.copy2(image.parent / "firmware.elf", args.output / "firmware.elf")
    package = args.output / (args.build_id + ".pksota")
    ota_tool.pack(
        shutil.which("openssl") or "openssl",
        args.output / "firmware.bin",
        Path(device["signingPrivateKey"]),
        args.build_id,
        package,
    )
    return package


def live_mode(args):
    """Spec 04 §8: caller attestations, not electrical or idle detection."""
    require(args.authorize_score_reset, "authorization", "Explicit --authorize-score-reset is required; 0-0 does not imply idle")
    if args.verify_ble_recovery:
        require(args.operation in ("verify", "deploy") and args.activation == "ble" and not args.build,
                "activation", "BLE recovery observation requires BLE verify or deploy without building")
        require(args.package and args.expect_package_sha256 and args.expect_source_slot,
                "authorization", "Pin the signed verification package digest and running slot")
    if args.operation == "deploy":
        require(args.activation != "usb", "activation", "Live deployment supports BLE or an existing AP, not USB reset")
        require(args.authorize_write_and_fallback, "authorization", "Explicit --authorize-write-and-fallback is required before inactive-slot erase")
    if args.activation == "usb":
        require(args.confirm_isolated_usb_only and args.port and not args.confirm_installed_ready,
                "electrical_safety", "USB reset requires port and fresh --confirm-isolated-usb-only, never installed mode")
        return "isolated-usb-commissioning"
    require(not (args.port or args.confirm_isolated_usb_only), "electrical_safety", "USB options are not used in installed wireless mode")
    require(args.confirm_installed_ready, "authorization", "Confirm idle court, controller disconnected, stable approved installed power and recovery with --confirm-installed-ready")
    return "installed-wireless"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "operation", choices=["preflight", "status", "build", "deploy", "verify"]
    )
    parser.add_argument("--device", required=True)
    parser.add_argument(
        "--registry",
        type=Path,
        default=Path(
            os.environ.get(
                "PICKLESCORE_DEVICES", str(Path.home() / ".picklescore/devices.json")
            )
        ),
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--package", type=Path)
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--build-id")
    parser.add_argument("--expect-package-sha256")
    parser.add_argument("--expect-source-build")
    parser.add_argument("--expect-source-slot", choices=["ota_0", "ota_1"])
    parser.add_argument("--expect-source-sha256")
    parser.add_argument("--expect-source-max-uptime-s", type=int,
                        help="Refuse to upload unless the authenticated source uptime is at most this")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--verify-ble-recovery", action="store_true",
                        help="After BLE verify/deploy and maintenance close, witness pinned BLE initial sync")
    parser.add_argument(
        "--authorize-score-reset",
        action="store_true",
        help="Owner explicitly permits interruption and loss of volatile game state, even at0-0",
    )
    parser.add_argument(
        "--activation", choices=["ble", "usb", "existing"], default="ble"
    )
    parser.add_argument("--confirm-installed-ready", action="store_true",
                        help="Owner confirms idle court, voluntarily disconnected controller, stable approved installed power/wiring and a recovery plan")
    parser.add_argument("--authorize-write-and-fallback", action="store_true",
                        help="Owner approves inactive-slot erase/write (replaces fallback) and install of the exact pinned package")
    parser.add_argument("--confirm-isolated-usb-only", action="store_true")
    parser.add_argument("--port")
    args = parser.parse_args()
    report = {
        "deploymentSuccess": False,
        "stage": "preflight",
        "wirelessTransferAttempted": False,
    }
    lock = None
    adapter_lock = None
    lock_path = None
    witness = None
    network = None
    output_created = False
    try:
        require(
            os.name == "nt",
            "platform",
            "Run Windows Python; from WSL invoke Windows firmware/.venv/Scripts/python.exe with Windows paths",
        )
        ota_tool.outside(args.output)
        args.output.mkdir(parents=True, exist_ok=False)
        output_created = True
        if not args.dry_run and args.operation in ("deploy", "status", "verify"):
            report["operationMode"] = live_mode(args)
            report["activationMethod"] = args.activation
        device, owner = load_device(args.registry, args.device)
        from ota_windows import DeploymentLock

        adapter_lock = DeploymentLock()
        # Lock all tool instances sharing this owner device; never auto-remove stale locks.
        lock_path = Path(device["ownerCredentials"]).parent / (
            "deployment-" + args.device + ".lock"
        )
        lock = lock_path.open("x")
        lock.write(str(os.getpid()))
        lock.flush()
        report.update(
            {"device": args.device, "mac": device["mac"], "operation": args.operation}
        )
        report["ethernetBefore"] = Network.internet()
        package = args.package
        source = None
        if args.operation == "deploy" and not args.dry_run:
            require(
                package is not None and not args.build,
                "package",
                "Live deployment requires a prebuilt, separately approved --package",
            )
            require(
                args.expect_source_build and args.expect_source_slot,
                "authorization",
                "Pin the approved source build and slot",
            )
            ota_tool.padded_build(args.expect_source_build)
            for digest in (args.expect_package_sha256, args.expect_source_sha256):
                require(
                    digest and re.fullmatch(r"[0-9a-f]{64}", digest),
                    "authorization",
                    "Pin approved package and source SHA-256 digests",
                )
            source = {
                "build": args.expect_source_build,
                "slot": args.expect_source_slot,
                "imageSha256": args.expect_source_sha256,
            }
            if args.expect_source_max_uptime_s is not None:
                require(0 < args.expect_source_max_uptime_s <= 86400, "authorization",
                        "Source uptime bound must be 1-86400 seconds")
                source["maxUptimeMs"] = args.expect_source_max_uptime_s * 1000
            report["expectedSource"] = source
        if args.build or args.operation == "build":
            package = build_package(args, device)
            report["buildSuccess"] = True
        release = None
        payload = None
        if package:
            with package.open("rb") as stream:
                payload = stream.read(ota_tool.HEADER_SIZE + ota_tool.SLOT_SIZE + 1)
            release = ota_tool.verify_bytes(
                shutil.which("openssl") or "openssl",
                payload,
                Path(device["signingPublicKey"]),
                args.output / "signature-check",
            )
            require(
                not args.expect_package_sha256
                or release["packageSha256"] == args.expect_package_sha256,
                "package_mismatch",
                "Verified package digest does not match the approved bytes",
            )
            report["signedPackage"] = release
            report["stage"] = "package-verified"
        if args.dry_run or args.operation in ["preflight", "build"]:
            from ota_windows import WindowsWifi

            inspected_wifi = WindowsWifi(device.get("wifiDescription"))
            try:
                report["wifiAdapter"] = {
                    "description": inspected_wifi.description,
                    "interfaceIndex": inspected_wifi.index,
                    "radioStates": inspected_wifi.saved_radios,
                    "connected": bool(inspected_wifi.saved),
                }
                require(
                    all(
                        hardware == 1 for _, _, hardware in inspected_wifi.saved_radios
                    ),
                    "wifi_radio",
                    "Wi-Fi hardware radio is off",
                )
            finally:
                report["networkRestoration"] = inspected_wifi.restore()
            report["preflightSuccess"] = True
            report["noHardwareActivation"] = True
        else:
            if args.activation == "usb":
                witness = SerialWitness(args.port, device["mac"], args.output)

            def activate():
                if args.activation == "ble":
                    if args.verify_ble_recovery:
                        asyncio.run(_activate_ble_once(device, owner))
                    else:
                        asyncio.run(activate_ble(device, owner))
                elif args.activation == "usb":
                    assert witness is not None
                    witness.reset()

            network = Network(device.get("wifiDescription"))
            report["wifiAttempts"] = network.wifi_attempts
            report["bleRecoveryVerified"] = False
            report["maintenanceLinkDownObserved"] = False
            if args.operation == "deploy":
                require(
                    release is not None and payload is not None and source is not None,
                    "package",
                    "A verified package and approved source are required",
                )
                assert (
                    payload is not None and release is not None and source is not None
                )
                report["deploymentAttempted"] = True
                owned_network = network
                network = (
                    None  # transaction owns/restores exactly once, including on failure
                )
                deploy_transaction(
                    device,
                    owner,
                    payload,
                    release,
                    owned_network,
                    report,
                    activate,
                    witness,
                    expected_source=source,
                    recover_ble=(lambda: asyncio.run(verify_ble_recovery(device))) if args.verify_ble_recovery else None,
                )
                network = None
            else:
                api = None
                try:
                    if args.operation == "verify":
                        require(
                            release is not None,
                            "package",
                            "Verify requires signed --package to pin image/version",
                        )
                    activate()
                    report["activationRequested"] = True
                    report["activationMethod"] = args.activation
                    report["stage"] = "maintenance-requested"
                    report["wifi"] = network.connect(
                        owner["ssid"], owner["apPassword"], device.get("bssid")
                    )
                    report["ethernetDuring"] = network.internet()
                    api = network.api()
                    validate_target(api.public_info(), device)
                    api.login(owner["maintenanceKey"])
                    report["authenticated"] = True
                    info = validate_target(api.request("/api/info"), device)
                    report["info"] = info
                    report["memory"] = maintenance_evidence(api.status())
                    report["stage"] = "status-verified"
                    if args.operation == "verify":
                        assert release is not None
                        report["healthEvidence"] = verify_health(
                            info,
                            release["build"],
                            args.expect_source_slot or info["slot"],
                            release["imageSha256"],
                            witness,
                        )
                finally:
                    owned_network = network
                    network = None
                    finish_network(owned_network, report, api, sys.exc_info()[1],
                                   observe_close=args.verify_ble_recovery)
                if args.verify_ble_recovery:
                    asyncio.run(verify_ble_recovery(device))
                    report["bleRecoveryVerified"] = True
                report["statusSuccess"] = True
    except (
        OSError,
        ValueError,
        KeyError,
        TypeError,
        RuntimeError,
        ImportError,
        subprocess.SubprocessError,
        KeyboardInterrupt,
    ) as error:
        report["failure"] = {
            "code": error.code
            if isinstance(error, DeploymentError)
            else type(error).__name__,
            "message": str(error)
            if isinstance(error, DeploymentError)
            else "Operation failed; inspect bounded local evidence; secrets omitted",
        }
        if getattr(error, "device_reason", None):
            report["failure"]["deviceReason"] = error.device_reason
        report["deploymentSuccess"] = False
    finally:
        if network:
            try:
                report["networkRestoration"] = network.restore()
            except (OSError, RuntimeError):
                report["networkRestoration"] = {"restored": False}
        if witness:
            witness.close()
        if lock:
            lock.close()
            assert lock_path is not None
            lock_path.unlink()
        if adapter_lock:
            adapter_lock.close()
        if output_created:
            (args.output / "result.json").write_text(
                json.dumps(report, indent=2) + "\n"
            )
        print(json.dumps(report, indent=2))
    return 1 if "failure" in report else 0


def entrypoint():
    if os.name != "nt" and "microsoft" in platform.release().lower():
        # WSL owns neither WLAN nor BLE. Delegate the entire operation, not just netsh.
        def windows_path(value):
            return subprocess.check_output(
                ["wslpath", "-w", str(Path(value).resolve())], text=True
            ).strip()

        arguments = sys.argv[1:]
        for index, argument in enumerate(arguments[:-1]):
            if argument in ("--registry", "--output", "--package"):
                arguments[index + 1] = windows_path(arguments[index + 1])
        executable = ROOT / "firmware/.venv/Scripts/python.exe"
        return subprocess.run(
            [str(executable), windows_path(__file__), *arguments], check=False
        ).returncode
    return main()


if __name__ == "__main__":
    raise SystemExit(entrypoint())
