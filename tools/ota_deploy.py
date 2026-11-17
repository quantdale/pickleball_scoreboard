"""Explicit signed deployment, Spec04 §8. Windows Python owns physical adapters.

Example: python tools/ota_deploy.py deploy --device court1 --build --build-id agent-001
         --authorize-score-reset --output <external-session> --registry <private-json>
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
DOMAIN = b"PKS-MAINT-1\0"


class DeploymentError(RuntimeError):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code


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


async def _activate_ble_once(device, owner, cached=True):
    bleak = importlib.import_module("bleak")
    bleak_errors = importlib.import_module("bleak.exc")
    stage = "scan"
    try:
        target = await bleak.BleakScanner.find_device_by_address(
            device["bleAddress"], timeout=15
        )
        require(
            target is not None,
            "ble_unavailable",
            "Target BLE unavailable/occupied; disconnect scoring controller, never evict it",
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
            and "SCOREBOARD ready state=0,0,L,2,0,D" in text
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
    def __init__(self, address):
        self.address = address
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
        try:
            connection.request(method, path, body=body, headers=h)
            response = connection.getresponse()
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
                # Never expose arbitrary response bodies or headers (tokens/secrets).
                raise DeploymentError(
                    "http_" + str(response.status),
                    f"{path} rejected with HTTP {response.status}",
                )
            return result
        except (OSError, http.client.HTTPException) as error:
            raise DeploymentError(
                "transport", f"{path} connection failed/interrupted"
            ) from error
        finally:
            connection.close()

    def login(self, key):
        result = self.request("/api/session", "POST", headers={"X-Admin-Key": key})
        require(
            all(
                re.fullmatch("[0-9a-f]{64}", result.get(k, ""))
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
            re.fullmatch("[0-9a-f]{64}", result.get("nonce", "")),
            "protocol",
            "Invalid session nonce",
        )
        self.nonce = result["nonce"]
        return result

    def upload(self, package, build):
        self.status()
        result = self.request(
            "/api/upload",
            "POST",
            package,
            {"Content-Type": "application/octet-stream"},
            timeout=125,
        )
        status = self.status()
        require(
            result.get("staged") is True
            and status.get("phase") == "staged"
            and status.get("stagedBuild") == build,
            "stage_verification",
            "Expected image was not verified/staged",
        )
        return {
            "transferAccepted": True,
            "flashReadbackVerifiedByDevice": True,
            "stagedBuild": build,
        }

    def install(self):
        self.status()
        result = self.request("/api/install", "POST")
        require(
            result.get("installing") is True,
            "install_refused",
            "Installation not accepted",
        )


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
    return info


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
):
    """Deep deployment seam: actual and mock WLAN/HTTP adapters share the lifecycle."""
    api = None
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
        previous = validate_target(api.request("/api/info"), device)
        report["before"] = previous
        require(
            previous["build"] != release["build"],
            "same_build",
            "Use a distinguishable new build ID",
        )
        api.login(owner["maintenanceKey"])
        report["authenticated"] = True
        report["wirelessTransferAttempted"] = True
        report["stage"] = "uploading"
        report["transfer"] = api.upload(package, release["build"])
        report["stage"] = "staged"
        try:
            api.install()
            report["installAccepted"] = True
            report["stage"] = "install-accepted"
        except DeploymentError as error:
            if error.code != "transport":
                raise
            report["installOutcomeUncertain"] = True
            report["stage"] = "install-uncertain-observing-reboot"
        # Never repeat an install POST whose outcome is uncertain.
        api = None
        deadline = clock() + 100
        sleep(5)
        last = "reboot_timeout"
        while clock() < deadline:
            try:
                network.connect(
                    owner["ssid"], owner["apPassword"], device.get("bssid"), timeout=12
                )
                candidate = network.api()
                info = validate_target(candidate.request("/api/info"), device)
                slot = "ota_1" if previous["slot"] == "ota_0" else "ota_0"
                health = verify_health(
                    info, release["build"], slot, release["imageSha256"], witness
                )
                report.update(
                    {
                        "after": info,
                        "healthEvidence": health,
                        "rebootVerified": True,
                        "installationVerified": True,
                        "stage": "health-confirmed",
                    }
                )
                api = candidate
                break
            except (DeploymentError, RuntimeError, OSError) as error:
                last = (
                    error.code
                    if isinstance(error, DeploymentError)
                    else "reconnect_timeout"
                )
                sleep(2)
        else:
            raise DeploymentError(
                last,
                "Post-install reboot/version/slot/health verification deadline exceeded; do not reflash automatically",
            )
        # Close AP intentionally; BLE resumes without retaining an admin session.
        api.login(owner["maintenanceKey"])
        api.status()
        api.request("/api/close", "POST")
        api = None
        report["maintenanceClosed"] = True
    finally:
        try:
            if api and api.token:
                try:
                    api.status()
                    api.request("/api/close", "POST")
                except (DeploymentError, OSError):
                    report["cleanupSessionCloseUncertain"] = True
        finally:
            try:
                report["networkRestoration"] = {"restored": False}
                report["networkRestoration"] = network.restore()
            finally:
                report["ethernetAfter"] = network.internet()
    report["deploymentSuccess"] = True
    return report


class Network:
    def __init__(self, description):
        self.description = description
        self.wifi = None  # BLE activation precedes Native WLAN adapter acquisition

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
        from ota_windows import WindowsWifi

        try:
            if self.wifi is None:
                self.wifi = WindowsWifi(self.description)
            return self.wifi.connect(*args, **kwargs)
        except RuntimeError as error:
            raise DeploymentError("wifi_connection", str(error)) from error

    def api(self):
        assert self.wifi is not None
        return Api(self.wifi.address)

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
        image,
        Path(device["signingPrivateKey"]),
        args.build_id,
        package,
    )
    return package


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
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument(
        "--authorize-score-reset",
        action="store_true",
        help="Owner explicitly permits interruption and loss of volatile game state, even at0-0",
    )
    parser.add_argument(
        "--activation", choices=["ble", "usb", "existing"], default="ble"
    )
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
        if args.build or args.operation == "build":
            package = build_package(args, device)
            report["buildSuccess"] = True
        release = None
        if package:
            release = ota_tool.verify(
                shutil.which("openssl") or "openssl",
                package,
                Path(device["signingPublicKey"]),
                args.output / "signature-check",
            )
            release["imageSha256"] = hashlib.sha256(
                package.read_bytes()[ota_tool.HEADER_SIZE :]
            ).hexdigest()
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
            require(
                args.authorize_score_reset,
                "authorization",
                "Explicit --authorize-score-reset is required; 0-0 does not imply idle",
            )
            require(
                args.activation != "usb"
                or (args.confirm_isolated_usb_only and args.port),
                "electrical_safety",
                "USB reset requires port and fresh --confirm-isolated-usb-only",
            )
            if args.activation == "usb":
                witness = SerialWitness(args.port, device["mac"], args.output)

            def activate():
                if args.activation == "ble":
                    asyncio.run(activate_ble(device, owner))
                elif args.activation == "usb":
                    assert witness is not None
                    witness.reset()

            network = Network(device.get("wifiDescription"))
            if args.operation == "deploy":
                require(
                    release is not None and package is not None,
                    "package",
                    "Supply --package or --build",
                )
                assert package is not None and release is not None
                report["deploymentAttempted"] = True
                owned_network = network
                network = (
                    None  # transaction owns/restores exactly once, including on failure
                )
                deploy_transaction(
                    device,
                    owner,
                    package.read_bytes(),
                    release,
                    owned_network,
                    report,
                    activate,
                    witness,
                )
                network = None
            else:
                try:
                    activate()
                    report["activationRequested"] = True
                    report["activationMethod"] = args.activation
                    report["stage"] = "maintenance-requested"
                    report["wifi"] = network.connect(
                        owner["ssid"], owner["apPassword"], device.get("bssid")
                    )
                    api = network.api()
                    info = validate_target(api.request("/api/info"), device)
                    report["info"] = info
                    report["stage"] = "status-verified"
                    if args.operation == "verify":
                        require(
                            release is not None,
                            "package",
                            "Verify requires signed --package to pin image/version",
                        )
                        assert release is not None
                        report["healthEvidence"] = verify_health(
                            info,
                            release["build"],
                            info["slot"],
                            release["imageSha256"],
                            witness,
                        )
                    report["statusSuccess"] = True
                finally:
                    owned_network = network
                    network = None
                    try:
                        report["networkRestoration"] = {"restored": False}
                        report["networkRestoration"] = owned_network.restore()
                    finally:
                        report["ethernetAfter"] = Network.internet()
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
