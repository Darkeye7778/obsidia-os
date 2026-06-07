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
- Syscall interface (int 0x80, base numbers for write/yield/exit/ticks/fbinfo)
- Interactive shell with both classic and Obsidia-flavored commands (status, demo, vfsinfo, etc.)
- Foundations for user programs / GUI: VFS for assets, syscalls for services, IRQ input events, FB access path, timer for scheduling/animation

The kernel is now in a state where a full GUI (windowing, widgets, custom workflows) can be developed on top as user-level code.

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
10+. Full per-process user mode, preemptive scheduler with context switch, storage drivers, C userland toolchain, writeable FS, desktop/GUI, automation, robotics integration - See ROADMAP.md

**Base OS is now ready for you to build the full GUI on top.**
Key capabilities exposed for GUI work:
- VFS for loading fonts, images, configs, "applets"
- Syscalls (SYS_WRITE, SYS_YIELD, SYS_GETTICKS, SYS_FBINFO, SYS_EXIT, ...)
- IRQ keyboard events (via future event queue or polling getkey)
- Timer for smooth animation / scheduling
- Paging + higher half (safe virtual memory model)
- Ability to add flat user binaries or future ELF user programs that use the syscall ABI
