#include "gfx.h"
#include "vga.h"
#include "keyboard.h"
#include "fs.h"
#include "shell.h"
#include "vbe.h"
#include "lib.h"
#include "idt.h"

void kernel_main(void)
{
    char buf[16];

    diag_mark('0');
    idt_init();          /* install IDT + COM1 debug now, so early faults
                          * are reported instead of triple-faulting      */
    diag_mark('V');
    if (!vbe_init()) {
        /* Hand-off block missing or does not match SCREEN_W/SCREEN_H.
         * The VM gave us a mode this kernel was not built for -- stop
         * before touching the framebuffer. */
        diag_mark('X');
        vbe_panic();
    }

    diag_mark('G');
    gfx_init();          /* 32bpp linear framebuffer + back buffer    */
    diag_mark('K');
    keyboard_flush();
    diag_mark('F');
    fs_init();
    diag_mark('D');
    fs_load_from_disk();     /* 若存在 ATA 硬盘，从盘上恢复文件系统 */

    diag_mark('C');
    vga_clear();

    vga_print("EC-OS 26Y10B  (c) EasyCloud Studio\n");
    vga_print("Booting...\n\n");

    vga_print("Display : ");
    itoa_u32(SCREEN_W, buf); vga_print(buf);
    vga_print(" x ");
    itoa_u32(SCREEN_H, buf); vga_print(buf);
    vga_print(" x 32bpp\n");

    vga_print("RAM     : ");
    itoa_u32(1024, buf); vga_print(buf); vga_print(" MB\n");

    vga_print("Disk    : ");
    itoa_u32((uint32_t)fs_capacity_mb(), buf); vga_print(buf); vga_print(" MB\n");

    vga_putchar('\n');

    diag_mark('S');
    shell_run();

    for (;;)
        ;
}