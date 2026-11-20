"""Deterministic deployment fault injection, never accesses hardware/networks."""

import hashlib
import json
import unittest
from ipaddress import IPv4Address
from pathlib import Path
from typing import Any
from unittest.mock import AsyncMock, Mock, call, patch

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

    def public_info(self):
        return self.request("/api/info")

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
        self.network.upload_count += 1
        if self.network.failure == "upload_uncertain":
            raise d.DeploymentError("transport", "lost upload response")
        if self.network.failure in ["interrupted", "signature", "partition"]:
            raise d.DeploymentError(self.network.failure, "rejected")
        self.network.uploaded = True
        return {"transferAccepted": True}

    def verify_staged(self, build, image_bytes):
        if self.network.failure in ("stage_mismatch", "upload_uncertain"):
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
        self.upload_count = 0

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

    def test_readiness_failure_survives_restore_and_ethernet_failures(self):
        network = FakeNetwork()
        network.connect = Mock(side_effect=d.DeploymentError("dhcp_apipa", "APIPA only"))
        network.internet = Mock(side_effect=[{"tlsInternetVerified": True}, RuntimeError("offline")])
        restore = network.restore

        def failed_restore():
            restore()
            raise OSError("mock restoration failed")

        network.restore = failed_restore
        report = {}
        with self.assertRaises(d.DeploymentError) as result:
            self.run_case(network, report=report)
        self.assertEqual(result.exception.code, "dhcp_apipa")
        network.connect.assert_called_once()
        self.assertEqual(report["maintenanceClosure"], "no_authenticated_session")
        self.assertIs(report["bleRecoveryVerified"], False)
        self.assertEqual(report["cleanupFailures"], {"wifi": "OSError", "ethernet": "RuntimeError"})
        self.assertFalse(network.uploaded)
        self.assertEqual(network.install_count, 0)

    def test_fresh_attempt_after_resolved_prewrite_timeout(self):
        first = FakeNetwork()
        first.connect = Mock(side_effect=d.DeploymentError("dhcp_apipa", "APIPA"))
        report = {}
        with self.assertRaises(d.DeploymentError):
            self.run_case(first, report=report)
        self.assertFalse(report.get("wirelessTransferAttempted", False))
        self.assertTrue(report["networkRestoration"]["restored"])
        self.assertEqual(first.upload_count, 0)
        # The caller resolves the previous maintenance window before explicitly running again.
        second = FakeNetwork()
        self.assertTrue(self.run_case(second)["installationVerified"])
        self.assertEqual(second.upload_count, 1)
        self.assertEqual(second.install_count, 1)

    def test_uncertain_upload_is_never_repeated_or_installed(self):
        network = FakeNetwork("upload_uncertain")
        report = {}
        with self.assertRaisesRegex(d.DeploymentError, "lost upload response"):
            self.run_case(network, report=report)
        self.assertTrue(report["wirelessTransferAttempted"])
        self.assertEqual((network.upload_count, network.install_count), (1, 0))
        self.assertTrue(report["uploadOutcomeUncertain"])
        self.assertEqual(report["uploadObservationFailure"], "stage_verification")

    def test_failed_close_is_not_retried_and_network_is_restored(self):
        network = FakeNetwork()
        close = Mock(side_effect=d.DeploymentError("close_unconfirmed", "no acknowledgement"))
        api = network.api

        def with_failed_close():
            result = api()
            result.close = close
            return result

        network.api = with_failed_close
        report = {}
        with self.assertRaises(d.DeploymentError) as result:
            self.run_case(network, report=report)
        self.assertEqual(result.exception.code, "close_unconfirmed")
        self.assertEqual(report["maintenanceClosure"], "close_unconfirmed")
        self.assertNotIn("maintenanceCloseAccepted", report)
        self.assertTrue(report["networkRestoration"]["restored"])
        self.assertTrue(report["ethernetAfter"]["tlsInternetVerified"])
        self.assertIs(report["bleRecoveryVerified"], False)
        self.assertEqual(network.install_count, 1)
        close.assert_called_once_with()

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
    @staticmethod
    def windows_platform():
        # Mock the CLI's platform gate, not the process-wide os.name that
        # pathlib uses to select a constructible path type on the CI host.
        platform = Mock(wraps=d.os)
        platform.name = "nt"
        return patch.object(d, "os", platform)

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
            self.windows_platform(),
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

    def test_bounded_read_only_verify_pins_slot_and_checks_ble_only_after_close(self):
        argv = ["ota_deploy.py", "verify", "--device", "court1", "--output", "unused",
                "--package", "test.pksota", "--expect-package-sha256", "c" * 64,
                "--expect-source-slot", "ota_0", "--confirm-installed-ready",
                "--authorize-score-reset", "--verify-ble-recovery"]
        for slot in ("ota_0", "ota_1"):
            with self.subTest(slot=slot):
                network: Any = FakeNetwork()
                network.wifi_attempts = []
                network.source_slot = slot
                network.observe_close = Mock(return_value=True)
                api = FakeApi(network)
                api.upload = Mock()
                api.install = Mock()
                network.api = Mock(return_value=api)

                async def observe(_device, session=network):
                    self.assertTrue(session.closed)
                    self.assertTrue(session.restored)

                with (patch.object(d.sys, "argv", argv), self.windows_platform(),
                      patch.object(d.ota_tool, "outside"), patch.object(Path, "mkdir"),
                      patch.object(Path, "open"), patch.object(Path, "unlink"),
                      patch.object(Path, "write_text") as write, patch("builtins.print"),
                      patch.object(windows, "DeploymentLock"),
                      patch.object(d, "Network", return_value=network, internet=network.internet),
                      patch.object(d, "load_device", return_value=({**DEVICE, "ownerCredentials": "test.json",
                                                                   "signingPublicKey": "pub.pem"}, OWNER)),
                      patch.object(d.ota_tool, "verify_bytes", return_value={**SOURCE, "packageSha256": "c" * 64}),
                      patch.object(d, "_activate_ble_once", AsyncMock()) as activate,
                      patch.object(d, "verify_ble_recovery", AsyncMock(side_effect=observe)) as ble):
                    self.assertEqual(d.main(), 0 if slot == "ota_0" else 1)
                report = json.loads(write.call_args.args[0])
                self.assertEqual(report["bleRecoveryVerified"], slot == "ota_0")
                self.assertTrue(report["maintenanceLinkDownObserved"])
                activate.assert_awaited_once()
                self.assertEqual(ble.await_count, int(slot == "ota_0"))
                api.upload.assert_not_called()
                api.install.assert_not_called()

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
    def test_public_probe_retries_transport_once_only(self):
        api = d.Api('192.168.4.210')
        error = d.DeploymentError('transport', 'response-headers')
        with patch.object(api, 'request', side_effect=[error, {'device': 'court'}]) as request, patch.object(d.time, 'sleep'):
            self.assertEqual(api.public_info(), {'device': 'court'})
            self.assertEqual(request.call_count, 2)
            self.assertTrue(all(call.args == ('/api/info',) for call in request.call_args_list))
        with patch.object(api, 'request', side_effect=error) as request, patch.object(d.time, 'sleep'):
            with self.assertRaisesRegex(d.DeploymentError, 'recheck: response-headers'):
                api.public_info()
            self.assertEqual(request.call_count, 2)

    def test_public_probe_does_not_retry_http_rejection(self):
        api = d.Api('192.168.4.210')
        with patch.object(api, 'request', side_effect=d.DeploymentError('http_403', 'rejected')) as request:
            with self.assertRaises(d.DeploymentError):
                api.public_info()
            request.assert_called_once()

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

    def test_route_or_identity_change_blocks_http_before_creating_socket(self):
        guard = Mock(side_effect=d.DeploymentError("route_wrong_interface", "foreign adapter"))
        with (patch.object(d.http.client, "HTTPConnection") as connection,
              self.assertRaisesRegex(d.DeploymentError, "foreign adapter")):
            d.Api("192.168.4.210", guard=guard).request("/api/upload", "POST", b"signed")
        connection.assert_not_called()

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
    def fake_bleak(self, *observations):
        target_scan = AsyncMock(side_effect=observations)
        context = AsyncMock()
        client = context.__aenter__.return_value
        client.read_gatt_char = AsyncMock(side_effect=[bytes.fromhex('013c8a1fa1ba28') + bytes(32), b'OK'])
        modules = {
            'bleak': Mock(BleakScanner=Mock(find_device_by_address=target_scan), BleakClient=Mock(return_value=context)),
            'bleak.exc': Mock(BleakError=type('BleakError', (Exception,), {})),
        }
        return patch.dict(d.sys.modules, modules), target_scan, modules['bleak'].BleakClient, client

    async def test_discovery_uses_one_passive_window_after_active_miss(self):
        device = {**DEVICE, 'bleAddress': '3c:8a:1f:a1:ba:2a'}
        target = Mock(address=device['bleAddress'].upper())
        modules, scan, connect, client = self.fake_bleak(None, target)
        with modules:
            await d._activate_ble_once(device, OWNER, cached=False)
        self.assertEqual(scan.await_args_list, [call(device['bleAddress'], timeout=15, scanning_mode='active'),
                                               call(device['bleAddress'], timeout=8, scanning_mode='passive')])
        connect.assert_called_once_with(target, timeout=30, winrt={'use_cached_services': False})
        client.write_gatt_char.assert_awaited_once()

    async def test_no_connection_after_two_misses_or_wrong_passive_address(self):
        device = {**DEVICE, 'bleAddress': '3c:8a:1f:a1:ba:2a'}
        for observed in (None, Mock(address='3C:8A:1F:A1:BA:29')):
            with self.subTest(observed=observed):
                modules, scan, connect, client = self.fake_bleak(None, observed)
                with modules, self.assertRaises(d.DeploymentError) as error:
                    await d.activate_ble(device, OWNER)
                self.assertEqual(error.exception.code, 'ble_unavailable')
                self.assertEqual(scan.await_count, 2)
                connect.assert_not_called()
                client.write_gatt_char.assert_not_called()

    async def test_recovery_uses_passive_fallback_without_application_writes(self):
        device = {**DEVICE, 'bleAddress': '3c:8a:1f:a1:ba:2a'}
        target = Mock(address=device['bleAddress'])
        modules, scan, connect, client = self.fake_bleak(None, target)

        async def notify(characteristic, callback):
            self.assertEqual(characteristic, d.STATE_UUID)
            callback(characteristic, b'0,0,L,2,0,D')

        client.start_notify.side_effect = notify
        with modules:
            await d.verify_ble_recovery(device)
        self.assertEqual(scan.await_args_list, [call(device['bleAddress'], timeout=12, scanning_mode='active'),
                                               call(device['bleAddress'], timeout=8, scanning_mode='passive')])
        connect.assert_called_once_with(target, timeout=10)
        client.write_gatt_char.assert_not_called()

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


class WindowsReadinessTest(unittest.TestCase):
    BSSID = "3c:8a:1f:a1:ba:29"

    def snapshot(self, state="Preferred", address="192.168.4.2", origin="Dhcp", prefix=24):
        on_link = str(IPv4Address(0))
        return {"addresses": [{"address": address, "state": state, "origin": origin,
                               "prefixLength": prefix, "skipAsSource": False}],
                "routes": [{"index": 15, "prefix": "192.168.4.0/24", "nextHop": on_link}],
                "selection": [{"kind": "address", "index": 15, "address": address},
                              {"kind": "route", "index": 15, "prefix": "192.168.4.0/24", "nextHop": on_link}],
                "dhcpEnabled": True, "interfaceState": "Connected"}

    def run_wlan(self, snapshots, *, bssid=BSSID, timeout=6, expected=None, tcp_error=None,
                 prior_link=False, elevated=False, static=False, duplicate=False, foreign_routes=(), peer=BSSID):
        clock = [0.]
        def sleep(seconds):
            clock[0] += seconds
        count = iter(range(200))

        def inspect():
            if static and adapter.static_address:
                state = "Duplicate" if duplicate and adapter.static_address == windows.STATIC_ADDRESSES[0] else "Preferred"
                return self.snapshot(state=state, address=adapter.static_address, origin="Manual")
            return snapshots[min(next(count), len(snapshots) - 1)]
        adapter: Any = object.__new__(windows.WindowsWifi)
        adapter.index = 15
        adapter.luid = 150
        adapter.static_address = adapter.static_policy = None
        adapter.ip = Mock()
        adapter.ip.addresses.return_value = []
        adapter.ip.routes.return_value = list(foreign_routes)
        adapter.ip.add.return_value = 0
        adapter.ip.neighbor.side_effect = lambda address, *_: peer if address == windows.AP_ADDRESS else None
        adapter.snapshot = Mock(side_effect=inspect)
        adapter.dll = Mock()
        adapter.guid = windows.Guid()
        adapter.handle = None
        adapter.temporary = None
        adapter.saved_radios = [(0, 1, 1)]
        adapter.saved = {"ssid": "prior", "bssid": "00:11:22:33:44:55", "profile": "prior", "auth": 7, "cipher": 4} if prior_link else None
        adapter.attempts = []
        adapter.connect_profile = Mock()
        adapter.fresh_bss = Mock(return_value={"ssid": OWNER["ssid"], "bssid": self.BSSID})
        adapter.connection = lambda: adapter.saved if prior_link and clock[0] < 1 else {
            "ssid": OWNER["ssid"], "bssid": bssid, "profile": adapter.temporary, "auth": 7, "cipher": 4}
        with (patch.object(windows.time, "sleep", side_effect=sleep),
              patch.object(windows.time, "monotonic", side_effect=lambda: clock[0]),
              patch.object(windows, "ipv4_policy", return_value={"dad": 3, "dhcp": 1, "dns": []}),
              patch.object(windows.C, "windll", create=True) as windll,
              patch.object(windows.socket, "create_connection", side_effect=tcp_error) as tcp):
            windll.shell32.IsUserAnAdmin.return_value = elevated
            if expected:
                with self.assertRaises(windows.WifiError) as error:
                    adapter.connect(OWNER["ssid"], OWNER["apPassword"], self.BSSID, timeout=timeout)
                self.assertEqual(error.exception.code, expected)
                self.assertLessEqual(clock[0], timeout)
            else:
                connected = adapter.connect(OWNER["ssid"], OWNER["apPassword"], self.BSSID, timeout=timeout)
                source = windows.STATIC_ADDRESSES[int(duplicate)] if static else "192.168.4.2"
                self.assertEqual(connected["sourceAddress"], source)
                self.assertEqual(connected["networkPath"], "temporary-static" if static else "dhcp")
                tcp.assert_called_once_with((windows.AP_ADDRESS, 80), timeout=1, source_address=(source, 0))
                if static:
                    self.assertEqual(adapter.ip.add.call_count, 1 + int(duplicate))
                    adapter.ip.add.assert_called_with(source, 150, 15, 24)
                    adapter.remove_static()
                    self.assertEqual(adapter.ip.delete.call_count, 1 + int(duplicate))
                    adapter.ip.delete.assert_called_with(source, 150, 15)
                    self.assertIsNone(adapter.static_address)
            adapter.connect_profile.assert_called_once_with(adapter.temporary, bssid=self.BSSID)
            attempt = adapter.attempts[0]
            self.assertNotIn(OWNER["apPassword"], json.dumps(attempt))
            return attempt, adapter.snapshot.call_count, tcp.call_count

    def test_association_without_completed_dhcp_times_out_before_tcp(self):
        report, _, tcp = self.run_wlan([self.snapshot(state="Tentative")], expected="dhcp_not_ready")
        self.assertIn("associatedSeconds", report)
        self.assertEqual(report["checks"][-1]["result"], "dhcp_not_ready")
        self.assertEqual(tcp, 0)

    def test_dhcp_eventually_ready(self):
        report, inspections, _ = self.run_wlan([self.snapshot(state="Tentative"), self.snapshot()])
        self.assertEqual(inspections, 3)
        self.assertEqual([row["result"] for row in report["checks"]], ["dhcp_not_ready", "ready"])
        self.assertTrue(report["tcp80Reachable"])

    def test_apipa_recovers_over_a_preferred_static_address(self):
        apipa = self.snapshot(address="169.254.202.214", origin="WellKnown", prefix=16)
        static = self.snapshot(address="192.168.4.10", origin="Manual")
        self.assertEqual(windows.local_readiness(apipa, 15), ("dhcp_apipa", None))
        self.assertEqual(windows.local_readiness(static, 15, static_address="192.168.4.10"),
                         ("route_ready", "192.168.4.10"))

    def test_apipa_trace_runs_the_real_static_fallback_and_cleans_up(self):
        apipa = self.snapshot(address="169.254.202.214", origin="WellKnown", prefix=16)
        report, _, _ = self.run_wlan([apipa], timeout=35, elevated=True, static=True)
        self.assertEqual(report["dhcpResult"], "dhcp_apipa")
        self.assertEqual(report["networkPath"], "temporary-static")

    def test_dad_conflict_is_removed_and_next_candidate_used(self):
        report, _, _ = self.run_wlan([self.snapshot(state="Tentative")], timeout=35,
                                     elevated=True, static=True, duplicate=True)
        self.assertEqual(len(report["staticAttempts"]), 2)
        self.assertIn("static_duplicate", {check["result"] for check in report["checks"]})

    def test_no_lease_requires_elevation_but_can_continue_dhcp_polling(self):
        report, _, tcp = self.run_wlan([self.snapshot(state="Tentative")], timeout=35,
                                       expected="static_permission_required")
        self.assertEqual(report["dhcpResult"], "dhcp_not_ready")
        self.assertEqual(tcp, 0)

    def test_conflicting_local_route_refuses_static(self):
        self.run_wlan([self.snapshot(state="Tentative")], timeout=35, elevated=True,
                      foreign_routes=({"prefix": "192.168.0.0/16", "index": 7},),
                      expected="static_route_conflict")

    def test_peer_must_match_ap_before_http(self):
        self.run_wlan([self.snapshot()], peer="00:11:22:33:44:55", expected="wifi_peer_identity")

    def test_apipa_is_not_a_dhcp_lease(self):
        _, _, tcp = self.run_wlan([self.snapshot(address="169.254.10.11", origin="WellKnown", prefix=16)],
                                  expected="dhcp_apipa")
        self.assertEqual(tcp, 0)

    def test_correct_ip_without_direct_route(self):
        self.run_wlan([{**self.snapshot(), "routes": []}], expected="route_not_on_link")

    def test_correct_ip_with_route_on_unexpected_adapter(self):
        snapshot = self.snapshot()
        snapshot["selection"][1]["index"] = 7
        self.run_wlan([snapshot], expected="route_wrong_interface")

    def test_valid_direct_local_route(self):
        report, _, _ = self.run_wlan([self.snapshot()])
        self.assertEqual(report["checks"][-1]["result"], "ready")

    def test_prior_saved_link_may_still_be_visible_while_switching(self):
        report, inspections, _ = self.run_wlan([self.snapshot()], prior_link=True)
        self.assertEqual(inspections, 2)
        self.assertEqual(report["checks"][0]["observed"]["ssid"], "prior")
        self.assertEqual(report["checks"][-1]["result"], "ready")

    def test_incorrect_bssid_never_reaches_network_or_tcp_probe(self):
        report, ps, tcp = self.run_wlan([], bssid="3c:8a:1f:a1:ba:99", timeout=35,
                                        elevated=True, expected="wifi_identity")
        self.assertEqual((ps, tcp), (0, 0))
        self.assertNotEqual(report["expectedBssid"], report["checks"][0]["observed"]["bssid"])

    def test_tcp_timeout_is_not_reported_as_dhcp_or_route_failure(self):
        report, _, tcp = self.run_wlan([self.snapshot()], expected="tcp80_unreachable", tcp_error=TimeoutError)
        self.assertGreater(tcp, 0)
        self.assertIs(report["tcp80Reachable"], False)

    def test_valid_route_is_not_lost_when_find_net_route_returns_route_first(self):
        # Find-NetRoute emits two CIM classes, not an index-addressed tuple.
        # Exercise the actual PowerShell projection in the host fixture below;
        # reversed tagged rows must work and the route's adapter must survive.
        on_link = str(IPv4Address(0))
        with patch.object(windows, "powershell", return_value=[
            {"kind": "route", "index": 15, "prefix": "192.168.4.0/24", "nextHop": on_link},
            {"kind": "address", "index": 15, "address": "192.168.4.2"},
        ]):
            self.assertEqual(windows.route("192.168.4.1"), {
                "index": 15, "routeIndex": 15, "address": "192.168.4.2",
                "prefix": "192.168.4.0/24", "nextHop": on_link,
            })


class WindowsAdapterTest(unittest.TestCase):
    def adapter(self) -> Any:
        adapter: Any = object.__new__(windows.WindowsWifi)
        adapter.dll = Mock()
        adapter.dll.WlanDisconnect.return_value = 0
        adapter.dll.WlanDeleteProfile.return_value = 0
        adapter.guid = windows.Guid()
        adapter.handle = None
        adapter.temporary = "test-only-temporary-profile"
        adapter.static_address = adapter.static_policy = None
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

    def test_interrupted_policy_check_still_removes_temporary_ipv4_and_restores_wlan(self):
        adapter = self.adapter()
        adapter.static_address = "192.168.4.210"
        adapter.luid, adapter.index = 150, 15
        adapter.ip = Mock()
        adapter.ip.addresses.return_value = adapter.ip.routes.return_value = []
        with (patch.object(windows, "ipv4_policy", side_effect=KeyboardInterrupt),
              self.assertRaisesRegex(RuntimeError, "IPv4 cleanup not verified")):
            adapter.restore()
        adapter.ip.delete.assert_called_once_with("192.168.4.210", 150, 15)
        adapter.dll.WlanCloseHandle.assert_called_once()
        adapter.set_radio.assert_called_once()

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


class SerialBootWitnessTest(unittest.TestCase):
    """Spec 01 §4: both installed generations boot at 0-0; only the server differs."""

    def witness(self, ready):
        serial = object.__new__(d.SerialWitness)
        serial.raw = bytearray(
            b"OTA build=b1 ABI=PKSCORE-OTA-ABI1 slot=ota_1 pending=1\n"
            b"OTA boot confirmed build=b1 slot=ota_1\n" + ready
        )
        return serial.confirmed("b1", "ota_1")

    def test_landscape_and_portrait_boot_positions_confirm(self):
        self.assertTrue(self.witness(b"SCOREBOARD ready state=0,0,L,1,0,D heap=1 undo_budget=6144\n"))
        self.assertTrue(self.witness(b"SCOREBOARD ready state=0,0,L,2,0,D heap=1 undo_budget=6144\n"))

    def test_non_boot_position_or_panic_is_not_confirmed(self):
        self.assertFalse(self.witness(b"SCOREBOARD ready state=0,0,R,1,0,D heap=1\n"))
        self.assertFalse(self.witness(b"SCOREBOARD ready state=0,0,L,1,0,D,1 heap=1\n"))
        self.assertFalse(self.witness(b"SCOREBOARD ready state=0,0,L,1,0,D heap=1\nBacktrace: 0x1\n"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
