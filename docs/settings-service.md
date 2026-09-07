# Settings service v1

`settingsd` owns the authoritative configuration for the current userspace
session. Its named `settings` endpoint accepts versioned GET, SUBSCRIBE, and
diagnostic requests. SET requests use a separate inherited IPC capability held
by the trusted desktop and granted only to metadata-authorized configuration
clients. No request field can claim privilege.

Public values are typed and namespaced in `obsidia/settings.h`. Clients cache
validated values, subscribe once to relevant categories, and repaint only after
a notification. Subscription endpoints are bounded and are reclaimed when the
owner process or its endpoint disappears. If `settingsd` stops, consumers keep
their last validated local theme/layout and do not query during painting.

V1 uses built-in defaults and an in-memory storage provider. The explicit
`storage_load`/`storage_commit` boundary is intentionally inactive until the VFS
can provide reliable replacement/atomic-write semantics; applications never
edit a backing file directly. Protocol and any future stored format are separate
versioned contracts.
