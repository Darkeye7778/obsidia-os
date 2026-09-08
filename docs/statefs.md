# Statefs v2

Statefs is Obsidia's first persistent writable filesystem. The kernel mounts the
primary ATA bootstrap disk at `/state`; the read-only initrd remains `/` and the
volatile RAM filesystem remains `/tmp`.

The v2 volume is deliberately bounded: 64 total files/directories, 32 KiB per
file, 127-byte mount-relative paths, and two 64 KiB on-disk snapshot slots. A
commit serializes the validated in-memory hierarchy into the inactive slot,
writes and flushes its payload, then writes and
flushes a generation-numbered checksummed commit header. Mount selects the
newest fully valid slot and falls back to the prior slot after a torn or corrupt
header/payload. Existing v1 flat-file volumes are loaded and upgraded on their
next commit.

The public `<obsidia/fs.h>` API provides stat, indexed bounded directory
enumeration, directory creation, file/directory deletion, synchronization, and
same-filesystem rename/move/replacement. Open nodes cannot be deleted or replaced;
nonempty directories cannot be deleted; moves cannot create directory cycles.
Settingsd uses `settings.new` + sync + replace for its versioned, checksummed
settings record. Applications cannot access raw ATA through these APIs.

This is a bootstrap persistence substrate, not the final general-purpose
filesystem. Its timestamps are boot-relative ticks rather than wall-clock time,
and it still lacks large files, permissions/ownership, open-file unlink semantics,
partitions, free-space queries, and a standalone repair utility.
