#!/usr/bin/env python3
"""Create deterministic, genuine PE/COFF fixtures without a Windows SDK."""

import argparse
import struct
from pathlib import Path

IMAGE_BASE = 0x140000000
SECTION_RVA = 0x1000
FILE_ALIGNMENT = 0x200
SECTION_ALIGNMENT = 0x1000


def align(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def make_pe(imported: bool) -> bytes:
    message = b"winabi: import-free PE32+ entry ran\n"
    code = bytearray(b"\xb8\x06\x00\x00\x00\xbf\x01\x00\x00\x00\x48\xbe")
    message_offset = 35
    code.extend(struct.pack("<Q", IMAGE_BASE + SECTION_RVA + message_offset))
    code.extend(b"\xba" + struct.pack("<I", len(message)) + b"\xcd\x80")
    code.extend(b"\x31\xff\x31\xc0\xcd\x80\x0f\x0b")
    assert len(code) == message_offset
    code.extend(message)

    import_rva = import_size = 0
    if imported:
        while len(code) < 0x80:
            code.append(0)
        descriptor_offset = len(code)
        name_offset = descriptor_offset + 20
        name_rva = SECTION_RVA + name_offset
        code.extend(struct.pack("<IIIII", 0, 0, 0, name_rva, 0))
        code.extend(b"KERNEL32.dll\0")
        import_rva = SECTION_RVA + descriptor_offset
        import_size = 20

    pe_offset = 0x80
    optional_size = 240
    headers_size = FILE_ALIGNMENT
    raw_size = align(len(code), FILE_ALIGNMENT)
    image_size = align(SECTION_RVA + len(code), SECTION_ALIGNMENT)
    dos = bytearray(pe_offset)
    dos[:2] = b"MZ"
    struct.pack_into("<I", dos, 0x3C, pe_offset)
    coff = struct.pack("<HHIIIHH", 0x8664, 1, 0, 0, 0, optional_size, 0x22)
    optional = bytearray(optional_size)
    struct.pack_into("<H", optional, 0, 0x20B)
    struct.pack_into("<I", optional, 4, raw_size)
    struct.pack_into("<I", optional, 16, SECTION_RVA)
    struct.pack_into("<I", optional, 20, SECTION_RVA)
    struct.pack_into("<Q", optional, 24, IMAGE_BASE)
    struct.pack_into("<II", optional, 32, SECTION_ALIGNMENT, FILE_ALIGNMENT)
    struct.pack_into("<HH", optional, 40, 6, 0)
    struct.pack_into("<HH", optional, 48, 6, 0)
    struct.pack_into("<II", optional, 56, image_size, headers_size)
    struct.pack_into("<H", optional, 68, 3)
    struct.pack_into("<QQQQ", optional, 72, 0x100000, 0x1000, 0x100000, 0x1000)
    struct.pack_into("<I", optional, 108, 16)
    struct.pack_into("<II", optional, 112 + 8, import_rva, import_size)
    section = struct.pack("<8sIIIIIIHHI", b".text\0\0\0", len(code), SECTION_RVA,
                          raw_size, headers_size, 0, 0, 0, 0, 0x60000020)
    output = dos + b"PE\0\0" + coff + optional + section
    output.extend(bytes(headers_size - len(output)))
    output.extend(code)
    output.extend(bytes(raw_size - len(code)))
    return bytes(output)


def make_pe32() -> bytes:
    code = b"\xc3"
    pe_offset, optional_size, headers_size = 0x80, 224, FILE_ALIGNMENT
    raw_size = FILE_ALIGNMENT
    dos=bytearray(pe_offset);dos[:2]=b"MZ";struct.pack_into("<I",dos,0x3c,pe_offset)
    coff=struct.pack("<HHIIIHH",0x14C,1,0,0,0,optional_size,0x102)
    optional=bytearray(optional_size);struct.pack_into("<H",optional,0,0x10B)
    struct.pack_into("<I",optional,4,raw_size);struct.pack_into("<III",optional,16,SECTION_RVA,SECTION_RVA,SECTION_RVA)
    struct.pack_into("<I",optional,28,0x00400000);struct.pack_into("<II",optional,32,SECTION_ALIGNMENT,FILE_ALIGNMENT)
    struct.pack_into("<HH",optional,40,6,0);struct.pack_into("<HH",optional,48,6,0)
    struct.pack_into("<II",optional,56,0x2000,headers_size);struct.pack_into("<H",optional,68,3)
    struct.pack_into("<IIII",optional,72,0x100000,0x1000,0x100000,0x1000);struct.pack_into("<I",optional,92,16)
    section=struct.pack("<8sIIIIIIHHI",b".text\0\0\0",len(code),SECTION_RVA,raw_size,headers_size,0,0,0,0,0x60000020)
    output=dos+b"PE\0\0"+coff+optional+section;output.extend(bytes(headers_size-len(output)));output.extend(code);output.extend(bytes(raw_size-len(code)))
    return bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--imports", action="store_true")
    parser.add_argument("--malformed", action="store_true")
    parser.add_argument("--pe32", action="store_true")
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.malformed:
        data=bytearray(64);data[:2]=b"MZ";struct.pack_into("<I",data,0x3c,0xfffffff0)
        args.output.write_bytes(data)
    elif args.pe32:
        args.output.write_bytes(make_pe32())
    else:
        args.output.write_bytes(make_pe(args.imports))


if __name__ == "__main__":
    main()
