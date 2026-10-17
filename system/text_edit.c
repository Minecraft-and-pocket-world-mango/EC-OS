#include "text_edit.h"
#include "window.h"
#include "gfx.h"
#include "fs.h"
#include "lib.h"
#include "mouse.h"
#include "desktop.h"

#define TE_W        720
#define TE_H        520
#define HX_W        760
#define HX_H        560
#define HX_MAX      (32 * 80)

#define PADX        16
#define PADY        16

#define DLG_W       720
#define DLG_H       260
#define DLG_BTN_W   150
#define DLG_BTN_H   52

#define COL_EDIT_BG RGB(0x0E,0x12,0x18)
#define COL_SAVE_BG RGB(0x2E,0x36,0x46)
#define COL_SAVE_HOV RGB(0x3A,0x46,0x5C)
#define COL_SAVE_PRS RGB(0x1E,0x24,0x30)
#define COL_BTN_BG  RGB(0x2E,0x36,0x46)
#define COL_BTN_HOV RGB(0x4C,0x8D,0xFF)
#define COL_BTN_PRS RGB(0x2F,0x6F,0xEB)

static int in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

static char hexdigit(int v)
{
    return (char)(v < 10 ? ('0' + v) : ('A' + (v - 10)));
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void draw_save_button(window_t *w)
{
    int bx = w->x + SAVE_BTN_X;
    int by = w->y + SAVE_BTN_Y;
    int hov = in_rect(mouse_x, mouse_y, bx, by, SAVE_BTN_W, SAVE_BTN_H);
    color_t bg = COL_SAVE_BG;

    if (hov && mouse_left)
        bg = COL_SAVE_PRS;
    else if (hov)
        bg = COL_SAVE_HOV;

    gfx_fill_round(bx, by, SAVE_BTN_W, SAVE_BTN_H, 8, bg);
    gfx_draw_string(bx + (SAVE_BTN_W - gfx_text_width("Save")) / 2,
                    by + (SAVE_BTN_H - FONT_H) / 2, "Save", COL_WHITE, GFX_NO_BG);
}

static int save_hit(int mx, int my)
{
    return mx >= SAVE_BTN_X && mx < SAVE_BTN_X + SAVE_BTN_W &&
           my >= SAVE_BTN_Y && my < SAVE_BTN_Y + SAVE_BTN_H;
}

static void ta_render(window_t *w, const char *buf, int len, color_t fg)
{
    int ex = w->x + PADX;
    int ey = w->y + TITLE_H + PADY;
    int ew = w->w - 2 * PADX;
    int eh = w->h - TITLE_H - 2 * PADY;
    int vis = eh / FONT_H;
    int total = 1, i, line, start, off, y;
    int sx, sy, sw, sh;
    int cl = 0, cc = 0;

    if (vis < 1)
        return;

    for (i = 0; i < len; i++)
        if (buf[i] == '\n')
            total++;
    start = (total > vis) ? (total - vis) : 0;

    for (i = 0; i < len; i++) {
        if (buf[i] == '\n') { cl++; cc = 0; }
        else cc++;
    }

    off = 0;
    for (line = 0; line < start; line++) {
        while (off < len && buf[off] != '\n') off++;
        if (off < len) off++;
    }

    gfx_get_clip(&sx, &sy, &sw, &sh);
    gfx_set_clip(ex, ey, ew, eh);

    y = ey;
    for (line = start; line < total; line++) {
        int e = off;
        while (e < len && buf[e] != '\n') e++;
        for (i = off; i < e; i++)
            gfx_draw_char(ex + (i - off) * FONT_W, y, buf[i], fg, GFX_NO_BG);
        if (line == cl) {
            int cx = ex + cc * FONT_W;
            if (cx < ex + ew)
                gfx_vline(cx, y, FONT_H, fg);
        }
        if (e < len) {
            off = e + 1;
            y += FONT_H;
        } else {
            break;
        }
    }

    gfx_set_clip(sx, sy, sw, sh);
}

/* ------------------------------------------------------------------ */
/*  text editor                                                       */
/* ------------------------------------------------------------------ */
static window_t *te_win = 0;
static char te_buf[NODE_DATA_SIZE];
static int  te_len = 0;
static int  te_idx = -1;

static void te_draw(window_t *w)
{
    gfx_fill_rect(w->x + 1, w->y + TITLE_H, w->w - 2, w->h - TITLE_H - 1,
                  COL_EDIT_BG);
    ta_render(w, te_buf, te_len, COL_TEXT);
}

static void te_key(window_t *w, int key)
{
    (void)w;
    if (key == 27) {
        window_close(te_win);
        desktop_redraw_all();
        return;
    }
    if (key == 8 || key == 127) {
        if (te_len > 0)
            te_len--;
    } else if (key == '\n' || key == '\r') {
        if (te_len < NODE_DATA_SIZE - 1)
            te_buf[te_len++] = '\n';
    } else if (key >= 32 && key < 127) {
        if (te_len < NODE_DATA_SIZE - 1)
            te_buf[te_len++] = (char)key;
    }
    te_buf[te_len] = '\0';
    desktop_redraw_all();
}

static int te_title_press(window_t *w, int mx, int my)
{
    fs_node_t *n;

    (void)w;
    if (!save_hit(mx, my))
        return 0;

    if (te_idx >= 0 && te_idx < MAX_NODES && fs_nodes[te_idx].type == FS_TYPE_FILE) {
        n = &fs_nodes[te_idx];
        memset(n->data, 0, NODE_DATA_SIZE);
        memcpy(n->data, te_buf, te_len);
        n->data[te_len] = '\0';
        fs_flush_to_disk();
        desktop_toast("Saved");
    }
    desktop_redraw_all();
    return 1;
}

window_t *text_edit_open(int fs_idx)
{
    fs_node_t *n;
    int len = 0;

    if (fs_idx < 0 || fs_idx >= MAX_NODES)
        return 0;
    n = &fs_nodes[fs_idx];
    if (n->type != FS_TYPE_FILE)
        return 0;

    while (len < NODE_DATA_SIZE - 1 && n->data[len])
        len++;
    memcpy(te_buf, n->data, len);
    te_buf[len] = '\0';
    te_len = len;
    te_idx = fs_idx;

    if (te_win && te_win->visible && te_win->id == 4) {
        strncpy(te_win->title, n->name, sizeof(te_win->title) - 1);
        te_win->title[sizeof(te_win->title) - 1] = '\0';
        desktop_redraw_all();
        return te_win;
    }

    te_win = window_create((SCREEN_W - TE_W) / 2, (SCREEN_H - TE_H) / 2,
                           TE_W, TE_H, n->name, COL_EDIT_BG,
                           te_draw, 0, 4);
    if (te_win) {
        te_win->on_key = te_key;
        te_win->on_title = draw_save_button;
        te_win->on_title_press = te_title_press;
    }
    return te_win;
}

window_t *text_edit_open_new(int parent, const char *name)
{
    int idx = fs_create(parent, name, FS_TYPE_FILE);
    if (idx < 0)
        return 0;
    return text_edit_open(idx);
}

int text_edit_is_open(void)
{
    return te_win && te_win->visible && te_win->id == 4;
}

/* ------------------------------------------------------------------ */
/*  hex editor                                                        */
/* ------------------------------------------------------------------ */
static window_t *hx_win = 0;
static char hx_text[HX_MAX];
static int  hx_len = 0;
static int  hx_idx = -1;

static int raw_len(const char *data)
{
    int i;
    for (i = NODE_DATA_SIZE - 1; i >= 0; i--)
        if (data[i] != 0)
            return i + 1;
    return 0;
}

static void hx_build(const char *raw, int rawlen)
{
    int off, i, p = 0;

    for (off = 0; off < rawlen; off += 16) {
        unsigned int o = (unsigned int)off;

        for (i = 0; i < 8; i++)
            hx_text[p++] = hexdigit((int)((o >> ((7 - i) * 4)) & 0xF));
        hx_text[p++] = ' ';
        hx_text[p++] = ' ';

        for (i = 0; i < 16; i++) {
            if (off + i < rawlen) {
                unsigned char c = (unsigned char)raw[off + i];
                hx_text[p++] = hexdigit((c >> 4) & 0xF);
                hx_text[p++] = hexdigit(c & 0xF);
            } else {
                hx_text[p++] = ' ';
                hx_text[p++] = ' ';
            }
            if (i < 15) {
                hx_text[p++] = ' ';
                if (i == 7)
                    hx_text[p++] = ' ';
            }
        }

        hx_text[p++] = ' ';
        hx_text[p++] = ' ';
        hx_text[p++] = '|';
        for (i = 0; i < 16; i++) {
            if (off + i < rawlen) {
                unsigned char c = (unsigned char)raw[off + i];
                hx_text[p++] = (c >= 0x20 && c < 0x7F) ? (char)c : '.';
            } else {
                hx_text[p++] = ' ';
            }
        }
        hx_text[p++] = '|';
        hx_text[p++] = '\n';

        if (p > HX_MAX - 64)
            break;
    }
    hx_text[p] = '\0';
    hx_len = p;
}

static int hx_parse(char *dst, int maxlen)
{
    int i = 0, count = 0;

    while (i < hx_len) {
        int e = i;
        int k, bytes = 0;

        while (e < hx_len && hx_text[e] != '\n')
            e++;

        if (e > i) {
            if (e - i < 8)
                return -1;
            k = i + 8;
            while (k < e && bytes < 16) {
                int hi, lo;

                while (k < e && hx_text[k] == ' ')
                    k++;
                if (k >= e)
                    break;
                if (hx_text[k] == '|')
                    break;

                hi = hexval(hx_text[k]);
                if (hi < 0)
                    return -1;
                k++;
                if (k >= e)
                    return -1;
                lo = hexval(hx_text[k]);
                if (lo < 0)
                    return -1;
                k++;

                if (count >= maxlen)
                    return -1;
                dst[count++] = (char)((hi << 4) | lo);
                bytes++;
            }
        }

        i = (e < hx_len) ? e + 1 : e;
    }
    return count;
}

static void hx_draw(window_t *w)
{
    gfx_fill_rect(w->x + 1, w->y + TITLE_H, w->w - 2, w->h - TITLE_H - 1,
                  COL_EDIT_BG);
    ta_render(w, hx_text, hx_len, COL_ACCENT2);
}

static void hx_key(window_t *w, int key)
{
    (void)w;
    if (key == 27) {
        window_close(hx_win);
        desktop_redraw_all();
        return;
    }
    if (key == 8 || key == 127) {
        if (hx_len > 0)
            hx_len--;
    } else if (key == '\n' || key == '\r') {
        if (hx_len < HX_MAX - 1)
            hx_text[hx_len++] = '\n';
    } else if (key >= 32 && key < 127) {
        if (hx_len < HX_MAX - 1)
            hx_text[hx_len++] = (char)key;
    }
    hx_text[hx_len] = '\0';
    desktop_redraw_all();
}

static int hx_title_press(window_t *w, int mx, int my)
{
    char raw[NODE_DATA_SIZE];
    int got;

    (void)w;
    if (!save_hit(mx, my))
        return 0;

    if (hx_idx < 0 || hx_idx >= MAX_NODES || fs_nodes[hx_idx].type != FS_TYPE_FILE)
        return 1;

    got = hx_parse(raw, NODE_DATA_SIZE);
    if (got < 0) {
        desktop_toast("Invalid hex format");
        desktop_redraw_all();
        return 1;
    }

    memset(fs_nodes[hx_idx].data, 0, NODE_DATA_SIZE);
    memcpy(fs_nodes[hx_idx].data, raw, got);
    if (got < NODE_DATA_SIZE)
        fs_nodes[hx_idx].data[got] = '\0';

    fs_flush_to_disk();
    desktop_toast("Saved");
    desktop_redraw_all();
    return 1;
}

static void hex_edit_open(int fs_idx)
{
    fs_node_t *n;
    int len;

    if (fs_idx < 0 || fs_idx >= MAX_NODES)
        return;
    n = &fs_nodes[fs_idx];
    if (n->type != FS_TYPE_FILE)
        return;

    len = raw_len(n->data);
    hx_build(n->data, len);
    hx_idx = fs_idx;

    if (hx_win && hx_win->visible && hx_win->id == 5) {
        strncpy(hx_win->title, n->name, sizeof(hx_win->title) - 1);
        hx_win->title[sizeof(hx_win->title) - 1] = '\0';
        desktop_redraw_all();
        return;
    }

    hx_win = window_create((SCREEN_W - HX_W) / 2, (SCREEN_H - HX_H) / 2,
                           HX_W, HX_H, n->name, COL_EDIT_BG,
                           hx_draw, 0, 5);
    if (hx_win) {
        hx_win->on_key = hx_key;
        hx_win->on_title = draw_save_button;
        hx_win->on_title_press = hx_title_press;
    }
}

/* ------------------------------------------------------------------ */
/*  warning dialog                                                    */
/* ------------------------------------------------------------------ */
static window_t *dlg_win = 0;
static int dlg_target = -1;

static const char *DLG_MSG =
    "This file format is highly risky. If you modify it, you will bear "
    "the consequences. Do you wish to open it?";

static void draw_wrapped(int x, int y, int maxw, const char *s, color_t col)
{
    int cx = x, cy = y;

    while (*s) {
        char word[40];
        int wl = 0, ww;

        while (*s == ' ')
            s++;
        while (*s && *s != ' ' && wl < 39)
            word[wl++] = *s++;
        word[wl] = '\0';
        if (wl == 0)
            break;

        ww = gfx_text_width(word);
        if (cx != x && cx + ww > x + maxw) {
            cx = x;
            cy += FONT_H;
        }
        gfx_draw_string(cx, cy, word, col, GFX_NO_BG);
        cx += ww + FONT_W;
    }
}

static void dlg_buttons(window_t *w, int *yx, int *nx, int *by)
{
    *yx = w->x + w->w / 2 - DLG_BTN_W - 20;
    *nx = w->x + w->w / 2 + 20;
    *by = w->y + w->h - DLG_BTN_H - 24;
}

static void dlg_draw(window_t *w)
{
    int yx, nx, by;
    int i;
    struct { int x; const char *label; } btns[2];

    draw_wrapped(w->x + 40, w->y + TITLE_H + 30, w->w - 80, DLG_MSG, COL_TEXT);

    dlg_buttons(w, &yx, &nx, &by);
    btns[0].x = yx; btns[0].label = "Yes";
    btns[1].x = nx; btns[1].label = "No";

    for (i = 0; i < 2; i++) {
        int hov = in_rect(mouse_x, mouse_y, btns[i].x, by, DLG_BTN_W, DLG_BTN_H);
        color_t bg = COL_BTN_BG;

        if (hov && mouse_left)
            bg = COL_BTN_PRS;
        else if (hov)
            bg = COL_BTN_HOV;

        gfx_fill_round(btns[i].x, by, DLG_BTN_W, DLG_BTN_H, 10, bg);
        gfx_draw_string(btns[i].x + (DLG_BTN_W - gfx_text_width(btns[i].label)) / 2,
                        by + (DLG_BTN_H - FONT_H) / 2,
                        btns[i].label, COL_WHITE, GFX_NO_BG);
    }
}

static void dlg_click(window_t *w, int mx, int my)
{
    int yx, nx, by;
    int ax = mx + w->x;
    int ay = my + w->y + TITLE_H;

    dlg_buttons(w, &yx, &nx, &by);

    if (in_rect(ax, ay, yx, by, DLG_BTN_W, DLG_BTN_H)) {
        int t = dlg_target;
        window_close(dlg_win);
        hex_edit_open(t);
        return;
    }
    if (in_rect(ax, ay, nx, by, DLG_BTN_W, DLG_BTN_H)) {
        window_close(dlg_win);
        desktop_redraw_all();
    }
}

void warn_dialog_open(int fs_idx)
{
    dlg_target = fs_idx;

    if (dlg_win && dlg_win->visible && dlg_win->id == 6) {
        desktop_redraw_all();
        return;
    }

    dlg_win = window_create((SCREEN_W - DLG_W) / 2, (SCREEN_H - DLG_H) / 2,
                            DLG_W, DLG_H, "Warning", COL_EDIT_BG,
                            dlg_draw, dlg_click, 6);
}
