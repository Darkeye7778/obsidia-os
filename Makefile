CC = gcc
LD = ld

CFLAGS = -ffreestanding -m64 -mcmodel=kernel -mno-red-zone -fno-pic -Ikernel
LDFLAGS = -nostdlib -z max-page-size=0x1000 -T linker.ld

KERNEL = kernel.elf
ISO = obsidia.iso

OBJS = \
	main.o \
	framebuffer.o \
	font.o \
        console.o \
        keyboard.o

all: iso

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

$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(KERNEL)

iso: $(KERNEL)
	cp $(KERNEL) iso/boot/kernel.elf
	xorriso -as mkisofs \
		-b boot/limine/limine-bios-cd.bin \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image \
		-o $(ISO) \
		iso
	~/limine/limine.exe bios-install $(ISO)

run: iso
	qemu-system-x86_64 -cdrom $(ISO) -serial stdio

clean:
	rm -f *.o $(KERNEL) $(ISO)
