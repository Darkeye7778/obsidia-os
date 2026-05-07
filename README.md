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

## Current Progress

- Framebuffer rendering
- Text rendering (8x8 font)
- Console with cursor + editing
- Keyboard input (modifiers, arrows)
- Line editing (in progress)

---

## Notes

- Runs in QEMU
- Uses Limine bootloader
- Freestanding kernel (no libc)

---

## List Until Self Editable

1. Add Makefile build system - Done
2. Add framebuffer text rendering - Done
3. Add on-screen console - Done
4. Add keyboard input - Done
5. Add memory manager - Done
   <ol type="A">
     <li>Request Limine memory map</li>
     <li>Print memory map regions</li>
     <li>Calculate total usable memory</li>
     <li>Build physical memory manager structs</li>
     <li>Mark usable pages as free</li>
     <li>Reserve kernel / bootloader / framebuffer memory</li>
     <li>Implement pmm_alloc_page()</li>
     <li>Implement pmm_free_page()</li>
     <li>Add meminfo shell command</li>
     <li>Add basic heap allocator groundwork</li>
   </ol>
6. Add initramfs support - Done
7. Add simple shell - Updated
8. Add filesystem read support - Done
9. Make personal Obsidia Archive Format (Remove TAR) - In Progress
10. Add filesystem write support - TBD
11. Add text editor - TBD
12. Add user program support - TBD
13. Add compiler toolchain groundwork - TBD 
