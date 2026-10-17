ENTRY(kernel_entry)

SECTIONS
{
    . = 0x8000;

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    /* Place BSS above the legacy VGA/BIOS memory hole (0xA0000-0xFFFFF).
     * The image is loaded low (0x8000), and the ~5 MiB software back buffer
     * lives in BSS; if BSS started right after .data it would span the hole,
     * and clearing it (rep stosb) would write into VGA/ROM and triple-fault. */
    . = 0x100000;

    __bss_start = .;
    .bss (NOLOAD) : {
        *(.bss*)
        *(COMMON)
    }
    __bss_end = .;
}
