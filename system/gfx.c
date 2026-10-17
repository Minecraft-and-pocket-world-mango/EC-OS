#include "gfx.h"
#include "font.h"
#include "vbe.h"
#include "lib.h"

/* ------------------------------------------------------------------ *
 *  Double buffering
 *  - back buffer : 8 MiB static array in .bss (never loaded from disk)
 *  - front  (fb) : VBE linear framebuffer, written by gfx_flip()
 * ------------------------------------------------------------------ */
static uint32_t backbuf[SCREEN_W * SCREEN_H];
static volatile uint32_t *fb = 0;
static uint32_t fb_pitch = SCREEN_W;          /* in 32-bit units */

typedef struct { int x, y, w, h; } rect_t;

#define DIRTY_MAX 96
static rect_t dirty[DIRTY_MAX];
static int    dirty_n = 0;
static int    dirty_full = 1;

static rect_t clip;

#define SCREEN_RECT ((rect_t){ 0, 0, SCREEN_W, SCREEN_H })

/* ---- fast 32-bit block memory ops (rep stosl / rep movsl) ---- */
static inline void fill32(uint32_t *dst, uint32_t color, uint32_t count)
{
    __asm__ volatile("rep stosl"
                     : "+D"(dst), "+c"(count)
                     : "a"(color)
                     : "memory");
}

static inline void copy32(uint32_t *dst, const uint32_t *src, uint32_t count)
{
    __asm__ volatile("rep movsl"
                     : "+D"(dst), "+S"(src), "+c"(count)
                     :
                     : "memory");
}

static int isqrt_i(int v)
{
    int x = 0;
    if (v <= 0)
        return 0;
    while ((x + 1) * (x + 1) <= v)
        x++;
    return x;
}

static color_t lerp_color(color_t a, color_t b, int t)   /* t: 0..256 */
{
    int ar = (int)((a >> 16) & 0xFF), ag = (int)((a >> 8) & 0xFF), ab = (int)(a & 0xFF);
    int br = (int)((b >> 16) & 0xFF), bg = (int)((b >> 8) & 0xFF), bb = (int)(b & 0xFF);
    int r = ar + (((br - ar) * t) >> 8);
    int g = ag + (((bg - ag) * t) >> 8);
    int c = ab + (((bb - ab) * t) >> 8);
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (c < 0) c = 0;
    if (c > 255) c = 255;
    return ((color_t)r << 16) | ((color_t)g << 8) | (color_t)c;
}

static int rect_intersect(rect_t a, rect_t b, rect_t *out)
{
    int x0 = a.x > b.x ? a.x : b.x;
    int y0 = a.y > b.y ? a.y : b.y;
    int x1 = (a.x + a.w) < (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    int y1 = (a.y + a.h) < (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    if (x1 <= x0 || y1 <= y0)
        return 0;
    out->x = x0; out->y = y0; out->w = x1 - x0; out->h = y1 - y0;
    return 1;
}

/* ------------------------------------------------------------------ */

void gfx_init(void)
{
    fb = (volatile uint32_t *)vbe_framebuffer();
    fb_pitch = vbe_pitch() / 4u;
    if (fb_pitch == 0)
        fb_pitch = SCREEN_W;
    clip = SCREEN_RECT;
    dirty_n = 0;
    dirty_full = 1;
}

color_t *gfx_backbuffer(void) { return backbuf; }

void gfx_mark_all_dirty(void)
{
    dirty_full = 1;
    dirty_n = 0;
}

void gfx_mark_dirty(int x, int y, int w, int h)
{
    rect_t r, tmp;
    int i;

    if (dirty_full)
        return;

    r.x = x; r.y = y; r.w = w; r.h = h;
    if (!rect_intersect(r, SCREEN_RECT, &tmp))
        return;
    r = tmp;

    for (i = 0; i < dirty_n; i++) {
        rect_t u = dirty[i];
        if (r.x >= u.x && r.y >= u.y &&
            r.x + r.w <= u.x + u.w && r.y + r.h <= u.y + u.h)
            return;
    }
    if (dirty_n >= DIRTY_MAX) {
        gfx_mark_all_dirty();
        return;
    }
    dirty[dirty_n++] = r;
}

void gfx_flip(void)
{
    int i, row;

    if (!fb)
        return;

    if (dirty_full) {
        for (row = 0; row < SCREEN_H; row++)
            copy32((uint32_t *)&fb[row * fb_pitch],
                   &backbuf[row * SCREEN_W], SCREEN_W);
        dirty_full = 0;
        dirty_n = 0;
        return;
    }

    for (i = 0; i < dirty_n; i++) {
        rect_t r = dirty[i];
        for (row = 0; row < r.h; row++)
            copy32((uint32_t *)&fb[(r.y + row) * fb_pitch + r.x],
                   &backbuf[(r.y + row) * SCREEN_W + r.x], (uint32_t)r.w);
    }
    dirty_n = 0;
}

/* ------------------------------------------------------------------ */

void gfx_set_clip(int x, int y, int w, int h)
{
    rect_t r;
    r.x = x; r.y = y; r.w = w; r.h = h;
    if (!rect_intersect(r, SCREEN_RECT, &clip))
        clip = (rect_t){ 0, 0, 0, 0 };
}

void gfx_get_clip(int *x, int *y, int *w, int *h)
{
    if (x) *x = clip.x;
    if (y) *y = clip.y;
    if (w) *w = clip.w;
    if (h) *h = clip.h;
}

void gfx_reset_clip(void)
{
    clip = SCREEN_RECT;
}

/* ------------------------------------------------------------------ */

void gfx_pixel(int x, int y, color_t c)
{
    if (x < clip.x || x >= clip.x + clip.w || y < clip.y || y >= clip.y + clip.h)
        return;
    backbuf[y * SCREEN_W + x] = c;
    gfx_mark_dirty(x, y, 1, 1);
}

void gfx_hline(int x, int y, int w, color_t c)
{
    int x1 = x + w;
    if (w <= 0)
        return;
    if (y < clip.y || y >= clip.y + clip.h)
        return;
    if (x < clip.x) x = clip.x;
    if (x1 > clip.x + clip.w) x1 = clip.x + clip.w;
    if (x >= x1)
        return;
    fill32(&backbuf[y * SCREEN_W + x], c, (uint32_t)(x1 - x));
    gfx_mark_dirty(x, y, x1 - x, 1);
}

void gfx_vline(int x, int y, int h, color_t c)
{
    int y1 = y + h;
    int yy;
    if (h <= 0)
        return;
    if (x < clip.x || x >= clip.x + clip.w)
        return;
    if (y < clip.y) y = clip.y;
    if (y1 > clip.y + clip.h) y1 = clip.y + clip.h;
    for (yy = y; yy < y1; yy++)
        backbuf[yy * SCREEN_W + x] = c;
    if (y1 > y)
        gfx_mark_dirty(x, y, 1, y1 - y);
}

void gfx_fill_rect(int x, int y, int w, int h, color_t c)
{
    int x0 = x, y0 = y, x1 = x + w, y1 = y + h, yy;

    if (x0 < clip.x) x0 = clip.x;
    if (y0 < clip.y) y0 = clip.y;
    if (x1 > clip.x + clip.w) x1 = clip.x + clip.w;
    if (y1 > clip.y + clip.h) y1 = clip.y + clip.h;
    if (x0 >= x1 || y0 >= y1)
        return;

    for (yy = y0; yy < y1; yy++)
        fill32(&backbuf[yy * SCREEN_W + x0], c, (uint32_t)(x1 - x0));
    gfx_mark_dirty(x0, y0, x1 - x0, y1 - y0);
}

void gfx_rect(int x, int y, int w, int h, color_t c)
{
    if (w <= 0 || h <= 0)
        return;
    gfx_hline(x, y, w, c);
    gfx_hline(x, y + h - 1, w, c);
    gfx_vline(x, y, h, c);
    gfx_vline(x + w - 1, y, h, c);
}

void gfx_line(int x0, int y0, int x1, int y1, color_t c)
{
    int dx = x1 - x0, dy = y1 - y0;
    int sx = dx < 0 ? -1 : 1;
    int sy = dy < 0 ? -1 : 1;
    int err, e2;

    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    err = dx - dy;

    for (;;) {
        gfx_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

void gfx_blend(int x, int y, color_t c, int alpha)
{
    color_t d, r, g, b, da;
    if (x < clip.x || x >= clip.x + clip.w || y < clip.y || y >= clip.y + clip.h)
        return;
    if (alpha <= 0)
        return;
    if (alpha >= 255) {
        backbuf[y * SCREEN_W + x] = c;
        gfx_mark_dirty(x, y, 1, 1);
        return;
    }
    d = backbuf[y * SCREEN_W + x];
    da = 255 - alpha;
    r = (color_t)((((c >> 16) & 0xFF) * alpha + ((d >> 16) & 0xFF) * da) / 255);
    g = (color_t)((((c >> 8) & 0xFF) * alpha + ((d >> 8) & 0xFF) * da) / 255);
    b = (color_t)(((c & 0xFF) * alpha + (d & 0xFF) * da) / 255);
    backbuf[y * SCREEN_W + x] = (r << 16) | (g << 8) | b;
    gfx_mark_dirty(x, y, 1, 1);
}

/* ------------------------------------------------------------------ */

void gfx_fill_circle(int cx, int cy, int r, color_t c)
{
    int dy;
    if (r <= 0)
        return;
    for (dy = -r; dy <= r; dy++) {
        int dx = isqrt_i(r * r - dy * dy);
        gfx_hline(cx - dx, cy + dy, 2 * dx + 1, c);
    }
}

void gfx_fill_round(int x, int y, int w, int h, int r, color_t c)
{
    int i;
    if (w <= 0 || h <= 0)
        return;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    if (r < 0) r = 0;

    for (i = 0; i < h; i++) {
        int inset = 0;
        if (i < r) {
            int d = r - i;
            inset = r - isqrt_i(r * r - d * d);
        } else if (i >= h - r) {
            int d = r - (h - 1 - i);
            inset = r - isqrt_i(r * r - d * d);
        }
        if (inset < 0) inset = 0;
        if (inset * 2 >= w) continue;
        gfx_hline(x + inset, y + i, w - 2 * inset, c);
    }
}

void gfx_outline_round(int x, int y, int w, int h, int r, color_t c)
{
    int i;
    if (w <= 0 || h <= 0)
        return;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    if (r < 0) r = 0;

    gfx_hline(x + r, y, w - 2 * r, c);
    gfx_hline(x + r, y + h - 1, w - 2 * r, c);
    gfx_vline(x, y + r, h - 2 * r, c);
    gfx_vline(x + w - 1, y + r, h - 2 * r, c);

    for (i = 0; i < r; i++) {
        int d = r - i;
        int inset = r - isqrt_i(r * r - d * d);
        gfx_pixel(x + inset, y + i, c);
        gfx_pixel(x + w - 1 - inset, y + i, c);
        gfx_pixel(x + inset, y + h - 1 - i, c);
        gfx_pixel(x + w - 1 - inset, y + h - 1 - i, c);
    }
}

void gfx_fill_vgrad(int x, int y, int w, int h, color_t top, color_t bot)
{
    int i;
    if (w <= 0 || h <= 0)
        return;
    for (i = 0; i < h; i++) {
        int t = (h <= 1) ? 0 : (i * 256) / (h - 1);
        gfx_hline(x, y + i, w, lerp_color(top, bot, t));
    }
}

void gfx_fill_round_grad(int x, int y, int w, int h, int r,
                         color_t top, color_t bot)
{
    int i;
    if (w <= 0 || h <= 0)
        return;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    if (r < 0) r = 0;

    for (i = 0; i < h; i++) {
        int inset = 0;
        int t = (h <= 1) ? 0 : (i * 256) / (h - 1);
        if (i < r) {
            int d = r - i;
            inset = r - isqrt_i(r * r - d * d);
        } else if (i >= h - r) {
            int d = r - (h - 1 - i);
            inset = r - isqrt_i(r * r - d * d);
        }
        if (inset < 0) inset = 0;
        if (inset * 2 >= w) continue;
        gfx_hline(x + inset, y + i, w - 2 * inset, lerp_color(top, bot, t));
    }
}

void gfx_fill_round_top_grad(int x, int y, int w, int h, int r,
                             color_t top, color_t bot)
{
    int i;
    if (w <= 0 || h <= 0)
        return;
    if (r * 2 > w) r = w / 2;
    if (r < 0) r = 0;

    for (i = 0; i < h; i++) {
        int inset = 0;
        int t = (h <= 1) ? 0 : (i * 256) / (h - 1);
        if (i < r) {
            int d = r - i;
            inset = r - isqrt_i(r * r - d * d);
        }
        if (inset < 0) inset = 0;
        if (inset * 2 >= w) continue;
        gfx_hline(x + inset, y + i, w - 2 * inset, lerp_color(top, bot, t));
    }
}

/* ------------------------------------------------------------------ */

void gfx_draw_char(int x, int y, char ch, color_t fg, color_t bg)
{
    const uint16_t *glyph;
    int idx = (int)(unsigned char)ch;
    int row, col;
    int gx0 = clip.x, gy0 = clip.y;
    int gx1 = clip.x + clip.w, gy1 = clip.y + clip.h;
    int has_bg = (bg != GFX_NO_BG);

    if (idx < 0x20 || idx > 0x7F)
        return;
    glyph = font16x32[idx - 0x20];

    for (row = 0; row < FONT_H; row++) {
        int py = y + row;
        uint32_t *rowp;
        uint16_t bits;

        if (py < gy0 || py >= gy1)
            continue;
        rowp = &backbuf[py * SCREEN_W];
        bits = glyph[row];

        for (col = 0; col < FONT_W; col++) {
            int px = x + col;
            int on;
            if (px < gx0 || px >= gx1)
                continue;
            on = bits & (0x8000 >> col);
            if (on)
                rowp[px] = fg;
            else if (has_bg)
                rowp[px] = bg;
        }
    }
    gfx_mark_dirty(x, y, FONT_W, FONT_H);
}

void gfx_draw_string(int x, int y, const char *s, color_t fg, color_t bg)
{
    while (*s) {
        gfx_draw_char(x, y, *s, fg, bg);
        x += FONT_W;
        s++;
    }
}

int gfx_text_width(const char *s)
{
    return (int)strlen(s) * FONT_W;
}
