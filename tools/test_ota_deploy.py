"""Deterministic deployment fault injection, never accesses hardware/networks."""

import hashlib
import json
import unittest
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
RELEASE = {"build": "new", "imageSha256": "a" * 64}


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
            "slot": n.slot if n.installed else "ota_0",
            "limit": d.ota_tool.SLOT_SIZE,
            "agentProtocol": 1,
            "imageSha256": n.digest,
            "healthy": n.healthy,
            "confirmed": n.healthy,
            "bootState": "valid" if n.healthy else "pending",
        }

    def login(self, key):
        if self.network.failure == "authentication":
            raise d.DeploymentError("http_401", "rejected")
        self.token = bytes(range(32)).hex()

    def status(self):
        return {"phase": "staged"}

    def upload(self, package, build):
        if self.network.failure in ["interrupted", "signature", "partition"]:
            raise d.DeploymentError(self.network.failure, "rejected")
        self.network.uploaded = True
        return {"transferAccepted": True, "stagedBuild": build}

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
        return {"restored": True}


class LifecycleTest(unittest.TestCase):
    def run_case(self, network):
        t = [0]

        def sleep(seconds):
            t[0] += seconds

        report = {}
        try:
            d.deploy_transaction(
                DEVICE,
                OWNER,
                b"test-only",
                RELEASE,
                network,
                report,
                lambda: None,
                sleep=sleep,
                clock=lambda: t[0],
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
        with self.assertRaises(d.DeploymentError) as error:
            self.run_case(network)
        self.assertEqual(error.exception.code, "health_unconfirmed")

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
                return {"nonce": "0" * 64, "phase": "staged", "stagedBuild": "new"}
            if path == "/api/upload":
                return {"staged": True}
            return {"installing": True}

        api.request = request
        api.upload(b"test", "new")
        api.install()
        self.assertEqual(
            calls,
            [
                "/api/status",
                "/api/upload",
                "/api/status",
                "/api/status",
                "/api/install",
            ],
        )


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
