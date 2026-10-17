#include "rename_dialog.h"
#include "fs.h"
#include "gfx.h"
#include "lib.h"
#include "mouse.h"
#include "desktop.h"

#define RD_W         560
#define RD_H         260
#define RD_TITLE_H   52
#define RD_FIELD_H   52
#define RD_BTN_W     150
#define RD_BTN_H     48
#define RD_BTN_GAP   32

#define RD_PANEL_TOP RGB(0x2A,0x30,0x3C)
#define RD_PANEL_BOT RGB(0x1C,0x21,0x2A)
#define RD_FIELD_BG  RGB(0x0E,0x12,0x18)
#define RD_BTN_BG    RGB(0x2E,0x36,0x46)
#define RD_BTN_HOV   RGB(0x4C,0x8D,0xFF)
#define RD_BTN_PRS   RGB(0x2F,0x6F,0xEB)

static int  rd_open = 0;
static int  rd_idx = -1;
static char rd_buf[MAX_NAME];
static int  rd_len = 0;

static int in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

static void rd_panel(int *x, int *y)
{
    *x = (SCREEN_W - RD_W) / 2;
    *y = (SCREEN_H - RD_H) / 2;
}

static void rd_field_rect(int px, int py, int *x, int *y, int *w, int *h)
{
    *x = px + 32;
    *y = py + RD_TITLE_H + 28;
    *w = RD_W - 64;
    *h = RD_FIELD_H;
}

static void rd_buttons(int px, int py, int *okx, int *cx, int *by)
{
    int total = RD_BTN_W * 2 + RD_BTN_GAP;
    *okx = px + (RD_W - total) / 2;
    *cx = *okx + RD_BTN_W + RD_BTN_GAP;
    *by = py + RD_H - RD_BTN_H - 28;
}

static void rd_button(int x, int y, const char *label)
{
    int hov = in_rect(mouse_x, mouse_y, x, y, RD_BTN_W, RD_BTN_H);
    color_t bg = RD_BTN_BG;

    if (hov && mouse_left)
        bg = RD_BTN_PRS;
    else if (hov)
        bg = RD_BTN_HOV;

    gfx_fill_round(x, y, RD_BTN_W, RD_BTN_H, 10, bg);
    gfx_outline_round(x, y, RD_BTN_W, RD_BTN_H, 10, COL_BORDER_IN);
    gfx_draw_string(x + (RD_BTN_W - gfx_text_width(label)) / 2,
                    y + (RD_BTN_H - FONT_H) / 2, label, COL_WHITE, GFX_NO_BG);
}

void rename_dialog_draw(void)
{
    int px, py;
    int fx, fy, fw, fh;
    int okx, cx, by;
    int i;

    if (!rd_open)
        return;

    rd_panel(&px, &py);

    gfx_fill_round(px + 8, py + 8, RD_W, RD_H, 12, COL_SHADOW);
    gfx_fill_round_grad(px, py, RD_W, RD_H, 12, RD_PANEL_TOP, RD_PANEL_BOT);
    gfx_outline_round(px, py, RD_W, RD_H, 12, COL_BORDER);
    gfx_outline_round(px + 1, py + 1, RD_W - 2, RD_H - 2, 11, COL_BORDER_IN);

    gfx_draw_string(px + 28, py + (RD_TITLE_H - FONT_H) / 2,
                    "Rename", COL_TEXT, GFX_NO_BG);

    rd_field_rect(px, py, &fx, &fy, &fw, &fh);
    gfx_fill_round(fx, fy, fw, fh, 8, RD_FIELD_BG);
    gfx_outline_round(fx, fy, fw, fh, 8, COL_BORDER_IN);

    {
        int tx = fx + 14;
        int ty = fy + (fh - FONT_H) / 2;

        for (i = 0; i < rd_len; i++) {
            int cxchar = tx + i * FONT_W;
            if (cxchar + FONT_W > fx + fw)
                break;
            gfx_draw_char(cxchar, ty, rd_buf[i], COL_TEXT, RD_FIELD_BG);
        }
        if (tx + rd_len * FONT_W + 2 < fx + fw)
            gfx_vline(tx + rd_len * FONT_W, ty, FONT_H, COL_ACCENT2);
    }

    rd_buttons(px, py, &okx, &cx, &by);
    rd_button(okx, by, "OK");
    rd_button(cx, by, "Cancel");
}

static void rd_close(void)
{
    rd_open = 0;
    desktop_redraw_all();
}

static void rd_confirm(void)
{
    if (rd_idx < 0 || rd_idx >= MAX_NODES) {
        rd_close();
        return;
    }

    if (rd_len <= 0) {
        desktop_toast("Rename failed");
        return;
    }
    if (fs_rename(rd_idx, rd_buf) < 0) {
        desktop_toast("Rename failed");
        return;
    }

    rd_open = 0;
    desktop_refresh_icons();
}

void rename_dialog_open(int fs_idx)
{
    const char *nm;
    int k = 0;

    if (fs_idx < 0 || fs_idx >= MAX_NODES)
        return;
    if (fs_nodes[fs_idx].type == FS_TYPE_FREE)
        return;

    nm = fs_nodes[fs_idx].name;
    while (nm[k] && k < MAX_NAME - 1) {
        rd_buf[k] = nm[k];
        k++;
    }
    rd_buf[k] = '\0';
    rd_len = k;
    rd_idx = fs_idx;
    rd_open = 1;

    desktop_redraw_all();
}

int rename_dialog_is_open(void)
{
    return rd_open;
}

void rename_dialog_key(int key)
{
    if (!rd_open)
        return;

    if (key == 27) {                 /* ESC: cancel */
        rd_close();
        return;
    }
    if (key == '\n' || key == '\r') { /* Enter: confirm */
        rd_confirm();
        return;
    }
    if (key == 8 || key == 127) {     /* Backspace */
        if (rd_len > 0)
            rd_len--;
    } else if (key >= 32 && key < 127) {
        if (rd_len < MAX_NAME - 1)
            rd_buf[rd_len++] = (char)key;
    }
    rd_buf[rd_len] = '\0';
    desktop_redraw_all();
}

void rename_dialog_click(int mx, int my)
{
    int px, py;
    int okx, cx, by;

    if (!rd_open)
        return;

    rd_panel(&px, &py);
    rd_buttons(px, py, &okx, &cx, &by);

    if (in_rect(mx, my, okx, by, RD_BTN_W, RD_BTN_H))
        rd_confirm();
    else if (in_rect(mx, my, cx, by, RD_BTN_W, RD_BTN_H))
        rd_close();
    /* clicks elsewhere are ignored (modal) */
}
