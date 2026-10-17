#include "window.h"
#include "gfx.h"
#include "lib.h"
#include "desktop.h"

window_t windows[MAX_WINDOWS];

window_t *window_create(int x, int y, int w, int h, const char *title,
                        color_t content,
                        void (*on_draw)(window_t *),
                        void (*on_click)(window_t *, int, int),
                        int id)
{
    int i;
    window_t *win = 0;

    for (i = 0; i < MAX_WINDOWS; i++) {
        if (!windows[i].visible) {
            win = &windows[i];
            break;
        }
    }
    if (!win)
        return 0;

    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->visible = 1;
    win->active = 1;
    win->content = content;
    win->on_draw = on_draw;
    win->on_click = on_click;
    win->on_key = 0;
    win->on_title = 0;
    win->on_title_press = 0;
    win->on_right_click = 0;
    win->id = id;
    strncpy(win->title, title, sizeof(win->title) - 1);
    win->title[sizeof(win->title) - 1] = '\0';

    /* 立即重绘桌面，让新窗口无需拖动就显示出来 */
    desktop_redraw_all();

    return win;
}

void window_rect_with_shadow(window_t *w, int *rx, int *ry, int *rw, int *rh)
{
    if (rx) *rx = w->x;
    if (ry) *ry = w->y;
    if (rw) *rw = w->w + WIN_SHADOW;
    if (rh) *rh = w->h + WIN_SHADOW;
}

static void draw_lights(window_t *w)
{
    int cy = w->y + TITLE_H / 2;
    int r = 8;
    color_t dim = COL_TEXT_DIM;
    color_t cc = w->active ? COL_CLOSE : dim;
    color_t mc = w->active ? COL_MIN : dim;
    color_t xc = w->active ? COL_MAX : dim;

    gfx_fill_circle(w->x + 22, cy, r, cc);
    gfx_fill_circle(w->x + 44, cy, r, mc);
    gfx_fill_circle(w->x + 66, cy, r, xc);
}

/* draw a title truncated to maxw pixels, appending "..." if it does not fit */
static void draw_title_trunc(int x, int y, int maxw, const char *s)
{
    char buf[64];
    char tmp[68];
    int len = 0, k, i;

    if (maxw <= 0)
        return;

    while (s[len] && len < (int)sizeof(buf) - 1) {
        buf[len] = s[len];
        len++;
    }
    buf[len] = '\0';

    if (gfx_text_width(buf) <= maxw) {
        gfx_draw_string(x, y, buf, COL_TEXT, GFX_NO_BG);
        return;
    }

    for (k = len; k > 0; k--) {
        for (i = 0; i < k; i++)
            tmp[i] = buf[i];
        tmp[k] = '.';
        tmp[k + 1] = '.';
        tmp[k + 2] = '.';
        tmp[k + 3] = '\0';
        if (gfx_text_width(tmp) <= maxw) {
            gfx_draw_string(x, y, tmp, COL_TEXT, GFX_NO_BG);
            return;
        }
    }

    gfx_draw_string(x, y, "...", COL_TEXT, GFX_NO_BG);
}

void window_draw(window_t *w)
{
    int r = WIN_RADIUS;
    int cx0, cy0, cw, ch;
    color_t ta, tb;

    if (!w->visible)
        return;

    /* drop shadow */
    gfx_fill_round(w->x + WIN_SHADOW, w->y + WIN_SHADOW, w->w, w->h, r, COL_SHADOW);

    /* body */
    gfx_fill_round(w->x, w->y, w->w, w->h, r, w->content);

    /* title gradient (rounded top corners) */
    ta = w->active ? COL_TITLE_A : COL_TITLE_IA;
    tb = w->active ? COL_TITLE_B : COL_TITLE_IB;
    gfx_fill_round_top_grad(w->x, w->y, w->w, TITLE_H, r, ta, tb);

    /* double border: outer dark, inner light */
    gfx_outline_round(w->x, w->y, w->w, w->h, r, COL_BORDER);
    gfx_outline_round(w->x + 1, w->y + 1, w->w - 2, w->h - 2,
                      r > 0 ? r - 1 : 0, COL_BORDER_IN);

    /* traffic lights */
    draw_lights(w);

    /* title text: left-aligned after the Save button when present, else centred */
    if (w->on_title) {
        int tx = w->x + SAVE_BTN_X + SAVE_BTN_W + 12;
        int maxw = (w->x + w->w - 16) - tx;
        draw_title_trunc(tx, w->y + (TITLE_H - FONT_H) / 2, maxw, w->title);
    } else {
        gfx_draw_string(w->x + (w->w - gfx_text_width(w->title)) / 2,
                        w->y + (TITLE_H - FONT_H) / 2,
                        w->title, COL_TEXT, GFX_NO_BG);
    }

    /* optional widgets drawn inside the title bar (e.g. Save button) */
    if (w->on_title)
        w->on_title(w);

    /* application content, clipped to the content rect */
    {
        int ox, oy, ow, oh;
        gfx_get_clip(&ox, &oy, &ow, &oh);

        cx0 = w->x + 1;
        cy0 = w->y + TITLE_H;
        cw = w->w - 2;
        ch = w->h - TITLE_H - 1;

        {
            int x0 = cx0 > ox ? cx0 : ox;
            int y0 = cy0 > oy ? cy0 : oy;
            int x1 = (cx0 + cw) < (ox + ow) ? (cx0 + cw) : (ox + ow);
            int y1 = (cy0 + ch) < (oy + oh) ? (cy0 + ch) : (oy + oh);
            if (x1 > x0 && y1 > y0)
                gfx_set_clip(x0, y0, x1 - x0, y1 - y0);
            else
                gfx_set_clip(0, 0, 0, 0);
        }

        if (w->on_draw)
            w->on_draw(w);

        gfx_set_clip(ox, oy, ow, oh);
    }
}

void window_close(window_t *w)
{
    if (w)
        w->visible = 0;
}

int window_in_titlebar(window_t *w, int x, int y)
{
    if (!w->visible)
        return 0;
    return x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + TITLE_H;
}

int window_in_close(window_t *w, int x, int y)
{
    int cx, cy, dx, dy;
    if (!w->visible)
        return 0;
    cx = w->x + 22;
    cy = w->y + TITLE_H / 2;
    dx = x - cx;
    dy = y - cy;
    return (dx * dx + dy * dy) <= (13 * 13);
}
