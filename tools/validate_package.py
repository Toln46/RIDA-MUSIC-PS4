#!/usr/bin/env python3
"""Audit OpenOrbis fake-SELF structure and PS4 param.sfo metadata."""
import argparse
import hashlib
import struct
import sys
from pathlib import Path

EXPECTED = {
    "APP_TYPE": 1,
    "APP_VER": "0.03",
    "ATTRIBUTE": 0,
    "CATEGORY": "gd",
    "CONTENT_ID": "IV0000-RIDA00001_00-RIDAMUSIC0000000",
    "DOWNLOAD_DATA_SIZE": 0,
    "SYSTEM_VER": 0,
    "TITLE": "RGS MUSIC",
    "TITLE_ID": "RIDA00001",
    "VERSION": "0.03",
}
EXPECTED_PAID = 0x3800000000000011
EXPECTED_AUTHINFO = bytes.fromhex(
    "000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
)
SELF_MAGIC = 0x1D3D154F
SELF_HEADER_SIZE = 0x20
SELF_ENTRY_SIZE = 0x50
SELF_EXTENDED_HEADER_SIZE = 0x40
SELF_NPDRM_BLOCK_SIZE = 0x30
SELF_META_FOOTER_SIZE = 0x50
SELF_SIGNATURE_SIZE = 0x100


def parse_sfo(path):
    data = Path(path).read_bytes()
    if len(data) < 20 or data[:4] != b"\x00PSF":
        raise ValueError(f"{path}: invalid PSF/param.sfo magic or truncated header")
    _, key_off, data_off, count = struct.unpack_from("<IIII", data, 4)
    if count > 4096 or 20 + count * 16 > len(data):
        raise ValueError(f"{path}: invalid SFO index table")
    result = {}
    for i in range(count):
        entry = 20 + i * 16
        key_rel, fmt, length, _, value_rel = struct.unpack_from("<HHIII", data, entry)
        key_pos = key_off + key_rel
        value_pos = data_off + value_rel
        if key_pos >= len(data) or value_pos + length > len(data):
            raise ValueError(f"{path}: SFO entry {i} points outside file")
        key_end = data.find(b"\x00", key_pos)
        if key_end < 0:
            raise ValueError(f"{path}: unterminated SFO key")
        key = data[key_pos:key_end].decode("utf-8")
        raw = data[value_pos:value_pos + length]
        if fmt == 0x0404:
            if length != 4:
                raise ValueError(f"{path}: {key} has invalid integer length {length}")
            value = struct.unpack("<I", raw)[0]
        elif fmt == 0x0204:
            value = raw.rstrip(b"\x00").decode("utf-8")
        else:
            raise ValueError(f"{path}: {key} has unexpected SFO format 0x{fmt:04x}")
        result[key] = value
    return result


def audit_sfo(path, label):
    entries = parse_sfo(path)
    errors = []
    for key, expected in EXPECTED.items():
        actual = entries.get(key, object())
        if actual != expected:
            errors.append(f"{label}: {key}: expected {expected!r}, got {actual!r}")
    if errors:
        raise ValueError("\n".join(errors))
    print(f"[OK] {label}: all {len(EXPECTED)} required SFO fields and values match")
    print(f"     TITLE={entries['TITLE']} | TITLE_ID={entries['TITLE_ID']} | CONTENT_ID={entries['CONTENT_ID']}")


def audit_self(path):
    data = Path(path).read_bytes()
    if len(data) < SELF_HEADER_SIZE:
        raise ValueError("eboot.bin is too small to contain a SELF header")

    # OpenOrbis SelfHeader is 0x1c bytes followed by 4-byte alignment padding.
    fields = struct.unpack_from("<IBBBB IHHQHH", data, 0)
    magic, version, mode, endian, attributes, key_type, header_size, meta_size, file_size, count, flags = fields
    if magic != SELF_MAGIC:
        raise ValueError(f"eboot.bin SELF magic mismatch: 0x{magic:08x}")
    if endian != 1:
        raise ValueError(f"unexpected SELF byte order: {endian}")
    if count == 0 or count % 2:
        raise ValueError(f"unexpected SELF entry count: {count}")

    expected_meta = count * SELF_ENTRY_SIZE + SELF_META_FOOTER_SIZE + SELF_SIGNATURE_SIZE
    if meta_size != expected_meta:
        raise ValueError(f"SELF metadata size mismatch: header={meta_size}, expected={expected_meta}")
    if header_size < SELF_HEADER_SIZE + SELF_EXTENDED_HEADER_SIZE + SELF_NPDRM_BLOCK_SIZE:
        raise ValueError(f"SELF header size is too small: 0x{header_size:x}")
    if header_size + meta_size > len(data):
        raise ValueError("SELF header/meta area points outside eboot.bin")
    if file_size < len(data) or file_size - len(data) >= 0x10:
        raise ValueError(f"SELF file size/alignment mismatch: header={file_size}, actual={len(data)}")

    # OpenOrbis create-fself writes the extended info immediately before its
    # 0x30-byte NPDRM control block, both included at the end of HeaderSize.
    extended_offset = header_size - SELF_EXTENDED_HEADER_SIZE - SELF_NPDRM_BLOCK_SIZE
    paid, program_type, app_version, fw_version = struct.unpack_from("<QQQQ", data, extended_offset)
    if paid != EXPECTED_PAID:
        raise ValueError(f"SELF PAID mismatch at 0x{extended_offset:x}: 0x{paid:016x}")
    if program_type != 1:
        raise ValueError(f"SELF program type is not fake: {program_type}")

    npdrm_offset = extended_offset + SELF_EXTENDED_HEADER_SIZE
    control_type = struct.unpack_from("<H", data, npdrm_offset)[0]
    if control_type != 3:
        raise ValueError(f"SELF NPDRM control block type mismatch: {control_type}")

    # create-fself encodes signature data as: authinfo byte length, PAID,
    # authinfo bytes after the first eight bytes, then zero padding to 0x100.
    signature_offset = header_size + count * SELF_ENTRY_SIZE + SELF_META_FOOTER_SIZE
    signature = data[signature_offset:signature_offset + SELF_SIGNATURE_SIZE]
    if len(signature) != SELF_SIGNATURE_SIZE:
        raise ValueError("SELF signature/authinfo block is truncated")
    auth_len, signature_paid = struct.unpack_from("<QQ", signature, 0)
    if auth_len == 0:
        # With no -authinfo argument, this create-fself build leaves the
        # entire 0x100-byte signature/authinfo area zeroed.
        if any(signature):
            raise ValueError("SELF AUTHINFO block should be entirely zero when -authinfo is omitted")
        auth_mode = "empty (all-zero block)"
    else:
        if auth_len != len(EXPECTED_AUTHINFO):
            raise ValueError(f"SELF AUTHINFO length mismatch: {auth_len}")
        if signature_paid != EXPECTED_PAID:
            raise ValueError(f"SELF AUTHINFO PAID mismatch: 0x{signature_paid:016x}")
        expected_signature = struct.pack("<QQ", auth_len, EXPECTED_PAID) + EXPECTED_AUTHINFO[8:]
        expected_signature = expected_signature.ljust(SELF_SIGNATURE_SIZE, b"\\x00")
        if signature != expected_signature:
            raise ValueError("SELF AUTHINFO/signature payload does not match the configured OpenOrbis AUTHINFO")
        auth_mode = f"configured (length {auth_len})"

    print("[OK] eboot.bin: SELF magic, header sizes, entry table and file length are coherent")
    print(f"[OK] eboot.bin: PAID=0x{paid:016x}, program type=fake, NPDRM block type={control_type}")
    print(f"[OK] eboot.bin: AUTHINFO {auth_mode}; 256-byte signature block is coherent")
    print(f"     SELF SHA-256: {hashlib.sha256(data).hexdigest()}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--eboot", required=True)
    parser.add_argument("--sfo", required=True)
    parser.add_argument("--packaged-sfo", required=True)
    args = parser.parse_args()
    try:
        audit_self(args.eboot)
        audit_sfo(args.sfo, "source param.sfo")
        audit_sfo(args.packaged_sfo, "param.sfo extracted from final PKG")
        # PKG metadata extraction may normalize/re-serialize the SFO container.
        # Validate the actual required fields and values in both files above;
        # byte-for-byte identity is not a reliable package correctness test.
        print("[OK] PKG metadata: all required PARAM.SFO fields and values match")
    except Exception as exc:
        print(f"[AUDIT FAILED] {exc}", file=sys.stderr)
        return 1
    print("[AUDIT PASSED] FSELF + source SFO + packaged SFO")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
