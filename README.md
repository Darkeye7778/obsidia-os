# Obsidia OS 

Custom hobby OS kernel built from scratch. 

--- 

## Requirements Install dependencies (Ubuntu / WSL):

bash

sudo apt update

sudo apt install -y build-essential nasm xorriso mtools qemu-system-x86 git curl

Setup

1. Clone the repository

git clone https://github.com/Darkeye7778/obsidia-os.git

cd obsidia-os

2. Clone Limine bootloader

git clone https://github.com/limine-bootloader/limine.git --branch v7.x-binary --depth=1

Build & Run

make clean

make run

Development Workflow (Multiple Machines)

First time on a new machine

git clone https://github.com/Darkeye7778/obsidia-os.git

cd obsidia-os

Install dependencies (see Requirements).

Normal workflow

Before working:

git pull

After making changes:

git add .

git commit -m "your message"

git push

Switching machines

Machine A → push

Machine B → pull

Machine B → push

Machine A → pull

Always run git pull before editing to avoid conflicts.

Current Progress

Framebuffer rendering

Text rendering (8x8 font)

Console with cursor + editing

Keyboard input (modifiers, arrows)

Line editing (in progress)

Notes

Runs in QEMU
Uses Limine bootloader
Freestanding kernel (no libc)

---
