#!/usr/bin/env python3
"""Pack x86_64 ELF PT_LOAD segments into the independent OBSX v1 format."""

import argparse
import struct
from pathlib import Path

ELF_HEADER = struct.Struct("<16sHHIQQQIHHHHHH")
ELF_PROGRAM = struct.Struct("<IIQQQQQQ")
OBS_HEADER = struct.Struct("<8sHHIIIIQQQQQ")
OBS_SEGMENT = struct.Struct("<QQQQIIQ")
OBS_MAGIC = b"OBSX\r\n\x1a\n"


def align(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def checked_slice(data: bytes, offset: int, size: int) -> bytes:
    if offset < 0 or size < 0 or offset > len(data) or size > len(data) - offset:
        raise ValueError("ELF table or segment lies outside the input file")
    return data[offset : offset + size]


def pack(source: Path, destination: Path) -> None:
    elf = source.read_bytes()
    if len(elf) < ELF_HEADER.size:
        raise ValueError("input is shorter than an ELF64 header")
    fields = ELF_HEADER.unpack_from(elf)
    ident, elf_type, machine, version = fields[:4]
    entry, phoff, _, _, _, phentsize, phnum = fields[4:11]
    if ident[:6] != b"\x7fELF\x02\x01" or elf_type not in (2, 3) or machine != 62 or version != 1:
        raise ValueError("input must be a little-endian x86_64 executable ELF")
    if phentsize != ELF_PROGRAM.size or not phnum or phnum > 64:
        raise ValueError("unsupported ELF program table")
    checked_slice(elf, phoff, phnum * phentsize)

    loads = []
    for index in range(phnum):
        ph = ELF_PROGRAM.unpack_from(elf, phoff + index * phentsize)
        kind, elf_flags, file_offset, virtual_address, _, file_size, memory_size, alignment = ph
        if kind != 1:
            continue
        checked_slice(elf, file_offset, file_size)
        if not memory_size:
            if file_size:
                raise ValueError("invalid empty PT_LOAD")
            continue
        if file_size > memory_size:
            raise ValueError("invalid PT_LOAD sizes")
        flags = (1 if elf_flags & 4 else 0) | (2 if elf_flags & 2 else 0) | (4 if elf_flags & 1 else 0)
        if flags & 2 and flags & 4:
            raise ValueError("OBSX v1 refuses writable+executable segments")
        loads.append((file_offset, virtual_address, file_size, memory_size, flags, alignment or 1))
    if not loads:
        raise ValueError("ELF has no loadable segments")

    table_offset = OBS_HEADER.size
    payload_offset = align(table_offset + len(loads) * OBS_SEGMENT.size, 16)
    output = bytearray(payload_offset)
    records = []
    for file_offset, virtual_address, file_size, memory_size, flags, alignment in loads:
        destination_offset = len(output)
        output.extend(checked_slice(elf, file_offset, file_size))
        while len(output) & 15:
            output.append(0)
        records.append((destination_offset, virtual_address, file_size, memory_size,
                        flags, min(alignment, 0x200000), 0))
    OBS_HEADER.pack_into(output, 0, OBS_MAGIC, 1, OBS_HEADER.size, 0x8664, 0,
                         len(records), OBS_SEGMENT.size, entry, table_offset, 0, 0, 0)
    for index, record in enumerate(records):
        OBS_SEGMENT.pack_into(output, table_offset + index * OBS_SEGMENT.size, *record)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    pack(args.input, args.output)


if __name__ == "__main__":
    main()
