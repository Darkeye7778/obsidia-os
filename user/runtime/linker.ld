/* userland/user.ld
 * Linker script for Obsidia userland flat binaries (ring 3).
 * Produces raw binary (no ELF headers) suitable for the simple loader at 0x400000.
 * Usage: ld -T userland/user.ld -o program.elf ... ; objcopy -O binary program.elf program.bin
 */

ENTRY(_start)

SECTIONS
{
    . = 0x400000;

    .text : ALIGN(16) {
        *(.text .text.*)
    }

    .rodata : ALIGN(16) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(16) {
        *(.data .data.*)
    }

    .bss : ALIGN(16) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /DISCARD/ : {
        *(.note.*)
        *(.comment)
        *(.eh_frame*)
        *(.gcc_except_table*)
    }
}
