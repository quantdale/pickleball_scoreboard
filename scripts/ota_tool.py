"""Offline PickleScore provisioning and authenticated firmware packaging (Spec 04).

Requires OpenSSL on PATH (or --openssl). Never opens serial or flashes implicitly.
Private output must live outside the repository. No real credentials are generated
by tests; tests create disposable keys labelled TEST ONLY in a session directory.
"""
import argparse
import hashlib
import hmac
import json
import os
import re
import secrets
import shutil
import struct
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAGIC = b'PKSOTA01'
TARGET = b'PKSCORE-ESP32-V1'
MARKER = b'PKSCORE-OTA-ABI1\0'
SLOT_SIZE = 0x1F0000
HEADER_SIZE = 170
SIGNED = struct.Struct('<8sII16s32s32s')
# Exact P-256 SubjectPublicKeyInfo prefix; do not accidentally provision another curve.
P256_SPKI = bytes.fromhex('3059301306072a8648ce3d020106082a8648ce3d030107034200')


def run_openssl(executable, *args, data=None):
    result = subprocess.run([str(executable), *map(str, args)], input=data, capture_output=True, check=False)
    if result.returncode:
        raise ValueError('OpenSSL operation failed: ' + result.stderr.decode(errors='replace').strip())
    return result.stdout


def outside(path):
    path = Path(path).resolve()
    if path.is_relative_to(ROOT):
        raise ValueError('Private/evidence output must be outside the repository')
    return path


def new_file(path, data, private=False):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    # Atomic exclusive creation, never truncate another key/package.
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600 if private else 0o644)
    with os.fdopen(descriptor, 'wb') as output:
        output.write(data)


def public_point(executable, key, private=False):
    args = ['pkey'] + ([] if private else ['-pubin']) + ['-in', str(key), '-pubout', '-outform', 'DER']
    der = run_openssl(executable, *args)
    if len(der) != 91 or not der.startswith(P256_SPKI) or der[26] != 4:
        raise ValueError('A SEC1 uncompressed NIST P-256 public key is required')
    return der[26:]


def validate_image(image, build=None):
    """Independent ESP32 image structure/checksum/hash checks, not a bootloader emulator."""
    if len(image) < 288 or len(image) > SLOT_SIZE:
        raise ValueError('Application outside 288..2031616 byte limit')
    if image[0] != 0xE9 or not 1 <= image[1] <= 16 or image[12:14] != b'\0\0' or image[23] != 1:
        raise ValueError('Not a SHA256-appended ESP32 application image')
    if image[32:36] != struct.pack('<I', 0xABCD5432):
        raise ValueError('ESP application descriptor missing (not an application image)')
    offset, checksum = 24, 0xEF
    for _ in range(image[1]):
        if offset + 8 > len(image):
            raise ValueError('Truncated segment header')
        _, size = struct.unpack_from('<II', image, offset)
        offset += 8
        if size % 4 or offset + size > len(image):
            raise ValueError('Truncated or unaligned application segment')
        for value in image[offset:offset+size]:
            checksum ^= value
        offset += size
    checksum_offset = (offset // 16) * 16 + 15
    if checksum_offset + 33 != len(image):
        raise ValueError('Truncated image or unexpected trailing/merged-flash bytes')
    if any(image[offset:checksum_offset]) or image[checksum_offset] != checksum:
        raise ValueError('ESP image checksum/padding mismatch')
    if not hmac.compare_digest(hashlib.sha256(image[:checksum_offset+1]).digest(), image[-32:]):
        raise ValueError('ESP image appended SHA256 mismatch')
    if MARKER not in image:
        raise ValueError('OTA compatibility marker missing; baseline is USB recovery only')
    if build is not None and b'PKSCORE-BUILD:' + build.encode('ascii') + b'\0' not in image:
        raise ValueError('Declared build does not match the compiled firmware marker')


def padded_build(build):
    if not re.fullmatch(r'[A-Za-z0-9_.-]{1,31}', build):
        raise ValueError('Build identifier must be 1..31 ASCII letters/digits/underscore/dash/dot')
    return build.encode('ascii').ljust(32, b'\0')


def provision(executable, directory, ssid, test_only=False):
    directory = outside(directory)
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,32}', ssid):
        raise ValueError('SSID must be 1..32 letters/digits/underscore/dash')
    if directory.exists():
        raise ValueError('Provisioning directory must not already exist; refusing key replacement')
    directory.mkdir(parents=True, mode=0o700)
    private = run_openssl(executable, 'genpkey', '-algorithm', 'EC', '-pkeyopt', 'ec_paramgen_curve:P-256')
    new_file(directory/'signing-private.pem', private, private=True)
    public = run_openssl(executable, 'pkey', '-pubout', data=private)
    new_file(directory/'signing-public.pem', public)
    point = public_point(executable, directory/'signing-public.pem')
    ap_password, admin = secrets.token_urlsafe(24), secrets.token_hex(32)
    assert len(ap_password) == 32 and len(admin) == 64
    record = (b'\1' + ssid.encode().ljust(33, b'\0') + ap_password.encode().ljust(33, b'\0')
              + hashlib.sha256(admin.encode()).digest() + point)
    assert len(record) == 164
    new_file(directory/'provisioning.bin', record, private=True)
    new_file(directory/'owner-credentials.json', (json.dumps({
        'schema': 1, 'ssid': ssid, 'apPassword': ap_password, 'maintenanceKey': admin,
        'publicKeySha256': hashlib.sha256(point).hexdigest(), 'testOnly': test_only,
        'warning': 'Keep this file and signing-private.pem private, outside Git. Back up before sealing enclosure.'
    }, indent=2)+'\n').encode(), private=True)
    return directory


def pack(executable, image_path, key, build, output):
    output = outside(output)
    image = Path(image_path).read_bytes()
    build_bytes = padded_build(build)
    validate_image(image, build)
    public_point(executable, key, private=True)
    manifest = SIGNED.pack(MAGIC, 1, len(image), TARGET, build_bytes, hashlib.sha256(image).digest())
    signature = run_openssl(executable, 'dgst', '-sha256', '-sign', key, data=manifest)
    if not 8 <= len(signature) <= 72:
        raise ValueError('Unexpected P-256 DER signature length')
    package = manifest + struct.pack('<H', len(signature)) + signature.ljust(72, b'\0') + image
    new_file(output, package)
    return {'build': build, 'imageBytes': len(image), 'slotBytes': SLOT_SIZE,
            'packageBytes': len(package), 'packageSha256': hashlib.sha256(package).hexdigest()}


def verify(executable, package_path, public_key, work_dir):
    with Path(package_path).open('rb') as source:
        package = source.read(HEADER_SIZE+SLOT_SIZE+1)
    return verify_bytes(executable, package, public_key, work_dir)


def verify_bytes(executable, package, public_key, work_dir):
    """Authenticate the immutable bytes the deployment client will actually send (Spec 04 §8)."""
    if not HEADER_SIZE+288 <= len(package) <= HEADER_SIZE+SLOT_SIZE:
        raise ValueError('Package size outside bounds')
    magic, version, size, target, encoded_build, digest = SIGNED.unpack(package[:SIGNED.size])
    if magic != MAGIC or version != 1 or target != TARGET or size != len(package)-HEADER_SIZE:
        raise ValueError('Package metadata/length/target mismatch')
    try:
        build = encoded_build.split(b'\0', 1)[0].decode('ascii')
    except UnicodeError as exc:
        raise ValueError('Invalid ASCII build') from exc
    if padded_build(build) != encoded_build:
        raise ValueError('Noncanonical build padding')
    signature_length = struct.unpack_from('<H', package, SIGNED.size)[0]
    if not 8 <= signature_length <= 72 or any(package[98+signature_length:HEADER_SIZE]):
        raise ValueError('Noncanonical signature length/padding')
    public_point(executable, public_key)
    work_dir = outside(work_dir)
    work_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='signature-', dir=work_dir) as temp:
        signature = Path(temp)/'signature.der'
        signature.write_bytes(package[98:98+signature_length])
        run_openssl(executable, 'dgst', '-sha256', '-verify', public_key, '-signature', signature, data=package[:SIGNED.size])
    image = package[HEADER_SIZE:]
    if not hmac.compare_digest(hashlib.sha256(image).digest(), digest):
        raise ValueError('Image differs from signed SHA256')
    validate_image(image, build)
    return {'build': build, 'imageBytes': len(image), 'authenticated': True,
            'packageSha256': hashlib.sha256(package).hexdigest(), 'imageSha256': digest.hex()}


def provision_serial(record_path, port_name, confirmed):
    if not confirmed:
        raise ValueError('Serial reset/write requires --confirm-isolated-usb-only after fresh owner confirmation')
    import serial  # existing PlatformIO venv dependency, only needed for explicit physical provisioning
    record = Path(record_path).read_bytes()
    if len(record) != 164 or record[0] != 1:
        raise ValueError('Invalid provisioning record')
    # Do not print record/credentials; capture only fixed firmware acknowledgement.
    port = serial.Serial(port=None, baudrate=115200, timeout=0.2)
    port.port = port_name
    port.dtr = False
    port.rts = False
    try:
        port.open()
        port.reset_input_buffer()
        port.rts = True
        time.sleep(0.12)
        port.rts = False
        pending, deadline = bytearray(), time.monotonic()+15
        while time.monotonic() < deadline:
            pending.extend(port.read(1024))
            if b'SCOREBOARD ready' in pending:
                break
        else:
            raise ValueError('Scoreboard startup not observed; provisioning not sent')
        port.write(b'PSPROV1 ' + record.hex().encode('ascii') + b'\n')
        port.flush()
        pending.clear()
        deadline = time.monotonic()+10
        while time.monotonic() < deadline:
            pending.extend(port.read(1024))
            if b'OTA PROVISIONED;' in pending:
                return
            if b'provisioning refused' in pending or b'provisioning rejected' in pending or b'provisioning storage failed' in pending:
                raise ValueError('Device rejected provisioning; check startup window/no BLE activity')
        raise ValueError('No provisioning acknowledgement; do not assume credentials were saved')
    finally:
        port.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--openssl', default=shutil.which('openssl') or 'openssl')
    commands = parser.add_subparsers(dest='command', required=True)
    gen = commands.add_parser('provision-files')
    gen.add_argument('--directory', type=Path, required=True)
    gen.add_argument('--ssid', default='PickleScore-Court1')
    sign = commands.add_parser('pack')
    sign.add_argument('--image', type=Path, required=True)
    sign.add_argument('--key', type=Path, required=True)
    sign.add_argument('--build', required=True)
    sign.add_argument('--output', type=Path, required=True)
    check = commands.add_parser('verify')
    check.add_argument('--package', type=Path, required=True)
    check.add_argument('--public-key', type=Path, required=True)
    check.add_argument('--work-dir', type=Path, required=True)
    physical = commands.add_parser('provision-serial')
    physical.add_argument('--record', type=Path, required=True)
    physical.add_argument('--port', required=True)
    physical.add_argument('--confirm-isolated-usb-only', action='store_true')
    args = parser.parse_args()
    try:
        if args.command == 'provision-files':
            directory = provision(args.openssl, args.directory, args.ssid)
            print('Private credential/key files created outside Git:', directory)
            print('Back up and restrict access to this directory before sealing. No serial access was performed.')
        elif args.command == 'pack':
            print(json.dumps(pack(args.openssl, args.image, args.key, args.build, args.output), indent=2))
        elif args.command == 'verify':
            print(json.dumps(verify(args.openssl, args.package, args.public_key, args.work_dir), indent=2))
        elif args.command == 'provision-serial':
            provision_serial(args.record, args.port, args.confirm_isolated_usb_only)
            print('Provisioning acknowledged. Port closed. Reboot manually to open maintenance AP.')
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        parser.exit(1, f'OTA tool failed: {exc}\n')


if __name__ == '__main__':
    main()
