# Installed application catalog v1

`appd` is the userspace authority for the set of installed applications. At
startup it reads `system/apps/catalog.list`, parses the referenced `.oam`
manifests, validates them, and publishes a versioned read-only snapshot through
the `applications` service. Launcher and shell clients use libobsidia's catalog
API and cache the snapshot; they never parse manifests while painting.

## Manifest contract

An OAM v1 manifest is bounded printable-ASCII `key=value` text with these required,
unique fields:

```
manifest-version=1
id=org.obsidia.example
name=Example
executable=example.obsx
version=1.0
icon=application
capabilities=none
```

Unknown fields are ignored for forward evolution. Missing, duplicated,
oversized, malformed, or unsupported required values reject that manifest
without discarding other valid applications. Stable IDs and exact executable
paths must each be unique. Executable format is deliberately absent from the
manifest: `obs_app_launch()` identifies OBSX, ELF, or PE from file contents.

## Trust and ownership

Manifest metadata is descriptive. It cannot grant privileged capabilities.
`appd` applies a separate system-owned exact ID/path authorization policy before
publishing records. A running window is associated with a catalog entry only by
the authenticated owner PID's kernel-reported image path; titles and
self-reported IDs are never identity.

The catalog is not an execution allowlist. Portable or unregistered executables
remain launchable and receive generic, temporary shell groups with no privileged
grants.

## Shell state

Installed applications and user choices are separate. `settingsd` owns the
ordered `shell.pinned-applications` list and exposes bounded query, pin, and
unpin operations. Pins neither modify manifests nor start applications. The
desktop combines its cached catalog, settings-owned pins, and displayd's live
window stream.

`settingsd` persists the ordered pin list and scalar desktop settings in the
bounded `/state` filesystem. It writes a replacement record, synchronizes it,
and atomically renames it; statefs commits through alternating checksummed disk
snapshots. This is currently a single-system-session store. Per-user scoping can
be added later without changing catalog, launcher, or shell-group contracts.
