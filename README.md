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

### 1. Clone the repository

```bash
git clone https://github.com/Darkeye7778/obsidia-os.git
cd obsidia-os
```

### 2. Clone Limine bootloader (REQUIRED)

```bash
git clone https://github.com/limine-bootloader/limine.git --branch v7.x-binary --depth=1
```

Verify:

```bash
ls limine
```

You should see `limine.exe`.

---

## Build & Run

```bash
make clean
make run
```

---

## Development Workflow (Multiple Machines)

### First time on a new machine

```bash
git clone https://github.com/Darkeye7778/obsidia-os.git
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

Always run `git pull` before editing to avoid conflicts.

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

This version is:

- clean  
- copy/paste ready  
- works across multiple machines without path issues  

---

## Future Improvement

👉 Automatically download Limine in the Makefile (one-command setup)
