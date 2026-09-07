# Desktop presentation contract

Obsidia keeps four desktop concepts separate:

- **Theme** (`obsidia/theme.h`) contains semantic colors and presentation metrics. Renderers request roles such as `titlebar_active` or `text_secondary`; they do not own the shipped palette. `obs_theme_get()` resolves the live identity selected through `settingsd`; the compile-time selector remains useful for isolated presentation tests.
- **Layout** (`obsidia/desktop_layout.h`) describes background mode, panels, panel edges and sizes, module order, and desktop shortcuts. It calculates the work area sent to displayd, so maximize follows layout rather than a duplicated panel constant.
- **Module** is a bounded shell component in a configured panel. V1 provides launcher, running-applications, and spacer module types. Modules are compiled into the trusted desktop; this is not yet a third-party plugin ABI.
- **Window policy** remains in displayd: focus, z-order, drag, resize, and state transitions. Theme selection changes chrome appearance, not those behaviors; layout changes panel placement, not window policy.

`libobsidia` owns the freestanding canvas, project-owned bitmap font, theme definitions, layout geometry, icons, and application metadata lookup. These are ordinary userspace resources. They add no kernel object types and do not change surface capabilities: displayd alone retains final presentation authority, applications draw only into their client surfaces, and the desktop receives window-management events rather than client pixels.

The shipped layout and theme are defaults, not an immutable Obsidia interface.
`settingsd` now changes the built-in theme identity, primary panel edge/size,
background mode, and text scale at runtime. Desktop and displayd retain local
validated copies, so appearance changes neither recreate the session nor add
settings policy to the kernel.
