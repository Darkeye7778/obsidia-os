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
- Paging enabled (higher-half kernel + 256MiB low identity)
- Syscall interface (int 0x80 DPL=3, fully working from ring 3: EXIT/WRITE/GETTICKS/YIELD/FBINFO with real FB values)
- GDT/TSS with proper kernel + user (ring 3) code/data segments and TSS rsp0 for privilege transitions
- Basic task/process structs (PID, state, kstack/ustack, cr3, rip/rsp, ring, name) + global list
- Cooperative multitasking (kernel thread creation, yield, context switch in asm, schedule, 'tasks' lister)
- Userland loader + safe ring-3 launch ('run <prog>'): flat bin from VFS, paged with USER flags at 0x400000, user stack, iretq using GDT selectors
- Tiny demo user program (hello_user.bin): prints via SYS_WRITE, GETTICKS, multiple YIELD, clean SYS_EXIT back to shell
- Interactive shell with both classic and Obsidia-flavored commands (status, demo, vfsinfo, tasks, run, etc.)

Phase 1 foundations complete (v0.1.0-alpha): GDT/TSS, tasks/coop multitasking, ring-3 userland + loader + syscalls, preserved console/shell.

Phase 2 (v0.2.0-alpha in progress/completed): Drivers & Real Storage
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
See ROADMAP.md for details and Phase 3.

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
12+. Preemptive scheduler polish, C userland, full GUI as userland, more FS, etc. - See ROADMAP.md
**Base OS Phase 2 complete. GUI can now use block storage, writable FS (/tmp), display layer.**

**Base OS is now ready for you to build the full GUI on top.**
Key capabilities exposed for GUI work:
- VFS for loading fonts, images, configs, "applets"
- Syscalls (SYS_WRITE, SYS_YIELD, SYS_GETTICKS, SYS_FBINFO, SYS_EXIT, ...)
- IRQ keyboard events (via future event queue or polling getkey)
- Timer for smooth animation / scheduling
- Paging + higher half (safe virtual memory model)
- Ability to add flat user binaries or future ELF user programs that use the syscall ABI
