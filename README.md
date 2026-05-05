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

### 3. Clone Limine bootloader (REQUIRED)

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

## Future Improvements

- Auto-download Limine in Makefile
- Command system / shell
- Scrolling + buffer system
