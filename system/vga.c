#include "vga.h"
#include "gfx.h"
#include "lib.h"

#define CON_COLS   (SCREEN_W / FONT_W)     /* 80  */
#define CON_ROWS   (SCREEN_H / FONT_H)     /* 32  */

#define CON_FG     COL_TEXT
#define CON_BG     RGB(0x0A,0x0C,0x11)

static int con_row = 0;
static int con_col = 0;

static void con_fill_bottom(void)
{
    color_t *bb = gfx_backbuffer();
    int y;

    for (y = (CON_ROWS - 1) * FONT_H; y < SCREEN_H; y++)
        memset(&bb[y * SCREEN_W], 0, sizeof(color_t) * SCREEN_W);
}

static void con_scroll(void)
{
    color_t *bb = gfx_backbuffer();
    size_t rowsz = (size_t)SCREEN_W * sizeof(color_t);
    size_t move;

    if (con_row < CON_ROWS)
        return;

    move = (size_t)(SCREEN_H - FONT_H) * rowsz;
    memmove(bb, bb + FONT_H * SCREEN_W, move);
    con_fill_bottom();

    con_row = CON_ROWS - 1;
    gfx_mark_all_dirty();
}

void vga_clear(void)
{
    con_row = 0;
    con_col = 0;
    gfx_fill_rect(0, 0, SCREEN_W, SCREEN_H, CON_BG);
    gfx_flip();
}

void vga_putchar(char c)
{
    if (c == '\n') {
        con_col = 0;
        con_row++;
        con_scroll();
        gfx_flip();
        return;
    }
    if (c == '\r') {
        con_col = 0;
        return;
    }
    if (c == '\b') {
        vga_backspace();
        return;
    }

    gfx_draw_char(con_col * FONT_W, con_row * FONT_H, c, CON_FG, CON_BG);
    con_col++;
    if (con_col >= CON_COLS) {
        con_col = 0;
        con_row++;
        con_scroll();
    }
    gfx_flip();
}

void vga_print(const char *s)
{
    while (*s)
        vga_putchar(*s++);
}

void vga_backspace(void)
{
    if (con_col <= 0)
        return;
    con_col--;
    gfx_draw_char(con_col * FONT_W, con_row * FONT_H, ' ', CON_FG, CON_BG);
    gfx_flip();
}
