ENTRY(kmain)

KERNEL_VMA = 0xffffffff80000000;

SECTIONS {
    . = KERNEL_VMA;

    .text : AT(ADDR(.text) - KERNEL_VMA) {
        *(.text*)
    }

    .rodata : AT(ADDR(.rodata) - KERNEL_VMA) {
        *(.rodata*)
    }

    .data : AT(ADDR(.data) - KERNEL_VMA) {
        *(.data*)
    }

    .bss : AT(ADDR(.bss) - KERNEL_VMA) {
        *(COMMON)
        *(.bss*)
    }
}
