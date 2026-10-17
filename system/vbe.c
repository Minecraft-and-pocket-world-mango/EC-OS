#include "vbe.h"
#include "gfx.h"

volatile vbe_params_t *const vbe = (volatile vbe_params_t *)VBE_PARAMS_ADDR;

/* True only when the boot sector wrote a sane hand-off block that exactly
 * matches the compile-time geometry.  A mismatch means the VM gave us a
 * different mode than the kernel was built for -- never draw into it. */
int vbe_init(void)
{
    if (vbe->magic != VBE_MAGIC)
        return 0;
    if (vbe->framebuffer == 0)
        return 0;
    if (vbe->width != SCREEN_W || vbe->height != SCREEN_H)
        return 0;
    if (vbe->bpp != 32)
        return 0;
    if (vbe->pitch < (uint32_t)SCREEN_W * 4u)
        return 0;
    if (vbe->pitch > 65536u)
        return 0;
    return 1;
}

void vbe_panic(void)
{
    volatile uint32_t *fb;
    uint32_t pitch = vbe->pitch;
    uint32_t rows  = vbe->height;
    uint32_t row;

    /* Use the framebuffer geometry we actually have, clamped hard so a
     * corrupted hand-off block can never scribble past the scan lines. */
    if (pitch == 0)
        pitch = (uint32_t)SCREEN_W * 4u;
    if (pitch > 65536u)
        pitch = 65536u;
    if (rows == 0 || rows > 4096u)
        rows = 1024u;

    {
        uint32_t cap = (rows * pitch) > 16u * 1024u * 1024u
                       ? 16u * 1024u * 1024u / pitch : rows;
        if (cap == 0) cap = 1;
        if (rows > cap) rows = cap;
    }

    fb = (volatile uint32_t *)vbe->framebuffer;
    if (fb == 0)
        fb = (volatile uint32_t *)0xFD000000u;   /* hope for an LFB there  */

    for (row = 0; row < rows; row++) {
        uint32_t *line = (uint32_t *)((uint8_t *)fb + row * pitch);
        uint32_t i, n = pitch / 4u;

        for (i = 0; i < n; i++)
            line[i] = RGB(0xAA, 0x00, 0x00);
    }

    __asm__ volatile("cli; hlt" ::: "memory");
    for (;;)
        __asm__ volatile("hlt");
}

uint32_t vbe_framebuffer(void) { return vbe->framebuffer; }
uint32_t vbe_pitch(void)       { return vbe->pitch; }
uint32_t vbe_width(void)       { return vbe->width; }
uint32_t vbe_height(void)      { return vbe->height; }
uint32_t vbe_bpp(void)         { return vbe->bpp; }

uint32_t vbe_detected_ram_mb(void)
{
    uint32_t kb = (uint32_t)vbe->low_mem_kb
                + (uint32_t)vbe->ext_mem_kb
                + (uint32_t)vbe->ext_mem_64k * 64u;
    return kb / 1024u;
}