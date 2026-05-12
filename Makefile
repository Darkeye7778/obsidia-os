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
	initrd.o

all: iso

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

$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(KERNEL)

iso: $(LIMINE_DIR) $(KERNEL)
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

run: iso
	qemu-system-x86_64 -cdrom $(ISO) -serial stdio

clean:
	rm -f *.o $(KERNEL) $(ISO)
