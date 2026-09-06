# Obsidia Windows ABI subsystem

This directory owns Windows userspace policy: PE process bootstrap, DLL/module
loading and import resolution, followed by Obsidia implementations of NT and
Win32-facing APIs. It is part of Obsidia and has no external runtime dependency.

The current milestone maps validated, import-free x86_64 PE32+ images through the
kernel's format-neutral image mapper. PE parsing exposes image, section, import,
and relocation metadata. Imported images fail with the first missing module name.
The next increment moves bootstrap/relocation/import work behind this userspace
boundary as the generic process-construction API grows.
