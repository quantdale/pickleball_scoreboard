"""Deterministic deployment fault injection, never accesses hardware/networks."""

import hashlib
import json
import unittest
from pathlib import Path
from typing import Any
from unittest.mock import AsyncMock, Mock, patch

import ota_deploy as d
import ota_windows as windows

DEVICE = {"mac": "3c:8a:1f:a1:ba:28", "httpDeviceId": "PickleScore-28baa11f8a3c"}
OWNER = {
    "ssid": "TEST-ONLY",
    "apPassword": "test-not-a-production-secret",
    "maintenanceKey": "0" * 64,
}
RELEASE = {"build": "new", "imageSha256": "a" * 64, "imageBytes": 320}
SOURCE = {"build": "old", "imageSha256": "b" * 64, "slot": "ota_0"}


class FakeApi:
    def __init__(self, network):
        self.network = network
        self.token = None

    def request(self, path, method="GET"):
        n = self.network
        if path == "/api/close":
            n.closed = True
            return {"closed": True}
        return {
            "device": n.identity,
            "build": n.build if n.installed else "old",
            "slot": n.slot if n.installed else n.source_slot,
            "limit": d.ota_tool.SLOT_SIZE,
            "agentProtocol": 1,
            "imageSha256": n.digest if n.installed else n.source_digest,
            "healthy": n.healthy if n.installed else n.source_confirmed,
            "confirmed": n.healthy if n.installed else n.source_confirmed,
            "bootState": "valid"
            if (n.healthy if n.installed else n.source_confirmed)
            else "pending",
            "unexpectedPrivateField": "should-not-be-copied",
        }

    def login(self, key):
        if self.network.failure == "authentication":
            raise d.DeploymentError("http_401", "rejected")
        self.token = bytes(range(32)).hex()

    def status(self):
        return self.network.status

    def close(self):
        self.request("/api/close", "POST")
        self.token = None

    def upload(self, package):
        if self.network.failure in ["interrupted", "signature", "partition"]:
            raise d.DeploymentError(self.network.failure, "rejected")
        self.network.uploaded = True
        return {"transferAccepted": True}

    def verify_staged(self, build, image_bytes):
        if self.network.failure == "stage_mismatch":
            raise d.DeploymentError("stage_verification", "wrong staged image extent")
        return {
            "flashReadbackVerifiedByDevice": True,
            "stagedBuild": build,
            "stagedBytes": image_bytes,
        }

    def install(self):
        self.network.install_count += 1
        if self.network.failure == "uncertain_install":
            raise d.DeploymentError("transport", "uncertain")
        self.network.installed = True
        if self.network.failure == "uncertain_completed":
            raise d.DeploymentError("transport", "lost response after device accepted")


class FakeNetwork:
    def __init__(self, failure=None):
        self.failure = failure
        self.restored = False
        self.closed = False
        self.installed = False
        self.uploaded = False
        self.install_count = 0
        self.internet_count = 0
        self.identity = DEVICE["httpDeviceId"]
        self.build = "new"
        self.slot = "ota_1"
        self.digest = "a" * 64
        self.healthy = True
        self.source_slot = SOURCE["slot"]
        self.source_digest = SOURCE["imageSha256"]
        self.source_confirmed = True
        self.status = {"phase": "session", "heap": 62000}
        self.release_count = 0

    def release_for_reboot(self):
        self.release_count += 1
        if self.failure == "release_link":
            raise RuntimeError("test native link release failure")

    def internet(self):
        self.internet_count += 1
        if self.failure == "ethernet":
            raise RuntimeError("Ethernet unavailable")
        return {"tlsInternetVerified": True}

    def connect(self, *args, **kwargs):
        if self.failure in ["adapter", "credentials", "window"] or (
            self.installed and self.failure == "reboot"
        ):
            raise RuntimeError("WLAN refused/timed out")
        return {"sourceAddress": "192.168.4.2", "ssid": "TEST-ONLY"}

    def api(self):
        return FakeApi(self)

    def restore(self):
        self.restored = True
        return {"restored": self.failure != "restoration"}


class LifecycleTest(unittest.TestCase):
    def run_case(self, network, expected_source=SOURCE, report=None, activate=lambda: None):
        t = [0]

        def sleep(seconds):
            t[0] += seconds

        if report is None:
            report = {}
        try:
            d.deploy_transaction(
                DEVICE,
                OWNER,
                b"test-only",
                RELEASE,
                network,
                report,
                activate,
                sleep=sleep,
                clock=lambda: t[0],
                expected_source=expected_source,
            )
        finally:
            self.assertTrue(network.restored)
        return report

    def test_complete_includes_health_and_restoration(self):
        network = FakeNetwork()
        report = self.run_case(network)
        self.assertTrue(report["deploymentSuccess"])
        self.assertTrue(network.closed)
        self.assertEqual(network.install_count, 1)
        self.assertEqual(network.internet_count, 3)
        self.assertEqual(network.release_count, 1)
        self.assertEqual(report["preUploadMemory"], {"heap": 62000})

    def test_busy_controller_is_never_evicted_or_uploaded_to(self):
        network = FakeNetwork()
        activate = Mock(side_effect=d.DeploymentError("ble_unavailable", "occupied"))
        with self.assertRaisesRegex(d.DeploymentError, "occupied"):
            self.run_case(network, activate=activate)
        activate.assert_called_once_with()
        self.assertFalse(network.uploaded)
        self.assertEqual(network.install_count, 0)

    def test_memory_shortage_or_non_idle_session_never_reaches_upload(self):
        for patch_status, reason in (
            ({"heap": 49151}, "memory_admission"),
            ({"heap": True}, "memory_admission"),
            ({"largestInternal8": 4095}, "memory_admission"),
            ({"largestInternal8": None}, "memory_admission"),
            ({"phase": "staged"}, "maintenance_busy"),
        ):
            with self.subTest(status=patch_status):
                network = FakeNetwork()
                network.status.update(patch_status)
                with self.assertRaises(d.DeploymentError) as error:
                    self.run_case(network)
                self.assertEqual(error.exception.code, reason)
                self.assertFalse(network.uploaded)
                self.assertEqual(network.install_count, 0)
                self.assertTrue(network.closed)

    def test_only_current_heap_and_contiguous_memory_gate_the_transfer(self):
        network = FakeNetwork()
        network.status.update(heap=49152, largestInternal8=4096, minimumHeap=32000, httpStackFree=3680)
        report = self.run_case(network)
        self.assertEqual(report["candidateMemory"]["httpStackFree"], 3680)
        self.assertEqual(report["preUploadMemory"]["minimumHeap"], 32000)
        self.assertTrue(report["deploymentSuccess"])

    def test_link_release_failure_still_observes_without_replaying_install(self):
        network = FakeNetwork("release_link")
        report = self.run_case(network)
        self.assertIs(report["sourceLinkReleased"], False)
        self.assertTrue(report["installationVerified"])
        self.assertEqual(network.install_count, 1)

    def test_live_source_must_match_owner_authorization_before_upload(self):
        for field, value in [
            ("source_digest", "c" * 64),
            ("source_slot", "ota_1"),
            ("source_confirmed", False),
        ]:
            with self.subTest(field=field):
                network = FakeNetwork()
                setattr(network, field, value)
                with self.assertRaises(d.DeploymentError) as error:
                    self.run_case(network, expected_source=SOURCE)
                self.assertEqual(error.exception.code, "source_mismatch")
                self.assertFalse(network.uploaded)
                self.assertTrue(network.closed)
                self.assertEqual(network.install_count, 0)

    def test_report_only_includes_known_target_fields(self):
        report = self.run_case(FakeNetwork(), expected_source=SOURCE)
        self.assertNotIn("unexpectedPrivateField", report["before"])
        self.assertNotIn("unexpectedPrivateField", report["after"])

    def test_upload_ack_without_verified_extent_never_installs(self):
        network = FakeNetwork("stage_mismatch")
        report = {}
        with self.assertRaises(d.DeploymentError) as error:
            self.run_case(network, report=report)
        self.assertEqual(error.exception.code, "stage_verification")
        self.assertEqual(report["stage"], "upload-acknowledged")
        self.assertNotIn("flashReadbackVerifiedByDevice", report["transfer"])
        self.assertTrue(network.uploaded)
        self.assertEqual(network.install_count, 0)
        self.assertTrue(network.closed)

    def test_false_restoration_cannot_return_success(self):
        with self.assertRaises(d.DeploymentError) as error:
            self.run_case(FakeNetwork("restoration"))
        self.assertEqual(error.exception.code, "network_restoration")

    def test_adapter_unavailable(self):
        self.failure("adapter")

    def test_bad_ap_credentials(self):
        self.failure("credentials")

    def test_ap_window_expired(self):
        self.failure("window")

    def test_ethernet_lost(self):
        self.failure("ethernet")

    def test_authentication(self):
        self.failure("authentication")

    def test_interruption_never_installs(self):
        self.failure("interrupted")

    def test_signature_never_installs(self):
        self.failure("signature")

    def test_partition_never_installs(self):
        self.failure("partition")

    def test_uncertain_install_not_retried(self):
        network = FakeNetwork("uncertain_install")
        with self.assertRaises(d.DeploymentError):
            self.run_case(network)
        self.assertEqual(network.install_count, 1)

    def test_uncertain_install_observed_without_replay(self):
        network = FakeNetwork("uncertain_completed")
        report = self.run_case(network)
        self.assertTrue(report["deploymentSuccess"])
        self.assertTrue(report["installOutcomeUncertain"])
        self.assertTrue(report["installationVerified"])
        self.assertNotIn("installAccepted", report)
        self.assertEqual(network.install_count, 1)

    def test_wrong_device_never_authenticates_or_uploads(self):
        network = FakeNetwork()
        network.identity = "wrong"
        with self.assertRaisesRegex(d.DeploymentError, "identity"):
            self.run_case(network)
        self.assertFalse(network.uploaded)

    def test_reboot_timeout(self):
        self.failure("reboot")

    def test_wrong_version_rollback(self):
        network = FakeNetwork()
        network.build = "old"
        with self.assertRaises(d.DeploymentError) as error:
            self.run_case(network)
        self.assertEqual(error.exception.code, "version_mismatch")

    def test_wrong_partition(self):
        network = FakeNetwork()
        network.slot = "ota_0"
        with self.assertRaises(d.DeploymentError) as error:
            self.run_case(network)
        self.assertEqual(error.exception.code, "slot_mismatch")

    def test_wrong_digest(self):
        network = FakeNetwork()
        network.digest = "b" * 64
        with self.assertRaises(d.DeploymentError) as error:
            self.run_case(network)
        self.assertEqual(error.exception.code, "image_mismatch")

    def test_unconfirmed_health(self):
        network = FakeNetwork()
        network.healthy = False
        report = {}
        with self.assertRaises(d.DeploymentError) as error:
            self.run_case(network, report=report)
        self.assertEqual(error.exception.code, "health_unconfirmed")
        self.assertEqual(report["stage"], "expected-image-booted")
        self.assertNotIn("deploymentSuccess", report)

    def failure(self, kind):
        network = FakeNetwork(kind)
        with self.assertRaises((RuntimeError, d.DeploymentError)):
            self.run_case(network)
        if kind in [
            "authentication",
            "interrupted",
            "signature",
            "partition",
            "adapter",
            "credentials",
            "window",
        ]:
            self.assertEqual(network.install_count, 0)


class AuthorizationTest(unittest.TestCase):
    def run_cli(self, operation, flags, case):
        digest = "a" * 64
        argv = ["ota_deploy.py", operation, "--device", "court1", "--output", "unused", *flags]
        if operation == "deploy":
            argv += ["--package", "test.pksota", "--expect-package-sha256", digest,
                     "--expect-source-build", "old", "--expect-source-slot", "ota_0",
                     "--expect-source-sha256", "b" * 64]
        written = {}

        def write(path, body):
            written.update(json.loads(body))

        with (
            patch.object(d.sys, "argv", argv),
            patch.object(d.os, "name", "nt"),
            patch.object(d.ota_tool, "outside"),
            patch.object(Path, "mkdir"),
            patch.object(Path, "open") as opened,
            patch.object(Path, "unlink"),
            patch.object(Path, "write_text", autospec=True, side_effect=write),
            patch.object(d, "load_device", return_value=({**DEVICE, "ownerCredentials": "test.json", "signingPublicKey": "pub.pem"}, OWNER)),
            patch.object(windows, "DeploymentLock"),
            patch.object(d, "SerialWitness") as serial,
            patch.object(d.Network, "internet", return_value={"tlsInternetVerified": True}),
            patch.object(d.Network, "connect", side_effect=d.DeploymentError("test_stop", "no radio")),
            patch.object(d, "activate_ble", AsyncMock()) as ble,
            patch.object(d.ota_tool, "verify_bytes", return_value={**RELEASE, "packageSha256": digest if case != "package_mismatch" else "c" * 64}),
            patch.object(d, "deploy_transaction") as deploy,
            patch("builtins.print"),
        ):
            opened.return_value.__enter__.return_value.read.return_value = b"test"
            d.main()
            return written, deploy, serial, ble

    def test_routine_wireless_requires_fresh_attestations_not_usb(self):
        required = ["--confirm-installed-ready", "--authorize-score-reset", "--authorize-write-and-fallback"]
        for omitted in range(len(required)):
            with self.subTest(flag=required[omitted]):
                report, deploy, serial, ble = self.run_cli("deploy", [f for f in required if f != required[omitted]], "blocked")
                self.assertEqual(report["failure"]["code"], "authorization")
                deploy.assert_not_called()
                serial.assert_not_called()
                ble.assert_not_called()
        report, deploy, serial, _ = self.run_cli("deploy", required, "authorized")
        self.assertEqual(report["operationMode"], "installed-wireless")
        self.assertNotIn("failure", report)
        deploy.assert_called_once()
        serial.assert_not_called()

    def test_wrong_signed_package_digest_blocks_activation(self):
        report, deploy, serial, ble = self.run_cli("deploy", ["--confirm-installed-ready", "--authorize-score-reset",
                                                               "--authorize-write-and-fallback"], "package_mismatch")
        self.assertEqual(report["failure"]["code"], "package_mismatch")
        deploy.assert_not_called()
        serial.assert_not_called()
        ble.assert_not_called()

    def test_usb_requires_isolation_and_is_never_a_deploy_mode(self):
        flags = ["--activation", "usb", "--port", "COM11", "--authorize-score-reset"]
        for extra in ([], ["--confirm-isolated-usb-only", "--confirm-installed-ready"]):
            report, deploy, serial, ble = self.run_cli("status", flags + extra, "blocked")
            self.assertEqual(report["failure"]["code"], "electrical_safety")
            deploy.assert_not_called()
            serial.assert_not_called()
            ble.assert_not_called()
        isolated = flags + ["--confirm-isolated-usb-only"]
        report, _, serial, _ = self.run_cli("status", isolated, "authorized")
        self.assertEqual(report["operationMode"], "isolated-usb-commissioning")
        serial.assert_called_once()
        report, deploy, serial, _ = self.run_cli("deploy", isolated + ["--authorize-write-and-fallback"], "blocked")
        self.assertEqual(report["failure"]["code"], "activation")
        deploy.assert_not_called()
        serial.assert_not_called()


class ProtocolTest(unittest.TestCase):
    def test_proof_exact_domain_target_and_flag(self):
        challenge = b"\1" + bytes.fromhex(DEVICE["mac"].replace(":", "")) + b"x" * 32
        expected = d.hmac.digest(
            hashlib.sha256(OWNER["maintenanceKey"].encode()).digest(),
            d.DOMAIN + challenge + b"\1",
            "sha256",
        )
        self.assertEqual(
            d.proof(challenge, DEVICE["mac"], OWNER["maintenanceKey"]),
            b"\1\1" + expected,
        )
        with self.assertRaises(d.DeploymentError):
            d.proof(challenge, "00:00:00:00:00:00", OWNER["maintenanceKey"])

    def test_legacy_no_false_health_success(self):
        with self.assertRaises(d.DeploymentError):
            d.verify_health({"build": "new", "slot": "ota_1"}, "new", "ota_1", "a" * 64)

    def test_exact_http_headers_source_binding_no_browser(self):
        response = Mock()
        response.status = 200
        response.read.return_value = json.dumps({"device": "test"}).encode()
        connection = Mock()
        connection.getresponse.return_value = response
        with patch("http.client.HTTPConnection", return_value=connection) as factory:
            api = d.Api("192.168.4.2")
            api.token = "t" * 64
            api.nonce = "n" * 64
            api.request(
                "/api/upload",
                "POST",
                b"bytes",
                {"Content-Type": "application/octet-stream"},
            )
            factory.assert_called_once_with(
                "192.168.4.1", 80, timeout=10, source_address=("192.168.4.2", 0)
            )
            headers = connection.request.call_args.kwargs["headers"]
            self.assertEqual(headers["Origin"], "http://192.168.4.1")
            self.assertEqual(headers["Host"], "192.168.4.1")
            self.assertEqual(headers["X-CSRF-Token"], "n" * 64)
            self.assertNotIn("Cookie", headers)

    def test_arbitrary_error_body_not_logged(self):
        response = Mock()
        response.status = 401
        response.read.return_value = b'{"error":"secret-content"}'
        connection = Mock()
        connection.getresponse.return_value = response
        with (
            patch("http.client.HTTPConnection", return_value=connection),
            self.assertRaises(d.DeploymentError) as error,
        ):
            d.Api("192.168.4.2").request("/api/info")
        self.assertNotIn("secret-content", str(error.exception))

    def test_nonce_refresh_between_upload_and_install(self):
        api = d.Api("192.168.4.2")
        calls = []

        def request(path, *args, **kwargs):
            calls.append(path)
            if path == "/api/status":
                return {
                    "nonce": "0" * 64,
                    "phase": "staged",
                    "stagedBuild": "new",
                    "received": 320,
                    "total": 320,
                }
            if path == "/api/upload":
                return {"staged": True}
            return {"installing": True}

        api.request = request
        api.status()  # authenticated pre-write status owns the upload nonce
        api.upload(b"test")
        api.verify_staged("new", 320)
        api.install()
        self.assertEqual(
            calls,
            [
                "/api/status",
                "/api/upload",
                "/api/status",
                "/api/install",
            ],
        )

    def test_stage_requires_exact_verified_extent(self):
        api = d.Api("192.168.4.2")
        api.status = Mock(
            return_value={
                "phase": "staged",
                "stagedBuild": "new",
                "received": 319,
                "total": 320,
            }
        )
        with self.assertRaises(d.DeploymentError) as error:
            api.verify_staged("new", 320)
        self.assertEqual(error.exception.code, "stage_verification")


class BleakRetryTest(unittest.IsolatedAsyncioTestCase):
    async def test_retry_only_before_proof(self):
        operation = AsyncMock(
            side_effect=[
                d.DeploymentError("ble_timeout_connect-services", "timeout"),
                None,
            ]
        )
        with (
            patch.object(d, "_activate_ble_once", operation),
            patch.object(d.asyncio, "sleep", AsyncMock()),
        ):
            await d.activate_ble(DEVICE, OWNER)
        self.assertEqual(operation.await_count, 2)
        assert operation.await_args is not None
        self.assertFalse(operation.await_args.kwargs["cached"])

    async def test_never_replay_after_proof_write(self):
        operation = AsyncMock(
            side_effect=d.DeploymentError("ble_timeout_proof-write", "uncertain")
        )
        with (
            patch.object(d, "_activate_ble_once", operation),
            self.assertRaises(d.DeploymentError),
        ):
            await d.activate_ble(DEVICE, OWNER)
        self.assertEqual(operation.await_count, 1)


class WindowsAdapterTest(unittest.TestCase):
    def adapter(self) -> Any:
        adapter: Any = object.__new__(windows.WindowsWifi)
        adapter.dll = Mock()
        adapter.dll.WlanDisconnect.return_value = 0
        adapter.dll.WlanDeleteProfile.return_value = 0
        adapter.guid = windows.Guid()
        adapter.handle = None
        adapter.temporary = "test-only-temporary-profile"
        adapter.saved = None
        adapter.saved_radios = [(0, 2, 1)]
        radios = windows.Radios()
        radios.count = 1
        radios.radios[0] = windows.Radio(0, 2, 1)
        adapter.inspect_interface = Mock(return_value=radios)
        adapter.set_radio = Mock()
        adapter.connection = Mock(return_value=None)
        adapter.connect_profile = Mock()
        return adapter

    def test_reboot_link_release_preserves_snapshot_for_final_restore(self):
        adapter = self.adapter()
        adapter.dll.WlanDisconnect.return_value = 5023  # already disconnected
        adapter.release_for_reboot()
        adapter.dll.WlanDisconnect.assert_called_once()
        adapter.dll.WlanDeleteProfile.assert_not_called()
        adapter.set_radio.assert_not_called()
        self.assertIsNotNone(adapter.temporary)

    def test_restore_original_off_disconnected_state(self):
        adapter = self.adapter()
        self.assertTrue(adapter.restore()["restored"])
        adapter.set_radio.assert_called_once_with(0, 2)
        adapter.dll.WlanDeleteProfile.assert_called_once()
        adapter.dll.WlanCloseHandle.assert_called_once()

    def test_restore_previous_profile(self):
        adapter = self.adapter()
        adapter.saved = {"profile": "prior-owner-profile"}
        adapter.connection.return_value = adapter.saved
        self.assertTrue(adapter.restore()["restored"])
        adapter.connect_profile.assert_called_once_with("prior-owner-profile")

    def test_failed_restore_is_not_success(self):
        adapter = self.adapter()
        adapter.dll.WlanDeleteProfile.return_value = 5
        with self.assertRaisesRegex(RuntimeError, "restoration failed"):
            adapter.restore()
        adapter.dll.WlanCloseHandle.assert_called_once()
        adapter.set_radio.assert_called_once_with(0, 2)

    def test_transient_authentication_is_not_connected(self):
        adapter = self.adapter()
        del adapter.connection
        value = windows.Connection()
        value.state = 7
        adapter.inspect_interface.return_value = value
        self.assertIsNone(adapter.connection())

    def test_existing_evidence_is_never_overwritten(self):
        with (
            patch.object(
                d.sys,
                "argv",
                [
                    "ota_deploy.py",
                    "preflight",
                    "--device",
                    "court1",
                    "--output",
                    "D:/owner-existing-session",
                ],
            ),
            patch.object(d.Path, "mkdir", side_effect=FileExistsError),
            patch.object(d.Path, "write_text") as write,
            patch("builtins.print"),
        ):
            self.assertEqual(d.main(), 1)
        write.assert_not_called()


if __name__ == "__main__":
    unittest.main(verbosity=2)
