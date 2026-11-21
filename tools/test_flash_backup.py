"""Host tests for the read-only flash backup; a fake chip stands in for esptool and COM11."""

import hashlib
import tempfile
import unittest
from pathlib import Path

import flash_backup as fb


class FakeChip:
    """read_flash writes the region, or a short file for scheduled lossy reads."""

    def __init__(self, drops=(), digest_ok=True):
        self.flash = bytes(i * 7 % 251 for i in range(fb.FLASH_BYTES))
        self.drops = dict(drops)  # (offset, size) -> number of lossy reads before success
        self.digest_ok = digest_ok
        self.calls = []

    def __call__(self, command, *args):
        self.calls.append((command, *args))
        if command == "read_flash":
            offset, size, path = int(args[0], 16), int(args[1], 16), Path(args[2])
            if self.drops.get((offset, size), 0):
                self.drops[(offset, size)] -= 1
                path.write_bytes(self.flash[offset:offset + size - 2])
                return 2, "A fatal error occurred: Corrupt data, expected 0x1000 bytes but received 0xffe bytes"
            path.write_bytes(self.flash[offset:offset + size])
            return 0, "Read done"
        if command == "verify_flash":
            same = Path(args[-1]).read_bytes() == self.flash and self.digest_ok
            return (0, "-- verify OK (digest matched)") if same else (2, "verify FAILED (digest mismatch)")
        raise AssertionError(f"unexpected esptool command {command}")


class BackupTest(unittest.TestCase):
    def run_backup(self, chip, attempts=4):
        directory = Path(tempfile.mkdtemp())
        out = directory / "court1.bin"
        return fb.backup(chip, directory / "chunks", out, attempts), out

    def test_lossy_chunk_is_retried_and_backup_matches_chip_digest(self):
        chip = FakeChip(drops={(fb.CHUNK, fb.CHUNK): 3})
        report, out = self.run_backup(chip)
        self.assertEqual(out.read_bytes(), chip.flash)
        self.assertEqual(report["sha256"], hashlib.sha256(chip.flash).hexdigest())
        self.assertEqual(chip.calls[-1][0], "verify_flash")
        self.assertTrue(all(call[0] in ("read_flash", "verify_flash") for call in chip.calls))

    def test_stubborn_chunk_falls_back_to_pieces(self):
        chip = FakeChip(drops={(0, fb.CHUNK): 4, (fb.PIECE, fb.PIECE): 1})
        report, out = self.run_backup(chip)
        self.assertEqual(out.read_bytes(), chip.flash)
        self.assertIn({"offset": fb.PIECE, "bytes": fb.PIECE, "attempts": 2}, report["reads"])

    def test_unreadable_piece_aborts_without_a_backup(self):
        chip = FakeChip(drops={(0, fb.CHUNK): 4, (2 * fb.PIECE, fb.PIECE): 6})
        with self.assertRaisesRegex(fb.BackupError, "0x20000 unreadable"):
            self.run_backup(chip)
        self.assertFalse(any(call[0] == "verify_flash" for call in chip.calls))

    def test_digest_mismatch_discards_the_backup(self):
        chip = FakeChip(digest_ok=False)
        directory = Path(tempfile.mkdtemp())
        out = directory / "court1.bin"
        with self.assertRaisesRegex(fb.BackupError, "verify_flash digest"):
            fb.backup(chip, directory / "chunks", out)
        self.assertFalse(out.exists())


class ExpectedImageTest(unittest.TestCase):
    def test_only_the_erased_app_sectors_change(self):
        flash = bytes(range(256)) * (fb.FLASH_BYTES // 256)
        image = b"\xe9" + b"\x11" * (fb.SECTOR + 9)
        result = fb.expected_after(flash, image)
        end = fb.APP_OFFSET + 2 * fb.SECTOR
        self.assertEqual(result[:fb.APP_OFFSET], flash[:fb.APP_OFFSET])
        self.assertEqual(result[fb.APP_OFFSET:fb.APP_OFFSET + len(image)], image)
        self.assertEqual(set(result[fb.APP_OFFSET + len(image):end]), {0xFF})
        self.assertEqual(result[end:], flash[end:])

    def test_rejects_images_outside_the_app_slot(self):
        flash = bytes(fb.FLASH_BYTES)
        for image, offset in ((b"", fb.APP_OFFSET), (b"\0" * (fb.SLOT_BYTES + 1), fb.APP_OFFSET),
                              (b"\xe9", fb.APP_OFFSET + 1)):
            with self.subTest(size=len(image), offset=offset), self.assertRaises(fb.BackupError):
                fb.expected_after(flash, image, offset)
        with self.assertRaises(fb.BackupError):
            fb.expected_after(flash[:-1], b"\xe9")


class CommandLineTest(unittest.TestCase):
    def test_chip_access_needs_isolation_and_safe_baud(self):
        for argv in (["backup", "--port", "COM11", "--output", "x.bin"],
                     ["verify", "--port", "COM11", "--image", "x.bin"],
                     ["backup", "--port", "COM11", "--output", "x.bin", "--confirm-isolated-usb-only",
                      "--baud", "230400"]):
            with self.subTest(argv=argv), self.assertRaises(SystemExit):
                fb.main(argv)


if __name__ == "__main__":
    unittest.main()
