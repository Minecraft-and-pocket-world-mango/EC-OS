#include "file_explorer.h"
#include "window.h"
#include "gfx.h"
#include "fs.h"
#include "lib.h"
#include "afm.h"
#include "open.h"
#include "i18n.h"
#include "desktop.h"
#include "icon_data.h"
#include "rename_dialog.h"

#define FE_W        1000
#define FE_H        680
#define SIDE_W      220
#define TOOLBAR_H   48
#define STATUS_H    32
#define PAD         24
#define CELL_W      140
#define CELL_H      132
#define GRID_COLS   ((FE_W - SIDE_W - 2 * PAD) / CELL_W)

#define COL_FE_BG   RGB(0x12,0x16,0x1E)
#define COL_SIDE_BG RGB(0x1A,0x1E,0x27)
#define COL_SIDE_SEL RGB(0x2C,0x3A,0x58)
#define COL_TOOL_BG RGB(0x20,0x25,0x30)
#define COL_STATUS  RGB(0x18,0x1C,0x24)
#define COL_BACK_BTN RGB(0x2C,0x33,0x42)

static window_t *fe_win = 0;
static int fe_dir = 0;
static int fe_sel = -1;

/* context-menu state for the currently right-clicked item */
static int fe_ctx_ci = -1;
static int fe_ctx_is_dir = 0;
static void fe_ctx_pick(int item);

static void fe_path(char *out, int idx)
{
    int stack[MAX_NODES];
    int depth = 0;
    int i = 0;
    const char *s;

    while (idx >= 0 && fs_nodes[idx].parent >= 0) {
        stack[depth++] = idx;
        idx = fs_nodes[idx].parent;
    }

    out[i++] = 'P';
    out[i++] = ':';
    while (depth > 0) {
        idx = stack[--depth];
        out[i++] = '/';
        s = fs_nodes[idx].name;
        while (*s && i < 120)
            out[i++] = *s++;
    }
    out[i] = '\0';
}

static void fe_trunc(const char *name, char *out)
{
    int k = 0;
    while (name[k] && k < 12) {
        out[k] = name[k];
        k++;
    }
    if (name[k]) {
        out[k++] = '.';
        out[k++] = '.';
    }
    out[k] = '\0';
}

static void blit_folder(int x, int y)
{
    int cx, cy;
    for (cy = 0; cy < FOLDER_ICON_H; cy++) {
        for (cx = 0; cx < FOLDER_ICON_W; cx++) {
            uint32_t px = folder_icon_rgba[cy * FOLDER_ICON_W + cx];
            int a = (int)((px >> 24) & 0xFF);
            if (a < 8)
                continue;
            gfx_blend(x + cx, y + cy, px & 0xFFFFFFu, a);
        }
    }
}

static void draw_app_icon(int x, int y, color_t a, color_t b, const char *glyph)
{
    gfx_fill_round(x + 8, y + 8, 48, 48, 12, a);
    gfx_fill_round(x + 13, y + 13, 38, 38, 9, b);
    gfx_draw_string(x + (64 - gfx_text_width(glyph)) / 2, y + 18,
                    glyph, COL_BLACK, GFX_NO_BG);
}

static void draw_doc_icon(int x, int y)
{
    gfx_fill_round(x + 14, y + 6, 36, 52, 6, COL_ICON_FILE);
    gfx_fill_round(x + 38, y + 6, 14, 14, 4, COL_ICON_FILE2);
    gfx_fill_rect(x + 20, y + 28, 24, 4, COL_BORDER_IN);
    gfx_fill_rect(x + 20, y + 38, 24, 4, COL_BORDER_IN);
    gfx_fill_rect(x + 20, y + 48, 16, 4, COL_BORDER_IN);
}

static void draw_item_icon(int x, int y, const fs_node_t *n)
{
    if (n->type == FS_TYPE_DIR) {
        blit_folder(x, y);
    } else if (strcmp(n->data, AFM_MINIDOS) == 0) {
        draw_app_icon(x, y, COL_ICON_DOS, COL_ICON_DOS2, ">_");
    } else if (strcmp(n->data, AFM_SETTINGS) == 0) {
        draw_app_icon(x, y, COL_ICON_APP, COL_ICON_APP2, "ST");
    } else if (strcmp(n->data, AFM_FILE_EXPLORER) == 0) {
        draw_app_icon(x, y, COL_ICON_APP2, COL_ICON_APP, "FE");
    } else {
        draw_doc_icon(x, y);
    }
}

static int side_node_at(int row)
{
    if (row == 0)
        return 0;
    if (row == 1)
        return fs_resolve("P:/Users");
    if (row == 2)
        return fs_resolve("P:/Users/desktop");
    return -1;
}

static void fe_draw(window_t *w)
{
    int bx = w->x, by = w->y + TITLE_H;
    char path[128];
    fs_node_t *d;
    int i, y;

    if (fe_dir < 0 || fe_dir >= MAX_NODES)
        fe_dir = 0;
    d = &fs_nodes[fe_dir];

    gfx_fill_rect(bx, by, SIDE_W, w->h - TITLE_H, COL_SIDE_BG);
    gfx_fill_rect(bx + SIDE_W, by, w->w - SIDE_W, TOOLBAR_H, COL_TOOL_BG);
    gfx_fill_rect(bx + SIDE_W, by + w->h - TITLE_H - STATUS_H,
                  w->w - SIDE_W, STATUS_H, COL_STATUS);

    for (i = 0; i < 3; i++) {
        int ni = side_node_at(i);
        int sy = by + TOOLBAR_H + 16 + i * 44;
        char sp[128];

        if (ni < 0)
            continue;
        if (ni == fe_dir)
            gfx_fill_round(bx + 12, sy, SIDE_W - 24, 38, 8, COL_SIDE_SEL);
        fe_path(sp, ni);
        gfx_draw_string(bx + 28, sy + (38 - FONT_H) / 2, sp,
                        ni == fe_dir ? COL_WHITE : COL_TEXT, GFX_NO_BG);
    }

    gfx_fill_round(bx + SIDE_W + PAD, by + 10, 80, TOOLBAR_H - 20, 8, COL_BACK_BTN);
    gfx_draw_string(bx + SIDE_W + PAD + 14, by + (TOOLBAR_H - FONT_H) / 2,
                    "<", COL_TEXT, GFX_NO_BG);

    fe_path(path, fe_dir);
    gfx_draw_string(bx + SIDE_W + PAD + 100, by + (TOOLBAR_H - FONT_H) / 2,
                    path, COL_TEXT_DIM, GFX_NO_BG);

    y = by + TOOLBAR_H + 16;
    for (i = 0; i < d->child_count; i++) {
        int ci = d->children[i];
        fs_node_t *n;
        int col, row, cx, cy, ix, iy;
        char label[24];
        char tr[20];
        int tw;

        if (ci < 0 || ci >= MAX_NODES)
            continue;

        n = &fs_nodes[ci];
        col = i % GRID_COLS;
        row = i / GRID_COLS;
        cx = bx + SIDE_W + PAD + col * CELL_W;
        cy = y + row * CELL_H;

        if (cy + CELL_H > by + w->h - TITLE_H - STATUS_H)
            break;

        if (ci == fe_sel)
            gfx_fill_round(cx, cy, CELL_W - 8, CELL_H - 8, 10, COL_SIDE_SEL);

        ix = cx + (CELL_W - 8 - 64) / 2;
        iy = cy + 10;
        draw_item_icon(ix, iy, n);

        fe_trunc(n->name, tr);
        strcpy(label, tr);
        tw = gfx_text_width(label);
        gfx_draw_string(cx + (CELL_W - 8 - tw) / 2, iy + 70,
                        label, COL_TEXT, GFX_NO_BG);
    }

    {
        char count[24];
        int p = itoa_u32((uint32_t)d->child_count, count);
        count[p++] = ' ';
        strcpy(count + p, i18n_str(STR_FE_ITEMS));
        gfx_draw_string(bx + SIDE_W + PAD, by + w->h - TITLE_H - STATUS_H + 6,
                        count, COL_TEXT_DIM, GFX_NO_BG);
    }
}

static void fe_open_item(int ci)
{
    fs_node_t *n;

    if (ci < 0 || ci >= MAX_NODES)
        return;
    n = &fs_nodes[ci];

    if (n->type == FS_TYPE_DIR) {
        fe_dir = ci;
        fe_sel = -1;
        desktop_redraw_all();
        return;
    }
    open_file(ci);
}

static void fe_click(window_t *w, int mx, int my)
{
    fs_node_t *d;
    int gx, gy, col, row, idx, ci;

    if (my < TOOLBAR_H) {
        if (mx >= SIDE_W + PAD && mx < SIDE_W + PAD + 80) {
            int p = fs_nodes[fe_dir].parent;
            if (p >= 0) {
                fe_dir = p;
                fe_sel = -1;
                desktop_redraw_all();
            }
        }
        return;
    }

    if (mx < SIDE_W) {
        int row = (my - TOOLBAR_H - 16) / 44;
        int ni = side_node_at(row);
        if (ni >= 0) {
            fe_dir = ni;
            fe_sel = -1;
            desktop_redraw_all();
        }
        return;
    }

    (void)w;
    gx = mx - (SIDE_W + PAD);
    gy = my - (TOOLBAR_H + 16);
    if (gx < 0 || gy < 0)
        return;
    col = gx / CELL_W;
    row = gy / CELL_H;
    if (col < 0 || col >= GRID_COLS)
        return;
    idx = row * GRID_COLS + col;

    d = &fs_nodes[fe_dir];
    if (idx < 0 || idx >= d->child_count) {
        fe_sel = -1;
        desktop_redraw_all();
        return;
    }

    ci = d->children[idx];
    if (ci < 0 || ci >= MAX_NODES)
        return;

    if (fs_nodes[ci].type == FS_TYPE_DIR || ci == fe_sel)
        fe_open_item(ci);
    else {
        fe_sel = ci;
        desktop_redraw_all();
    }
}

/* Return the fs index of the grid cell under content coords (mx,my), or -1. */
static int fe_grid_item(int mx, int my)
{
    fs_node_t *d;
    int gx, gy, col, row, idx;

    if (mx < SIDE_W || my < TOOLBAR_H)
        return -1;
    gx = mx - (SIDE_W + PAD);
    gy = my - (TOOLBAR_H + 16);
    if (gx < 0 || gy < 0)
        return -1;
    col = gx / CELL_W;
    row = gy / CELL_H;
    if (col < 0 || col >= GRID_COLS)
        return -1;
    idx = row * GRID_COLS + col;
    d = &fs_nodes[fe_dir];
    if (idx < 0 || idx >= d->child_count)
        return -1;
    return d->children[idx];
}

static void fe_ctx_pick(int item)
{
    if (fe_ctx_ci < 0 || fe_ctx_ci >= MAX_NODES)
        return;

    if (fe_ctx_is_dir) {
        if (item == 0) {
            fe_dir = fe_ctx_ci;
            fe_sel = -1;
            desktop_redraw_all();
        } else if (item == 1) {
            rename_dialog_open(fe_ctx_ci);
        } else if (item == 3) {
            fs_delete(fe_ctx_ci);
            fe_sel = -1;
            desktop_redraw_all();
        }
    } else {
        if (item == 0) {
            open_file(fe_ctx_ci);
        } else if (item == 1) {
            fs_delete(fe_ctx_ci);
            fe_sel = -1;
            desktop_redraw_all();
        } else if (item == 2) {
            rename_dialog_open(fe_ctx_ci);
        }
    }
}

static void fe_right_click(window_t *w, int mx, int my)
{
    static const char *labels[4];
    int ci = fe_grid_item(mx, my);

    if (ci < 0)
        return;

    fe_ctx_ci = ci;
    fe_ctx_is_dir = (fs_nodes[ci].type == FS_TYPE_DIR);
    fe_sel = ci;

    if (fe_ctx_is_dir) {
        labels[0] = i18n_str(STR_MENU_OPEN);
        labels[1] = i18n_str(STR_MENU_RENAME);
        labels[2] = i18n_str(STR_MENU_COPY);
        labels[3] = i18n_str(STR_MENU_DELETE);
    } else {
        labels[0] = i18n_str(STR_MENU_OPEN);
        labels[1] = i18n_str(STR_MENU_DELETE);
        labels[2] = i18n_str(STR_MENU_RENAME);
        labels[3] = i18n_str(STR_MENU_COPY);
    }

    desktop_redraw_all();
    desktop_context_menu(w->x + mx, w->y + TITLE_H + my, labels, 4, fe_ctx_pick);
}

void file_explorer_open_at(int idx)
{
    if (idx < 0 || idx >= MAX_NODES)
        idx = 0;

    fe_dir = idx;
    fe_sel = -1;

    if (fe_win && fe_win->visible && fe_win->id == 1)
        return;

    fe_win = window_create((SCREEN_W - FE_W) / 2, (SCREEN_H - FE_H) / 2,
                           FE_W, FE_H, i18n_str(STR_TITLE_FILES),
                           COL_FE_BG, fe_draw, fe_click, 1);
    if (fe_win)
        fe_win->on_right_click = fe_right_click;
}

void file_explorer_open(void)
{
    int d = fs_resolve("P:/Users/desktop");
    file_explorer_open_at(d >= 0 ? d : 0);
}
