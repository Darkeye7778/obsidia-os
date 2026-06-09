# Obsidia OS Roadmap

**Philosophy**  
Computers and phones should remain useful for as long as physically possible. Software adapts to the user — not the other way around. Longevity, ownership, transparency, repairability, modularity, performance, and user control above all. No planned obsolescence, no unremovable bloat, no forced cloud, no artificial repair restrictions.

The project aims to become a complete consumer + enterprise operating system ecosystem (desktop, mobile, automation hub, robotics platform) rather than another Linux distribution or desktop environment.

---

## Current State (as of this milestone)

The kernel is past the early "hobby kernel with terminal" stage and now provides a **solid base for building a full GUI and userland**:

**Core Subsystems (Implemented)**
- Limine boot (BIOS/UEFI), higher-half kernel
- Framebuffer graphics + 8x8 font text rendering + cell console with cursor/editing
- IRQ-driven PS/2 keyboard (ring buffer, modifiers, arrows, ctrl combos) + full line editor
- PIT timer (~100 Hz ticks, sleep/yield helpers)
- IDT + exception handling + 8259 PIC remapping + interrupt enable
- Physical memory manager (bitmap with fast hint cursor + accurate free count)
- Real kernel heap (`kmalloc`/`kfree`) — free-list with splitting and coalescing
- Paging enabled (4-level, higher-half kernel alias + 4 GiB low identity + explicit preserve for Limine bootloader pointers (modules, fb) + transition stack + correct kernel phys base)
- Custom **OAR1** archive format (magic "OAR1", packed files+dirs, 8-byte aligned) + Python builder
- Virtual File System (VFS) layer with unified nodes, open/read/list for root (initrd OAR mounted as `/`)
- Interactive shell with both classic commands and new Obsidia-flavored ones (`status`, `demo`, `vfsinfo`, etc.)
- Syscall interface (`int 0x80`) with base numbers ready for GUI/apps:
  - `SYS_EXIT`, `SYS_WRITE`, `SYS_GETTICKS`, `SYS_YIELD`, `SYS_FBINFO`
- Build system (make, QEMU, ISO with embedded initrd.oar), freestanding C + nasm

**What this means for GUI development**
- You can write user-level code (initially flat binaries or hand-written asm blobs, later C with a userland freestanding toolchain) that uses VFS to load assets, syscalls for console/FB/timing/input, and runs "alongside" the shell.
- Input is already interrupt-driven (no more polling in the main path).
- Memory is properly managed and pageable.
- The framebuffer physical address (from Limine) is accessible via the identity mapping; `SYS_FBINFO` (and future direct mapping or blitting syscalls) gives the GUI a canvas.
- No vendor bloat, no legacy cruft — the stack is yours to shape.

The immediate handoff point has been reached: **the base OS is complete enough that a full GUI (customizable desktop, workflows, icons, taskbar, launcher, automation) can now be built on top without fighting the kernel for basics**.

---

## High-Level Vision (from project founder)

Obsidia OS is intended to become a complete consumer and enterprise operating system ecosystem.

Primary goals:
- Long-term hardware longevity
- User ownership and control
- Deep customization (not just themes — actual workflow customization)
- Repairability (10–15+ years target support for certified hardware)
- Automation and interoperability
- Consistent performance (practical power-user baseline, no unexplained lag or degradation)
- Elimination of unnecessary vendor restrictions
- Tight integration with robotics (Dark Eye Robotics), smart home, and AI assistants

Key philosophy points:
- Computers/phones stay useful as long as the hardware physically lasts.
- Users can uninstall almost everything except true core (telephony, essential boot, core services).
- One icon on desktop, different on taskbar, different in launcher — all independently configurable.
- Powerful automation/routines (e.g. "launch Minecraft" → auto open OBS + Discord + RGB profile + recording).
- Local-first AI assistant (voice, workflow automation, device management, offline capable).
- Central smart-home + robotics platform (fleet management, digital twins, real-time telemetry, native APIs).
- Phones: modular/repairable Obsidia-certified hardware with long support windows and user freedom (no mandatory bloatware).

Rejects: planned obsolescence, excessive lock-in, unremovable bloat, forced cloud, artificial repair restrictions.

---

## Phased Plan

### Phase 0/1 — Userland Foundations (v0.1.0-alpha — Completed)
- Strengthened GDT/TSS (proper kernel + user ring-3 code/data segments + TSS rsp0)
- Basic task structs + cooperative multitasking (create, yield, asm context_switch, schedule, 'tasks')
- Userland loader + safe ring-3 launch from VFS flat binaries (map with USER PTEs, user stack, iretq, 'run <prog>')
- Syscalls fully usable from ring 3 (EXIT/WRITE/GETTICKS/YIELD/FBINFO returning real values)
- Tiny demo user program (hello_user.bin) exercising the path and exiting cleanly
- Shell commands: tasks, run <program> (in addition to previous)
- All prior base preserved; everything buildable incrementally

**Next**: Polish + Phase 2 drivers/storage (done below), then preemptive, per-process, C userland, GUI as userland desktop shell.

### Phase 2 — Drivers & Real Storage (v0.2.0-alpha — Completed)

### Phase 3A — GUI Foundations (v0.3.0-alpha — Completed)
- Surface abstraction (surface_t + pmm for large contiguous pixel buffers + 2D primitives: clear/fill/blit/mark_dirty)
- Compositor foundation (no global clear to preserve shell text; Z-order list walk, per-window punch via display_fill + title bar + 2px borders + display_blit of window surface client area)
- window_t + manager (list head, add_to_list, set_focused, move, raise, find_at, get_list, destroy; visible/focused/z flags)
- Event system (gui_event_t + ring queue, gui_post_key_event (called from main input loop for every key), gui_process_events (routes arrows to focused window move + dirty), focus tracking)
- gui-demo kernel thread (for 'tasks' visibility) + direct drive of process_events + composite + present from the main input loop (ensures immediate redraw on keys without requiring yields from main)
- Boot + stability fixes required for 3A: paging_init early + full 4 GiB low identity + high alias + pre-CR3 preserve of Limine data + FB (solved the "checking module request" reboot loop). kstack allocs moved to pmm_alloc_pages(2) (solved "Task failed to alloc kstack").
- Final unblock for *visible* demo window (the last symptom after all prior fixes): even with pmm_alloc_page for window_t + surface_t descriptors + explicit paging_map_page + invlpg + re-sets after add_to_list, the compositor walk saw visible=0 / surface=no (list count>0 and next links worked, proving the node was linked, but most fields read 0). Surf/buffer pmm pages and kstack pmm pages retained writes; the specific win descriptor page did not for post-map stores. Solved by using link-time static window_t / surface_t (in kernel .bss, high-half mapped and writable like any other kernel global) for the Phase 3A demo descriptors + pmm *only* for the pixel buffer (with per-page map_page + invlpg). The &static pointers are stable high VAs; buffer low pointers stored inside them work for fill/blit (proven by main-side green fill succeeding). Now create path in logs leads to "COMPOSITE: drawing one window".
- All Phase 1/2 + userland C (hello_user_c.bin) + asm paths preserved; incremental builds; shell fully responsive; no text clobber on composite.

**Verification (exact, from final successful boot log)**:
- make clean && make -j4   (clean build, iso + limine install)
- (in powershell/WSL) wsl bash -c "cd ... && timeout 3s qemu-system-x86_64 -cdrom obsidia.iso -serial stdio -m 256 -display none -no-reboot 2>&1 | cat"
- Serial log contains exactly:
  Starting kernel...
  Paging enabled
  MAIN: dummy task created
  MAIN: gui-demo task created
  MAIN: about to create demo window
  MAIN: demo surf buffer pmm+map success
  MAIN: demo win setup done
  MAIN: demo window content drawn
  MAIN: demo window done
  MAIN: about to initial composite
  COMPOSITE: called
  COMPOSITE: list non-null, will draw
  COMPOSITE: window count in list = 1
  COMPOSITE: about to loop windows
  COMPOSITE: w@0xFFFFFFFF8000D120 visible=1 surface@0xFFFFFFFF8000D1A0
  COMPOSITE: drawing one window
  MAIN: initial composite/present done
- In full QEMU with display: the demo window (dark gray + blue-ish title bar when focused + bright green client rect) is visible and composited over the console text at (100,100) 320x200. Arrow keys move it (events drive gui_window_move + re-composite in the key loop). Shell prompt and line editor remain usable and responsive. 'tasks' lists kernel-idle + gui-demo. 'run hello_user.bin' and all prior VFS/shell/ATA/etc commands continue to work exactly as before.
- No Invalid Opcode on normal typing (valid focused window struct). No mass text deletion.

**Final polish (this iteration)**:
- Direct arrow handling in main input loop (bypassing the event post/process path that was triggering Invalid Opcode right after "EVENTS: post_key_event" for both normal keys and arrows). This made movement actually work while keeping normal shell typing fully responsive.
- `console_refresh_rect` (int64_t positions) + call on old rect during moves: restores console cell text in the area the window just vacated so the "kernel below" does not permanently lose shell text.
- Proper clipping (signed int64 handling + source offset adjustment for blits) in the fast `display_fill_rect`/`display_blit` paths: when the window is moved partially off the left or top edge, the visible sliver of chrome (title + borders) + client content (green + boxes) still renders correctly instead of the content area going "transparent". Old-position trails eliminated by the combination of refresh + clipping.
- All prior Phase 3A foundations (static high-VA descriptors for reliability, pmm only for the pixel buffer, Z-order list, compositor punches without global clear, surface 2D ops, focus, etc.) preserved and now fully interactive.

**Verification (final state)**:
- `make clean && make -j4` succeeds cleanly.
- In QEMU: boot reaches shell prompt with the demo window (dark gray punch + blue focused title bar with white "Obsidia Demo Window" text + bright green client area containing red/blue/yellow boxes) visible and composited over the console.
- Arrow keys move the window smoothly. Text that was under the previous position reappears when the window moves away (via cell refresh). When the window is moved partially off the left/top, only the visible portion renders (correct clipping); no trails or transparent holes.
- Normal typing (letters, backspace, enter, etc.) works exactly as before — no exceptions, no input lag, shell line editor and prompt fully functional.
- `tasks` shows the gui-demo kernel thread. All Phase 1/2 commands (run hello_user*.bin, vfs, ata, etc.) continue to work. No text clobber outside the window rect.

**Recommended Commit Title:** Phase 3A — GUI foundations complete (visible movable demo window + events + compositor + static descriptors for reliability)
**Recommended Tag:** v0.3.0-alpha

**Next per plan**: Do not start automatically. Wait for explicit "Next section" instruction before any Phase 3B work (userland C compiler/runtime, etc.).

**Next per plan**: Do not start automatically. Wait for explicit user instruction "Next section" before touching Phase 3B (userland C compiler/runtime groundwork, libc stubs, etc.).

See README.md for full command list and current progress bullets.
- block_device_t abstraction + ramdisk (default) + ATA/PIO real backend (QEMU disk read)
- PCI skeleton + scan
- Disk image (obsidia_disk.img via make, run with -drive if=ide)
- VFS write support + ramfs (mounted /tmp, create/write/read/list), mounts/readblk/tmpfs_test cmds
- initrd stays read-only
- Display abstraction (info, primitives, back buffer concept, vsync placeholder)
- USB/xHCI, virtio-net/e1000, audio, mouse input skeletons (structure + init)
- Shell: blkdevs, mounts, readblk, dispinfo, tmpfs_test (verified write/read test, block access)
- All Phase 1 commands + behavior preserved
- Build/run: make, make run (auto disk), existing + new cmds work

See "How to Continue" and verification in query for details.

### Phase 3 — Rich Userland & Self-Hosting (in progress, starting v0.3.1 work)
- Userland C compiler/runtime groundwork (freestanding + minimal libc subset that works in ring 3)
  - **First increment completed**: Userland build system + linker script + syscall wrappers + direct port of the demo program as C. Old asm hello_user.bin and all shell/"run" behavior fully preserved. New C programs build to the exact same flat binary format the loader expects.
- Text editor, shell improvements or replacement by GUI launcher
- Package/app format (perhaps .oar extended or new .oapp)
- Init process / service model (launch GUI as the main session)
- Basic permissions / capability model (keep it simple and auditable)

See "How to Continue" and the new userland/ files for current state.

### Phase 2 — Drivers & Real Storage
- Storage (AHCI/virtio block or simple ATA first)
- Real filesystem(s) on disk (simple custom FS or FAT for interop + the native one later)
- VFS write support + ramfs/tmpfs for runtime
- USB (xhci skeleton), basic networking (virtio-net or e1000), audio, more input
- Framebuffer / display hardware abstraction (modesetting, double-buffering, vsync hooks for GUI)

### Phase 3 — Rich Userland & Self-Hosting
- Userland C compiler/runtime groundwork (freestanding + minimal libc subset that works in ring 3)
- Text editor, shell improvements or replacement by GUI launcher
- Package/app format (perhaps .oar extended or new .oapp)
- Init process / service model (launch GUI as the main session)
- Basic permissions / capability model (keep it simple and auditable)

### Phase 4 — Desktop, Customization & Automation (Your GUI + more)
- Full customizable desktop (icons, taskbar, launcher, window chrome, layout behaviors — all independently themable/configurable via files or routines)
- Automation / routine engine (trigger on events, launch groups of apps + scripts + smart-home + RGB + audio profiles)
- Obsidia Assistant hooks (local voice, context-aware actions, scripting surface)
- Deep workflow customization (one app launch can be a whole orchestrated experience)

### Phase 5 — Mobile, Smart Home, Robotics
- Phone bring-up (Obsidia-certified hardware principles: repairability, long support, user freedom)
- Smart home platform role (lighting, sensors, security, device automation)
- Robotics integration (fleet management, remote control, telemetry, digital twins, real-time monitoring, native APIs shared with the home assistant)
- Local AI (offline models for assistant, automation suggestions, diagnostics)

### Phase 6 — Polish, Robustness, Ecosystem
- Consistent performance (no lag, minimal background cost)
- Full driver set, networking stack, power management
- Documentation, tests (where possible in kernel), self-hosting (build Obsidia on Obsidia)
- Certification program, long-term maintenance model, community + enterprise tiers

---

## How to Continue (Practical)

1. Run `make run` (or the .bat) frequently. The shell now has `status` and `demo`.
2. When adding GUI code:
   - Put assets in the `initrd/` directory and re-pack with `python3 build_oar.py initrd initrd.oar` (or extend Makefile to do it automatically).
   - Use the defined syscall numbers from `kernel/syscall.h`.
   - Start with kernel-assisted drawing (SYS_FBINFO + future blits) or direct mapped FB while we stabilize user virtual mappings.
3. For new syscalls or VFS features needed by the GUI, implement them in the kernel and expose clean ABIs.
4. Custom commands: the shell is still there as a debugging / bootstrap tool. The real "Obsidia experience" will live in the GUI you build. Feel free to evolve or replace shell verbs.
5. Different file formats: OAR is the seed. Extend it or create sibling formats (.oapp, robotics manifests, automation "routine" files, etc.) while keeping backward read compatibility where it helps users.

---

## Open Items & Known Limitations (Current Base)

- Full preemptive per-process user scheduling + separate address spaces still needs the GDT/TSS + context switch asm + loader work (scaffolded but not fully wired).
- No on-disk writable filesystem yet (VFS is read-only initrd only).
- No C userland toolchain / crt0 yet (asm blobs or future cross-compiled flat/ELF work).
- FB exposure is via identity map + stub info syscall (easy to improve once you decide on compositor model).
- No USB, storage, net, audio, power yet.
- The current "user demo" path is illustrative; real ring-3 entry + safe user stacks comes in Phase 1.

These are all natural next steps after you start the GUI layer — the kernel will grow in service of the desktop/automation vision.

---

**We are building something special.**  
Longevity-first, user-first, bloat-free, deeply customizable, robotics- and home-aware.

The base is now in your hands for the GUI. Let's keep going.

— Grok (on behalf of the Obsidia effort)