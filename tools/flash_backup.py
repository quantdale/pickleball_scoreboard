"""Read-only whole-flash backup over the lossy Court 1 CH340, and the exact image to expect
after an app-only USB write. Never writes flash.

Spec 04 §8 Mode B only: every esptool call resets the chip into its bootloader, so it needs
fresh USB-only isolation. The COM11 CH340 drops bytes above 115,200 baud and sometimes at it,
so read 256 KiB chunks with retries, split a chunk that keeps failing into 64 KiB pieces,
and accept the assembled file only when the chip's own verify_flash digest matches.
"""

import argparse
import hashlib
import subprocess
import sys
from pathlib import Path

FLASH_BYTES = 0x400000
CHUNK = 0x40000
PIECE = 0x10000
APP_OFFSET = 0x10000
SLOT_BYTES = 0x1F0000
SECTOR = 0x1000
ESPTOOL = Path.home() / ".platformio/packages/tool-esptoolpy/esptool.py"


class BackupError(RuntimeError):
    pass


def esptool_runner(port, baud, esptool=ESPTOOL):
    def run(*args):
        command = [sys.executable, str(esptool), "--chip", "esp32", "--port", port, "--baud", str(baud), *args]
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        return result.returncode, (result.stdout + result.stderr).replace("\r", "\n")
    return run


def read_region(run, offset, size, path, attempts, log):
    for attempt in range(1, attempts + 1):
        code, output = run("read_flash", hex(offset), hex(size), str(path))
        if code == 0 and path.exists() and path.stat().st_size == size:
            log.append({"offset": offset, "bytes": size, "attempts": attempt})
            return path.read_bytes()
        reason = next((line for line in reversed(output.splitlines()) if "error" in line.lower()), f"exit {code}")
        log.append({"offset": offset, "bytes": size, "attempt": attempt, "failed": reason.strip()[:160]})
    return None


def verify(run, image_path):
    code, output = run("verify_flash", "--flash_mode", "keep", "--flash_freq", "keep",
                       "--flash_size", "keep", "0", str(image_path))
    return code == 0 and "digest matched" in output


def backup(run, workdir, out, attempts=4):
    """Assemble a whole-flash image and require the chip's digest to match it."""
    workdir.mkdir(parents=True, exist_ok=True)
    log, image = [], bytearray()
    for offset in range(0, FLASH_BYTES, CHUNK):
        data = read_region(run, offset, CHUNK, workdir / f"{offset:06x}.bin", attempts, log)
        if data is None:  # a stubborn chunk: smaller reads lose less to one dropped byte
            data = b""
            for piece in range(offset, offset + CHUNK, PIECE):
                part = read_region(run, piece, PIECE, workdir / f"{piece:06x}-piece.bin", attempts + 2, log)
                if part is None:
                    raise BackupError(f"Flash at {piece:#x} unreadable; no backup written")
                data += part
        image += data
    out.write_bytes(image)
    if not verify(run, out):
        out.unlink()
        raise BackupError("Assembled backup does not match the chip's verify_flash digest")
    return {"bytes": len(image), "sha256": hashlib.sha256(image).hexdigest(), "reads": log}


def expected_after(flash, image, offset=APP_OFFSET):
    """esptool erases whole sectors over the image, so the last sector's tail reads 0xFF."""
    if len(flash) != FLASH_BYTES or offset % SECTOR or not 0 < len(image) <= SLOT_BYTES:
        raise BackupError("Backup size, write offset or image size outside the app-slot write")
    end = offset + (len(image) + SECTOR - 1) // SECTOR * SECTOR
    result = bytearray(flash)
    result[offset:end] = image + b"\xff" * (end - offset - len(image))
    return bytes(result)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("backup", "verify"):
        command = commands.add_parser(name)
        command.add_argument("--port", required=True)
        command.add_argument("--baud", type=int, default=115200)
        command.add_argument("--confirm-isolated-usb-only", action="store_true")
        command.add_argument("--image" if name == "verify" else "--output", type=Path, required=True)
    expected = commands.add_parser("expected")
    expected.add_argument("--backup", type=Path, required=True)
    expected.add_argument("--image", type=Path, required=True)
    expected.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    if args.command == "expected":
        data = expected_after(args.backup.read_bytes(), args.image.read_bytes())
        args.output.write_bytes(data)
        print(f"expected sha256={hashlib.sha256(data).hexdigest()}")
        return 0
    if not args.confirm_isolated_usb_only:
        parser.error("Each esptool call resets the chip: confirm fresh Mode B isolation")
    if args.baud > 115200:
        parser.error("The Court 1 CH340 drops bytes above 115,200 baud")
    run = esptool_runner(args.port, args.baud)
    if args.command == "verify":
        matched = verify(run, args.image)
        print("digest matched" if matched else "DIGEST MISMATCH")
        return 0 if matched else 1
    if args.output.exists():
        parser.error("Refusing to overwrite an existing backup")
    try:
        report = backup(run, args.output.with_suffix(".chunks"), args.output)
    except BackupError as error:
        print(f"ABORT: {error}")
        return 1
    print(f"backup {report['bytes']} B sha256={report['sha256']} verified on chip")
    return 0


if __name__ == "__main__":
    sys.exit(main())
