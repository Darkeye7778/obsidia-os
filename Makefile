CC = gcc
LD = ld

CFLAGS = -ffreestanding -m64 -mcmodel=kernel -mno-red-zone -fno-pic -Ikernel
LDFLAGS = -nostdlib -z max-page-size=0x1000 -T linker.ld

KERNEL = kernel.elf
ISO = obsidia.iso

LIMINE_DIR = limine
LIMINE_BIN = $(LIMINE_DIR)/limine.exe

INITRD = initrd.oar

OBJS = \
	main.o \
	framebuffer.o \
	font.o \
	console.o \
	keyboard.o \
	line_editor.o \
        shell.o \
	memory.o \
	heap.o \
	initrd.o \
	vfs.o \
	ramfs.o \
	block.o \
	pci.o \
	ata.o \
	display.o \
	usb.o \
	net.o \
	audio.o \
	mouse.o \
	gdt.o \
	task.o \
	context.o \
	idt.o \
	isr.o \
	timer.o \
	paging.o \
	syscall.o \
	gui_surface.o \
	gui_window.o \
	gui_compositor.o \
	gui_events.o

all: build

$(LIMINE_DIR):
	git clone https://github.com/limine-bootloader/limine.git --branch v7.x-binary --depth=1 $(LIMINE_DIR)

main.o: kernel/main.c
	$(CC) $(CFLAGS) -c kernel/main.c -o main.o

framebuffer.o: kernel/drivers/framebuffer.c
	$(CC) $(CFLAGS) -c kernel/drivers/framebuffer.c -o framebuffer.o

font.o: kernel/drivers/font.c
	$(CC) $(CFLAGS) -c kernel/drivers/font.c -o font.o

console.o: kernel/console/console.c
	$(CC) $(CFLAGS) -c kernel/console/console.c -o console.o

keyboard.o: kernel/drivers/keyboard.c
	$(CC) $(CFLAGS) -c kernel/drivers/keyboard.c -o keyboard.o

line_editor.o: kernel/input/line_editor.c
	$(CC) $(CFLAGS) -c kernel/input/line_editor.c -o line_editor.o

shell.o: kernel/shell/shell.c
	$(CC) $(CFLAGS) -c kernel/shell/shell.c -o shell.o

memory.o: kernel/memory/memory.c
	$(CC) $(CFLAGS) -c kernel/memory/memory.c -o memory.o

heap.o: kernel/memory/heap.c
	$(CC) $(CFLAGS) -c kernel/memory/heap.c -o heap.o

initrd.o: kernel/initrd/initrd.c
	$(CC) $(CFLAGS) -c kernel/initrd/initrd.c -o initrd.o

vfs.o: kernel/vfs/vfs.c
	$(CC) $(CFLAGS) -c kernel/vfs/vfs.c -o vfs.o

ramfs.o: kernel/vfs/ramfs.c
	$(CC) $(CFLAGS) -c kernel/vfs/ramfs.c -o ramfs.o

idt.o: kernel/idt.c
	$(CC) $(CFLAGS) -c kernel/idt.c -o idt.o

isr.o: kernel/isr.asm
	nasm -f elf64 kernel/isr.asm -o isr.o

timer.o: kernel/timer.c
	$(CC) $(CFLAGS) -c kernel/timer.c -o timer.o

paging.o: kernel/paging.c
	$(CC) $(CFLAGS) -c kernel/paging.c -o paging.o

syscall.o: kernel/syscall.c
	$(CC) $(CFLAGS) -c kernel/syscall.c -o syscall.o

block.o: kernel/block.c
	$(CC) $(CFLAGS) -c kernel/block.c -o block.o

pci.o: kernel/pci.c
	$(CC) $(CFLAGS) -c kernel/pci.c -o pci.o

ata.o: kernel/ata.c
	$(CC) $(CFLAGS) -c kernel/ata.c -o ata.o

display.o: kernel/display.c
	$(CC) $(CFLAGS) -c kernel/display.c -o display.o

usb.o: kernel/drivers/usb.c
	$(CC) $(CFLAGS) -c kernel/drivers/usb.c -o usb.o

net.o: kernel/drivers/net.c
	$(CC) $(CFLAGS) -c kernel/drivers/net.c -o net.o

audio.o: kernel/drivers/audio.c
	$(CC) $(CFLAGS) -c kernel/drivers/audio.c -o audio.o

mouse.o: kernel/drivers/mouse.c
	$(CC) $(CFLAGS) -c kernel/drivers/mouse.c -o mouse.o

gdt.o: kernel/gdt.c
	$(CC) $(CFLAGS) -c kernel/gdt.c -o gdt.o

# Phase 3A GUI Foundations
gui_surface.o: kernel/gui/surface.c
	$(CC) $(CFLAGS) -c kernel/gui/surface.c -o gui_surface.o

gui_window.o: kernel/gui/window.c
	$(CC) $(CFLAGS) -c kernel/gui/window.c -o gui_window.o

gui_compositor.o: kernel/gui/compositor.c
	$(CC) $(CFLAGS) -c kernel/gui/compositor.c -o gui_compositor.o

gui_events.o: kernel/gui/events.c
	$(CC) $(CFLAGS) -c kernel/gui/events.c -o gui_events.o


task.o: kernel/task.c
	$(CC) $(CFLAGS) -c kernel/task.c -o task.o

context.o: kernel/context.asm
	nasm -f elf64 kernel/context.asm -o context.o

$(KERNEL): $(OBJS) userland/hello_user.bin
	$(LD) $(LDFLAGS) $(OBJS) -o $(KERNEL)

$(INITRD): initrd/hello_user.bin
	python3 build_oar.py initrd $(INITRD)

iso: $(LIMINE_DIR) $(KERNEL) $(INITRD)
	# Ensure limine bootloader files are in place (self-contained clean build)
	mkdir -p iso/boot/limine
	cp $(LIMINE_DIR)/limine-bios-cd.bin iso/boot/limine/
	cp $(LIMINE_DIR)/limine-bios.sys iso/boot/limine/
	cp $(LIMINE_DIR)/limine-uefi-cd.bin iso/boot/limine/
	# Generate clean limine.cfg (no debug hacks that can break module loading)
	printf '%s\n' 'TIMEOUT=0' '' ':Obsidia OS' '    PROTOCOL=limine' '    KERNEL_PATH=boot:///boot/kernel.elf' '    MODULE_PATH=boot:///boot/initrd.oar' '    MODULE_STRING=initrd.oar' '    RESOLUTION=1280x720' > iso/boot/limine/limine.cfg
	# Always (re)pack the initrd from current contents of initrd/ directory.
	# This ensures "make clean && make run" (or any iso build) automatically
	# includes the latest hello_user.bin etc. without any manual python step.
	python3 build_oar.py initrd $(INITRD)
	cp $(KERNEL) iso/boot/kernel.elf
	cp $(INITRD) iso/boot/
	xorriso -as mkisofs \
		-b boot/limine/limine-bios-cd.bin \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image \
		-o $(ISO) \
		iso
	$(LIMINE_BIN) bios-install $(ISO)

run: iso obsidia_disk.img
	qemu-system-x86_64 -cdrom $(ISO) -serial stdio -drive file=obsidia_disk.img,format=raw,if=ide -m 256

# Create a small test disk image for ATA/PIO testing (10MB raw)
obsidia_disk.img:
	dd if=/dev/zero of=$@ bs=1M count=10 2>/dev/null || true
	@echo "Test disk image created: $@ (attach with -drive ...if=ide)"

clean:
	rm -f *.o $(KERNEL) $(ISO)
	rm -f userland/*.bin initrd/hello_user.bin
	rm -f $(INITRD)
	rm -f obsidia_disk.img
	# Note: source iso/ structure (for limine files) is preserved for self-contained builds; build will repopulate needed files from limine clone.

userland/hello_user.bin: userland/hello_user.asm
	nasm -f bin -o $@ $<

initrd/hello_user.bin: userland/hello_user.bin
	cp $< $@

programs: userland/hello_user.bin initrd/hello_user.bin
	@echo "User program prepared in initrd/. The next 'make' (or 'make iso') will automatically re-pack initrd.oar"

# ===== Userland C support (Phase 3 groundwork) =====
# Flat binaries for ring 3, using the same loader base (0x400000).
# Preserves the existing .asm hello_user.bin exactly.
# Build a C program with: make userland/hello_user_c.bin
# Then copy to initrd/ and rebuild oar to test with "run hello_user_c.bin" (or overwrite hello_user.bin for drop-in).

USER_CC      = gcc
USER_CFLAGS  = -ffreestanding -m64 -mcmodel=large -mno-red-zone -fno-pic -fno-pie \
               -nostdlib -nostdinc -Iuserland -Wall -Wextra -O2
USER_LD      = ld
USER_LDFLAGS = -T userland/user.ld -nostdlib

# .c -> .bin (via temp ELF then strip to raw binary)
userland/%.bin: userland/%.c userland/user.ld
	$(USER_CC) $(USER_CFLAGS) -c $< -o $(@:.bin=.tmp.o)
	$(USER_LD) $(USER_LDFLAGS) $(@:.bin=.tmp.o) -o $(@:.bin=.tmp.elf)
	objcopy -O binary $(@:.bin=.tmp.elf) $@
	rm -f $(@:.bin=.tmp.o) $(@:.bin=.tmp.elf)

# Convenience: build the C version of the demo
userland/hello_user_c.bin: userland/hello_user_c.c

# Copy rule (user can do manually or extend)
initrd/hello_user_c.bin: userland/hello_user_c.bin
	cp $< $@

.PHONY: user-c-programs
user-c-programs: userland/hello_user_c.bin
	@echo "C user program built: userland/hello_user_c.bin"
	@echo "To test: cp userland/hello_user_c.bin initrd/hello_user.bin && make run  (auto re-packs initrd.oar)"
	@echo "(overwrites the asm one for testing; original asm rule still available)"

.PHONY: build
build: iso
	@echo "Build complete (kernel + fresh initrd.oar + ISO). Use 'make run' to test."
	@echo "Workflow: make clean && make build && make run"
	@echo "(make build ensures initrd.oar is packed from current initrd/ contents and ISO is ready)"

