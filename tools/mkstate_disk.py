#!/usr/bin/env python3
"""Create the deterministic GPT development disk used by Obsidia.

The image contains one format-neutral block partition whose type identifies it
as Obsidia session-state storage.  statefs owns the contents of that partition;
the partition tool deliberately knows nothing about the filesystem format.
"""

import argparse
import struct
import zlib
from pathlib import Path

SECTOR_SIZE = 512
DEFAULT_MIB = 10
GPT_ENTRY_COUNT = 128
GPT_ENTRY_SIZE = 128
STATE_TYPE_GUID = bytes(
    (0x4F, 0x42, 0x53, 0x49, 0x44, 0x49, 0x41, 0x53,
     0x54, 0x41, 0x54, 0x45, 0x46, 0x53, 0x01, 0x00)
)
DISK_GUID = bytes.fromhex("6f6273696469612d6465766469736b01")
STATE_GUID = bytes.fromhex("6f6273696469612d7374617465667301")


def make_header(current_lba, backup_lba, entries_lba, first_usable,
                last_usable, disk_blocks, entries_crc):
    header = bytearray(SECTOR_SIZE)
    struct.pack_into(
        "<8sIIIIQQQQ16sQIII",
        header,
        0,
        b"EFI PART",
        0x00010000,
        92,
        0,
        0,
        current_lba,
        backup_lba,
        first_usable,
        last_usable,
        DISK_GUID,
        entries_lba,
        GPT_ENTRY_COUNT,
        GPT_ENTRY_SIZE,
        entries_crc,
    )
    struct.pack_into("<I", header, 16, zlib.crc32(header[:92]) & 0xFFFFFFFF)
    return header


def build_image(path, size_mib):
    disk_blocks = size_mib * 1024 * 1024 // SECTOR_SIZE
    entry_sectors = GPT_ENTRY_COUNT * GPT_ENTRY_SIZE // SECTOR_SIZE
    if disk_blocks < 4096:
        raise ValueError("disk must be at least 2 MiB")

    backup_header_lba = disk_blocks - 1
    backup_entries_lba = backup_header_lba - entry_sectors
    first_usable = 2 + entry_sectors
    last_usable = backup_entries_lba - 1
    state_first = 2048
    state_last = last_usable
    if state_first > state_last:
        raise ValueError("disk is too small for aligned state partition")

    protective_mbr = bytearray(SECTOR_SIZE)
    protective_mbr[446 + 4] = 0xEE
    struct.pack_into("<II", protective_mbr, 446 + 8, 1,
                     min(disk_blocks - 1, 0xFFFFFFFF))
    protective_mbr[510:512] = b"\x55\xaa"

    entries = bytearray(entry_sectors * SECTOR_SIZE)
    entries[0:16] = STATE_TYPE_GUID
    entries[16:32] = STATE_GUID
    struct.pack_into("<QQQ", entries, 32, state_first, state_last, 0)
    label = "Obsidia State".encode("utf-16-le")
    entries[56:56 + len(label)] = label
    entries_crc = zlib.crc32(entries) & 0xFFFFFFFF

    primary = make_header(1, backup_header_lba, 2, first_usable,
                          last_usable, disk_blocks, entries_crc)
    backup = make_header(backup_header_lba, 1, backup_entries_lba,
                         first_usable, last_usable, disk_blocks, entries_crc)

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as image:
        image.truncate(disk_blocks * SECTOR_SIZE)
        image.seek(0)
        image.write(protective_mbr)
        image.seek(SECTOR_SIZE)
        image.write(primary)
        image.seek(2 * SECTOR_SIZE)
        image.write(entries)
        image.seek(backup_entries_lba * SECTOR_SIZE)
        image.write(entries)
        image.seek(backup_header_lba * SECTOR_SIZE)
        image.write(backup)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--size-mib", type=int, default=DEFAULT_MIB)
    args = parser.parse_args()
    build_image(args.output, args.size_mib)


if __name__ == "__main__":
    main()
