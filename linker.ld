ENTRY(kmain)
PHDRS {
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}

KERNEL_VMA = 0xffffffff80000000;

SECTIONS {
    . = KERNEL_VMA;

    __kernel_start = .;
    .limine_requests : AT(ADDR(.limine_requests) - KERNEL_VMA) {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :text

    . = ALIGN(4096); __text_start = .;
    .text : AT(ADDR(.text) - KERNEL_VMA) {
        *(.text*)
    } :text
    . = ALIGN(4096); __text_end = .; __rodata_start = .;

    .rodata : AT(ADDR(.rodata) - KERNEL_VMA) {
        *(.rodata*)
    } :rodata
    . = ALIGN(4096); __rodata_end = .; __data_start = .;

    .data : AT(ADDR(.data) - KERNEL_VMA) {
        *(.data*)
    } :data

    .bss : AT(ADDR(.bss) - KERNEL_VMA) {
        *(COMMON)
        *(.bss*)
    } :data
    . = ALIGN(4096); __data_end = .; __kernel_end = .;
}
