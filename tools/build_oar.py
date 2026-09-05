#!/usr/bin/env python3
import os
import struct
import sys

OAR_MAGIC = b"OAR1"
OAR_VERSION = 1
OAR_TYPE_FILE = 1
OAR_TYPE_DIR = 2

def align8(value):
    return (value + 7) & ~7

def collect_entries(root):
    entries = []

    for dirpath, dirnames, filenames in os.walk(root):
        rel_dir = os.path.relpath(dirpath, root)

        if rel_dir != ".":
            name = rel_dir.replace("\\", "/") + "/"
            entries.append((name, OAR_TYPE_DIR, b""))

        for filename in filenames:
            full_path = os.path.join(dirpath, filename)
            rel_path = os.path.relpath(full_path, root).replace("\\", "/")

            with open(full_path, "rb") as f:
                data = f.read()

            entries.append((rel_path, OAR_TYPE_FILE, data))

    return entries

def build_oar(input_dir, output_file):
    entries = collect_entries(input_dir)

    with open(output_file, "wb") as out:
        # Main header:
        # magic[4], version u32, file_count u32, flags u32
        out.write(struct.pack("<4sIII", OAR_MAGIC, OAR_VERSION, len(entries), 0))

        for name, entry_type, data in entries:
            name_bytes = name.encode("utf-8")

            # Entry header:
            # name_len u32, type u32, size u64, flags u64
            out.write(struct.pack("<IIQQ", len(name_bytes), entry_type, len(data), 0))

            out.write(name_bytes)
            out.write(data)

            padding = align8(out.tell()) - out.tell()
            if padding:
                out.write(b"\0" * padding)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: build_oar.py <input_dir> <output.oar>")
        sys.exit(1)

    build_oar(sys.argv[1], sys.argv[2])
