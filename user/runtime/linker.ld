/* userland/user.ld
 * Linker script for Obsidia userland flat binaries (ring 3).
 * Produces raw binary (no ELF headers) suitable for the simple loader at 0x400000.
 * Usage: ld -T userland/user.ld -o program.elf ... ; objcopy -O binary program.elf program.bin
 */

ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5); /* read + execute */
    data PT_LOAD FLAGS(6); /* read + write, NX */
    stack PT_GNU_STACK FLAGS(6);
}

SECTIONS
{
    . = 0x400000;

    .text : ALIGN(16) {
        *(.text .text.*)
    } :text

    .rodata : ALIGN(16) {
        *(.rodata .rodata.*)
    } :text

    . = ALIGN(0x1000);

    .data : ALIGN(16) {
        *(.data .data.*)
    } :data

    .bss : ALIGN(16) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    } :data

    /DISCARD/ : {
        *(.note.*)
        *(.comment)
        *(.eh_frame*)
        *(.gcc_except_table*)
    }
}
