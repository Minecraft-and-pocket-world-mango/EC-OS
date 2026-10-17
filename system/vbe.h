#ifndef EC_VBE_H
#define EC_VBE_H

#include <stdint.h>

#define VBE_PARAMS_ADDR   0x5000u
#define VBE_MAGIC         0x31564345u   /* "ECV1" in memory (little-endian) */

/* Written by boot/boot.asm, read once at kernel start. */
typedef struct {
    uint32_t magic;        /* "ECV1"                                  */
    uint32_t framebuffer;  /* linear framebuffer physical address     */
    uint32_t pitch;        /* bytes per scan line                     */
    uint32_t width;        /* SCREEN_W (1280 or 1920)                 */
    uint32_t height;       /* SCREEN_H (1024 or 1080)                 */
    uint32_t bpp;          /* 32                                      */
    uint16_t low_mem_kb;   /* int 0x12  : conventional KB             */
    uint16_t ext_mem_kb;   /* int 0x15  : KB between 1M and 16M       */
    uint16_t ext_mem_64k;  /* int 0x15  : 64K blocks above 16M        */
    uint16_t reserved;
} vbe_params_t;

extern volatile vbe_params_t *const vbe;

int      vbe_init(void);
void     vbe_panic(void);   /* red screen + halt, never returns        */
uint32_t vbe_framebuffer(void);
uint32_t vbe_pitch(void);
uint32_t vbe_width(void);
uint32_t vbe_height(void);
uint32_t vbe_bpp(void);
uint32_t vbe_detected_ram_mb(void);   /* real detected RAM, integer MB */

#endif
