#ifndef EC_GFX_H
#define EC_GFX_H

#include <stdint.h>

/* ------------------------------------------------------------------ *
 *  Resolution.
 *
 *  Default target: 1280x1024x32 -- a native VBE mode of VMware
 *  Workstation (and most VGA BIOSes).  Define RES_HIGH=1 (build switch)
 *  to restore the 1920x1080x32 build.  compile-time only: the back
 *  buffer stride and boot sector must agree with SCREEN_W/SCREEN_H,
 *  so the resolution can not change at runtime.
 * ------------------------------------------------------------------ */
#ifndef RES_HIGH
#define RES_HIGH   0
#endif

#if RES_HIGH
#define SCREEN_W   1920
#define SCREEN_H   1080
#else
#define SCREEN_W   1280
#define SCREEN_H   1024
#endif

#define FONT_W     16
#define FONT_H     32

typedef uint32_t color_t;                 /* 0x00RRGGBB */

#define RGB(r,g,b)  ( ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b) )
#define GFX_NO_BG   0xFFFFFFFFu            /* transparent background sentinel */

/* ------------------------------------------------------------------ *
 *  Modern dark palette  (macOS Big Sur / Windows 11 inspired)
 * ------------------------------------------------------------------ */
#define COL_BG_TOP      RGB(0x1A,0x1D,0x25)   /* desktop gradient top     */
#define COL_BG_BOT      RGB(0x08,0x0A,0x0F)   /* desktop gradient bottom  */

#define COL_PANEL_TOP   RGB(0x26,0x2B,0x37)   /* window body gradient     */
#define COL_PANEL_BOT   RGB(0x16,0x1A,0x22)

#define COL_TITLE_A     RGB(0x38,0x40,0x52)   /* active title gradient    */
#define COL_TITLE_B     RGB(0x23,0x28,0x34)
#define COL_TITLE_IA    RGB(0x26,0x2A,0x33)   /* inactive title gradient  */
#define COL_TITLE_IB    RGB(0x1A,0x1E,0x26)

#define COL_ACCENT      RGB(0x4C,0x8D,0xFF)   /* highlight / selection    */
#define COL_ACCENT2     RGB(0x7A,0xB0,0xFF)

#define COL_TEXT        RGB(0xE8,0xEB,0xF1)
#define COL_TEXT_DIM    RGB(0x97,0xA0,0xB0)

#define COL_BORDER      RGB(0x06,0x07,0x0B)   /* outer (dark)             */
#define COL_BORDER_IN   RGB(0x4A,0x54,0x68)   /* inner (light)            */
#define COL_SHADOW      RGB(0x03,0x04,0x06)

#define COL_CLOSE       RGB(0xFF,0x5F,0x57)
#define COL_MIN         RGB(0xFE,0xBC,0x2E)
#define COL_MAX         RGB(0x28,0xC8,0x40)

#define COL_WHITE       RGB(0xFF,0xFF,0xFF)
#define COL_BLACK       RGB(0x00,0x00,0x00)

#define COL_ICON_DIR    RGB(0x5B,0x8D,0xFF)
#define COL_ICON_DIR2   RGB(0x9C,0xC0,0xFF)
#define COL_ICON_FILE   RGB(0xD7,0xDD,0xE8)
#define COL_ICON_FILE2  RGB(0xA6,0xB0,0xC0)
#define COL_ICON_APP    RGB(0x32,0xD0,0x8A)
#define COL_ICON_APP2   RGB(0x8C,0xF0,0xC0)
#define COL_ICON_DOS    RGB(0x30,0x38,0x48)
#define COL_ICON_DOS2   RGB(0x12,0x16,0x1E)

#define COL_MENU_TOP    RGB(0x2A,0x30,0x3C)
#define COL_MENU_BOT    RGB(0x1C,0x21,0x2A)
#define COL_HOVER_TOP   RGB(0x4C,0x8D,0xFF)
#define COL_HOVER_BOT   RGB(0x2F,0x6F,0xEB)

/* ---- lifecycle ---- */
void     gfx_init(void);
void     gfx_clear(color_t c);
color_t *gfx_backbuffer(void);
void     gfx_flip(void);

/* ---- dirty region tracking ---- */
void     gfx_mark_dirty(int x, int y, int w, int h);
void     gfx_mark_all_dirty(void);

/* ---- clipping ---- */
void     gfx_set_clip(int x, int y, int w, int h);
void     gfx_get_clip(int *x, int *y, int *w, int *h);
void     gfx_reset_clip(void);

/* ---- primitives ---- */
void     gfx_pixel(int x, int y, color_t c);
void     gfx_hline(int x, int y, int w, color_t c);
void     gfx_vline(int x, int y, int h, color_t c);
void     gfx_fill_rect(int x, int y, int w, int h, color_t c);
void     gfx_rect(int x, int y, int w, int h, color_t c);
void     gfx_line(int x0, int y0, int x1, int y1, color_t c);
void     gfx_blend(int x, int y, color_t c, int alpha);   /* alpha 0..255 */

/* ---- shapes ---- */
void     gfx_fill_circle(int cx, int cy, int r, color_t c);
void     gfx_fill_round(int x, int y, int w, int h, int r, color_t c);
void     gfx_outline_round(int x, int y, int w, int h, int r, color_t c);
void     gfx_fill_vgrad(int x, int y, int w, int h, color_t top, color_t bot);
void     gfx_fill_round_grad(int x, int y, int w, int h, int r,
                             color_t top, color_t bot);
void     gfx_fill_round_top_grad(int x, int y, int w, int h, int r,
                                 color_t top, color_t bot);

/* ---- text (16x32 bitmap font) ---- */
void     gfx_draw_char(int x, int y, char ch, color_t fg, color_t bg);
void     gfx_draw_string(int x, int y, const char *s, color_t fg, color_t bg);
int      gfx_text_width(const char *s);

#endif
