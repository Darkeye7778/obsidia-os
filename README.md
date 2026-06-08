# Obsidia OS

Custom hobby OS kernel built from scratch.

---

## Requirements

Install dependencies (Ubuntu / WSL):

```bash
sudo apt update
sudo apt install -y build-essential nasm xorriso mtools qemu-system-x86 git curl
```

---

## Setup

### 1. Setup GitHub SSH (one-time per machine)

Generate key:

```bash
ssh-keygen -t ed25519 -C "your_email@example.com"
```

Get your public key:

```bash
cat ~/.ssh/id_ed25519.pub
```

Add it to:

GitHub → Settings → SSH and GPG keys → New SSH key

Test it:

```bash
ssh -T git@github.com
```

---

### 2. Clone the repository (SSH)

```bash
git clone git@github.com:Darkeye7778/obsidia-os.git
cd obsidia-os
```

---

### 3. Limine bootloader

Limine is automatically downloaded by the Makefile if the `limine/` folder is missing.

No manual Limine setup is required.

To build and run:

```bash
make clean
make run
```

---

## Development Workflow (Multiple Machines)

### First time on a new machine

```bash
git clone git@github.com:Darkeye7778/obsidia-os.git
cd obsidia-os

git clone https://github.com/limine-bootloader/limine.git --branch v7.x-binary --depth=1
```

Install dependencies (see Requirements).

---

### Normal workflow

Before working:

```bash
git pull
```

After making changes:

```bash
git add .
git commit -m "your message"
git push
```

---

### Switching machines

Machine A → push  
Machine B → pull  
Machine B → push  
Machine A → pull  

Always run `git pull` before editing.

---

## Current Progress (Base OS for GUI)

- Framebuffer + 8x8 text + console with editing
- IRQ-driven keyboard (ring buffer) + PIT timer (~100Hz)
- IDT + PIC remap + exception handling + interrupts enabled
- Custom OAR1 archive format + full VFS layer (read, ls, cat via unified interface, initrd mounted as /)
- Real heap with kfree (free list + split/coalesce) + improved PMM (hint cursor + free count)
- Paging enabled (higher-half + 4GiB low identity + Limine module/fb pointer preserve across CR3 + correct phys base + transition hardening)
- Syscall interface (int 0x80 DPL=3, fully working from ring 3: EXIT/WRITE/GETTICKS/YIELD/FBINFO with real FB values)
- GDT/TSS with proper kernel + user (ring 3) code/data segments and TSS rsp0 for privilege transitions
- Basic task/process structs (PID, state, kstack/ustack, cr3, rip/rsp, ring, name) + global list
- Cooperative multitasking (kernel thread creation, yield, context switch in asm, schedule, 'tasks' lister)
- Userland loader + safe ring-3 launch ('run <prog>'): flat bin from VFS, paged with USER flags at 0x400000, user stack, iretq using GDT selectors
- Tiny demo user program (hello_user.bin): prints via SYS_WRITE, GETTICKS, multiple YIELD, clean SYS_EXIT back to shell
- Interactive shell with both classic and Obsidia-flavored commands (status, demo, vfsinfo, tasks, run, etc.)

Phase 1 foundations complete (v0.1.0-alpha): GDT/TSS, tasks/coop multitasking, ring-3 userland + loader + syscalls, preserved console/shell.

Phase 2 (v0.2.0-alpha): Drivers & Real Storage (completed)
- block_device abstraction + registration + blkdevs cmd
- RAM disk (default ram0) + ATA/PIO real backend (QEMU -hda support)
- PCI skeleton + scan
- Disk image support in Makefile (obsidia_disk.img, run with -drive if=ide)
- VFS extended for write/create, ramfs backend mounted at /tmp (writable runtime)
- mounts, readblk, tmpfs_test, dispinfo commands
- Display abstraction (info, clear/fill/blit, backbuf placeholder, vsync hook)
- USB/xHCI, net (virtio), audio, mouse skeletons
- initrd/OAR remains read-only and mounted; all Phase 1 commands work
- Verified: make, existing + new shell cmds, block read, tmpfs write/read test

Phase 3A (v0.3.0-alpha): GUI Foundations (completed)
- Surface abstraction (surface_t + kmalloc backing + create/destroy/clear/fill/blit) + manager init
- Compositor (Z-order composite of visible windows to display back buffer + present; damage via surface dirty)
- window_t + manager (create with surface, destroy, move, focus, visibility, list)
- Events (queue, post key from input path, focus routing, process in gui tasks)
- gui-demo kernel task (creates movable demo window, draws primitives, runs compositor + processes events on yields)
- Full boot stability fix required for 3A: post-paging Limine module_request + initrd blob + fb access pointers now survive CR3 via explicit preserve (current_virt_to_phys + re-map same v->p before switch) + 4 GiB identity + corrected kernel high base + fb MMIO real-phys mapping. Removed all investigation DIAG spam; single "Paging enabled" milestone kept.
- All Phase 1/2 fully preserved (shell + every old/new command still works, no removal of console)
- Verified (see below)

See ROADMAP.md for Phase 3B+ and the detailed 3A implementation + verification checklist.

---

## Notes

- Runs in QEMU
- Uses Limine bootloader
- Freestanding kernel (no libc)

---

## List Until Self Editable (Base Complete for GUI)

1-5. Core boot, graphics, console, input, memory (PMM + real heap with free) - **Done** (enhanced with paging, IRQs, timer)
6. Initramfs + custom OAR format + VFS - **Done**
7. Interactive shell + Obsidia commands - **Done** (status, demo, etc. + classic compat)
8-9. Filesystem (VFS over OAR) + user program / syscall foundations - **Done**
10. Phase 1: GDT/TSS + tasks + cooperative multitasking + userland loader + ring3 execution + expanded syscalls for user progs - **Done** (v0.1.0-alpha)
11. Phase 2: Drivers & Real Storage (block dev, ATA+ramdisk, PCI skeleton, VFS write+ramfs, display abstraction, USB/net/audio/mouse skeletons, disk image, new shell cmds) - **Done** (v0.2.0-alpha)
12. Phase 3A: GUI Foundations (surfaces + manager, compositor, window_t + manager, events, drawing primitives, gui task demo, userland GUI groundwork) + paging/Limine pointer survival fix for full boot to shell+demo - **Done** (v0.3.0-alpha)
13+. Phase 3B Desktop Prototype, 3C Customization, etc. - See ROADMAP.md
**Phase 3A GUI foundations complete (v0.3.0-alpha). The boot loop (module_request / fb post-CR3) is resolved. Desktop / full GUI can now be built on this layer as a proper task/process.**

**Base OS is now ready for you to build the full GUI on top.**
Key capabilities exposed for GUI work:
- VFS for loading fonts, images, configs, "applets"
- Syscalls (SYS_WRITE, SYS_YIELD, SYS_GETTICKS, SYS_FBINFO, SYS_EXIT, ...)
- IRQ keyboard events (via event queue + gui_post + routing in gui tasks)
- Timer + cooperative yield for animation/scheduling
- Paging + higher half + safe kernel data (Limine pointers preserved)
- Surfaces/windows/compositor/events foundations + demo (movable window)
- Ability to add flat user binaries or future ELF user programs that use the syscall ABI

### Verification (Phase 3A complete)
```bash
make clean
make -j4          # must succeed, produce obsidia.iso + kernel.elf
make run          # QEMU: -cdrom obsidia.iso -serial stdio -drive ... -m 256
```
Expected:
- Serial: "Starting kernel..." then "Paging enabled" (no repeats, no triple fault/reboot loop)
- QEMU window: shell prompt appears ("This is real now.", "Type something:", ">")
- A demo window is visible (content area + border). Left/Right/Up/Down arrows move it when it has focus.
- Shell commands all work: `help`, `tasks` (idle + gui-demo), `run hello_user.bin` (userland syscall demo prints + yields + exits), `initrd`, `mounts`, `tmpfs_test`, `dispinfo`, `blkdevs`, `status`, `demo`, line editing, etc.
- Old console/shell behavior fully preserved alongside the new GUI foundations.

See ROADMAP.md for the exact Phase 3A checklist and "how to continue".
