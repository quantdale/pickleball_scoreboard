"""Real OpenSSL signing/container tests with disposable keys, NOT hardware OTA.

Usage: python scripts/test_ota_tool.py --output-dir <external-session>/tool-tests
Build ota and ota_b first. The output directory must not already exist.
"""
import argparse
import hashlib
import json
import shutil
import struct
import unittest
from pathlib import Path

import ota_tool as tool


class ToolTests(unittest.TestCase):
    root: Path
    openssl: str
    image: Path
    keys: Path
    alternate: Path
    package: Path

    @classmethod
    def setUpClass(cls):
        cls.keys = tool.provision(cls.openssl, cls.root/'TEST-ONLY-keys', 'PickleScore-TEST', test_only=True)
        cls.alternate = tool.provision(cls.openssl, cls.root/'TEST-ONLY-other', 'PickleScore-TEST-other', test_only=True)
        cls.image = tool.ROOT/'firmware/.pio/build/ota/firmware.bin'
        cls.package = cls.root/'TEST-ONLY-a.pksota'
        tool.pack(cls.openssl, cls.image, cls.keys/'signing-private.pem', 'ota-v1-a', cls.package)

    def verify_bytes(self, data):
        path = self.root/'mutated.pksota'
        path.write_bytes(data)
        return tool.verify(self.openssl, path, self.keys/'signing-public.pem', self.root/'scratch')

    def test_01_valid_actual_application_signature(self):
        result = tool.verify(self.openssl, self.package, self.keys/'signing-public.pem', self.root/'scratch')
        self.assertTrue(result['authenticated'])
        self.assertEqual(result['build'], 'ota-v1-a')

    def test_02_independent_credentials_and_trust_record(self):
        data = json.loads((self.keys/'owner-credentials.json').read_text())
        other = json.loads((self.alternate/'owner-credentials.json').read_text())
        record = (self.keys/'provisioning.bin').read_bytes()
        self.assertEqual(len(record), 164)
        self.assertEqual(record[1:34].rstrip(b'\0'), data['ssid'].encode())
        self.assertEqual(record[34:67].rstrip(b'\0'), data['apPassword'].encode())
        self.assertEqual(record[67:99], hashlib.sha256(data['maintenanceKey'].encode()).digest())
        self.assertEqual(record[99:], tool.public_point(self.openssl, self.keys/'signing-public.pem'))
        self.assertEqual(len(data['apPassword']), 32)
        self.assertEqual(len(data['maintenanceKey']), 64)
        self.assertNotEqual(data['apPassword'], other['apPassword'])
        self.assertNotEqual(data['maintenanceKey'], other['maintenanceKey'])
        self.assertTrue(data['testOnly'])

    def test_03_untrusted_key_rejected(self):
        with self.assertRaises(ValueError):
            tool.verify(self.openssl, self.package, self.alternate/'signing-public.pem', self.root/'scratch')

    def test_04_signature_manifest_mutation_rejected(self):
        data = self.package.read_bytes()
        for offset in [0, 8, 12, 16, 32, 64, 80, 98]:
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                changed = bytearray(data)
                changed[offset] ^= 1
                self.verify_bytes(changed)

    def test_05_payload_mutation_rejected(self):
        data = bytearray(self.package.read_bytes())
        data[-100] ^= 0x40
        with self.assertRaises(ValueError):
            self.verify_bytes(data)

    def test_06_truncated_extra_and_overflow_lengths_rejected(self):
        data = self.package.read_bytes()
        for changed in [data[:5], data[:169], data[:200], data[:-1], data+b'X']:
            with self.subTest(length=len(changed)), self.assertRaises(ValueError):
                self.verify_bytes(changed)
        for size in [0, 287, tool.SLOT_SIZE+1, 0xFFFFFFFF]:
            changed = bytearray(data)
            struct.pack_into('<I', changed, 12, size)
            with self.subTest(size=size), self.assertRaises(ValueError):
                self.verify_bytes(changed)

    def test_07_noncanonical_signature_and_build_padding_rejected(self):
        original = self.package.read_bytes()
        for size in [0, 7, 73, 0xFFFF]:
            data = bytearray(original)
            struct.pack_into('<H', data, 96, size)
            with self.assertRaises(ValueError):
                self.verify_bytes(data)
        data = bytearray(original)
        data[63] = ord('a')
        with self.assertRaises(ValueError):
            self.verify_bytes(data)

    def test_08_plain_baseline_and_wrong_build_cannot_be_packaged(self):
        baseline = tool.ROOT/'firmware/.pio/build/esp32dev/firmware.bin'
        for image, build in [(baseline, 'ota-v1-a'), (self.image, 'wrong-build')]:
            with self.assertRaises(ValueError):
                tool.pack(self.openssl, image, self.keys/'signing-private.pem', build, self.root/'must-not-exist.pksota')
        self.assertFalse((self.root/'must-not-exist.pksota').exists())

    def test_09_existing_key_and_package_are_never_overwritten(self):
        before = (self.keys/'signing-private.pem').read_bytes()
        with self.assertRaises(ValueError):
            tool.provision(self.openssl, self.keys, 'PickleScore-TEST')
        self.assertEqual(before, (self.keys/'signing-private.pem').read_bytes())
        with self.assertRaises(FileExistsError):
            tool.pack(self.openssl, self.image, self.keys/'signing-private.pem', 'ota-v1-a', self.package)

    def test_10_wrong_curve_cannot_be_provisioned(self):
        key = self.root/'TEST-ONLY-wrong-curve.pem'
        tool.new_file(key, tool.run_openssl(self.openssl, 'genpkey', '-algorithm', 'EC', '-pkeyopt', 'ec_paramgen_curve:P-384'), private=True)
        with self.assertRaises(ValueError):
            tool.public_point(self.openssl, key, private=True)

    def test_11_both_distinguishable_builds_validate(self):
        b = tool.ROOT/'firmware/.pio/build/ota_b/firmware.bin'
        target = self.root/'TEST-ONLY-b.pksota'
        tool.pack(self.openssl, b, self.keys/'signing-private.pem', 'ota-v1-b', target)
        result = tool.verify(self.openssl, target, self.keys/'signing-public.pem', self.root/'scratch')
        self.assertEqual(result['build'], 'ota-v1-b')
        self.assertNotEqual(self.image.read_bytes(), b.read_bytes())

    def test_12_esp_image_checksum_hash_and_structure(self):
        image = self.image.read_bytes()
        for offset in [0, 12, 23, 32, 80, -32, -1]:
            data = bytearray(image)
            data[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                tool.validate_image(data, 'ota-v1-a')
        for data in [image[:-1], image+b'X', image[:100]]:
            with self.assertRaises(ValueError):
                tool.validate_image(data)

    def test_13_partition_binary_exactly_fits_four_megabytes(self):
        data = (tool.ROOT/'firmware/.pio/build/ota/partitions.bin').read_bytes()
        entries = []
        for offset in range(0, len(data), 32):
            magic, typ, subtype, address, size, name, flags = struct.unpack_from('<HBBII16sI', data, offset)
            if magic != 0x50AA:
                break
            entries.append((name.rstrip(b'\0').decode(), typ, subtype, address, size))
        self.assertEqual(entries, [('nvs', 1, 2, 0x9000, 0x5000), ('otadata', 1, 0, 0xE000, 0x2000),
            ('ota_0', 0, 0x10, 0x10000, tool.SLOT_SIZE), ('ota_1', 0, 0x11, 0x200000, tool.SLOT_SIZE),
            ('coredump', 1, 3, 0x3F0000, 0x10000)])
        for before, after in zip(entries, entries[1:], strict=False):
            self.assertLessEqual(before[3]+before[4], after[3])
        self.assertEqual(entries[-1][3]+entries[-1][4], 0x400000)

    def test_14_serial_operation_requires_fresh_explicit_safety_flag(self):
        with self.assertRaises(ValueError):
            tool.provision_serial(self.keys/'provisioning.bin', 'NO-SERIAL-ACCESS', False)

    def test_15_secrets_and_evidence_output_refuses_repository(self):
        with self.assertRaises(ValueError):
            tool.outside(tool.ROOT/'private-key.pem')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--openssl', default=shutil.which('openssl') or 'openssl')
    args = parser.parse_args()
    ToolTests.root = tool.outside(args.output_dir)
    if ToolTests.root.exists():
        parser.error('Use a new session test directory; do not overwrite existing private keys')
    ToolTests.root.mkdir(parents=True)
    ToolTests.openssl = args.openssl
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ToolTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    (ToolTests.root/'result.json').write_text(json.dumps({'tests': result.testsRun,
        'failures': len(result.failures), 'errors': len(result.errors), 'passed': result.wasSuccessful(),
        'physicalHardwareTested': False, 'keysAreDisposableTestOnly': True}, indent=2)+'\n')
    raise SystemExit(0 if result.wasSuccessful() else 1)


if __name__ == '__main__':
    main()
