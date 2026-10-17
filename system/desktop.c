#include "desktop.h"
#include "gfx.h"
#include "mouse.h"
#include "keyboard.h"
#include "fs.h"
#include "lib.h"
#include "window.h"
#include "afm.h"
#include "file_explorer.h"
#include "minidos.h"
#include "settings.h"
#include "i18n.h"
#include "io.h"
#include "open.h"
#include "text_edit.h"
#include "text_edit.h"
#include "rename_dialog.h"

/* ------------------------------------------------------------------ */
#define TB_H          56
#define ICON_ART      76
#define ICON_CELL_W   148
#define ICON_CELL_H   158
#define ICON_MX       40
#define ICON_MY       (TB_H + 28)
#define MAX_ICONS     24

#define MENU_W        380
#define MENU_ITEM_H   46
#define MAX_MENU      6

#define COL_SEL_BG    RGB(0x1E,0x36,0x60)
#define COL_TOAST_TOP RGB(0x2A,0x30,0x3C)
#define COL_TOAST_BOT RGB(0x1A,0x1E,0x26)

typedef struct icon_t {
    int  idx;
    int  x, y;
    int  w, h;
    int  type;
    char label[32];
} icon_t;

static icon_t icons[MAX_ICONS];
static int  icon_count = 0;
static int  desk_idx = 0;
static int  selected_icon = -1;

static int  mb_left = 0, mb_right = 0;
static int  press_x = 0, press_y = 0;
static int  was_drag = 0;
static int  drag_win = -1;
static int  drag_ox = 0, drag_oy = 0;

static int  menu_open = 0;
static int  menu_items = 0;
static int  menu_mode = 0;
static int  menu_x = 0, menu_y = 0;
static int  menu_target = -1;
static char menu_text[MAX_MENU][28];
static int  menu_hover = -1;
static void (*menu_pick)(int) = 0;

static char msg[64];
static int  msg_ticks = 0;
static int  msg_repaint = 0;

static char clock_str[8] = "--:--";
static int  exit_desktop = 0;

static const str_id_t m_desktop[MAX_MENU] = {
    STR_MENU_CREATE_FILE, STR_MENU_OPEN_DOS, STR_MENU_REFRESH,
    STR_MENU_WALLPAPER, STR_MENU_SHUTDOWN
};
static const str_id_t m_file[4] = {
    STR_MENU_OPEN, STR_MENU_DELETE, STR_MENU_RENAME, STR_MENU_COPY
};
static const str_id_t m_dir[4] = {
    STR_MENU_OPEN, STR_MENU_RENAME, STR_MENU_COPY, STR_MENU_DELETE
};

static int in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

static color_t lerp(color_t a, color_t b, int t)
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

static color_t bg_at(int y)
{
    int t = (y * 256) / (SCREEN_H - 1);
    return lerp(COL_BG_TOP, COL_BG_BOT, t);
}

/* ------------------------------------------------------------------ */
/*  CMOS clock                                                        */
/* ------------------------------------------------------------------ */
static uint8_t cmos_read(uint8_t reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static int cmos_updating(void)
{
    return cmos_read(0x0A) & 0x80;
}

static int bcd2bin(int v)
{
    return (v & 0x0F) + ((v >> 4) * 10);
}

static void read_clock(char *out)
{
    int hour, min, sec, regb;
    char hb[4], mb[4];

    do {
        sec = cmos_read(0x00);
        min = cmos_read(0x02);
        hour = cmos_read(0x04);
        regb = cmos_read(0x0B);
    } while (cmos_updating() || sec != cmos_read(0x00));

    if ((regb & 0x04) == 0) {
        hour = bcd2bin(hour);
        min = bcd2bin(min);
    }

    itoa_u32_pad((uint32_t)hour, 2, '0', hb);
    itoa_u32_pad((uint32_t)min, 2, '0', mb);

    out[0] = hb[0]; out[1] = hb[1]; out[2] = ':';
    out[3] = mb[0]; out[4] = mb[1]; out[5] = '\0';
}

static void clock_rect(int *x, int *y, int *w, int *h)
{
    *w = 176;
    *h = TB_H - 20;
    *x = SCREEN_W - *w - 24;
    *y = 10;
}

/* ------------------------------------------------------------------ */

static void show_msg(const char *s)
{
    strncpy(msg, s, sizeof(msg) - 1);
    msg[sizeof(msg) - 1] = '\0';
    msg_ticks = 160;
    msg_repaint = 1;
}

void desktop_toast(const char *s)
{
    show_msg(s);
}

static void toast_rect(int *x, int *y, int *w, int *h)
{
    int tw = gfx_text_width(msg) + 64;
    if (tw < 420) tw = 420;
    *w = tw;
    *h = 60;
    *x = 32;
    *y = SCREEN_H - 92;
}

static uint32_t acpi_rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int acpi_tag_eq(const uint8_t *p, const char *s)
{
    int i;
    for (i = 0; s[i]; i++)
        if (p[i] != (uint8_t)s[i])
            return 0;
    return 1;
}

static int acpi_checksum_ok(const uint8_t *p, int len)
{
    uint8_t sum = 0;
    int i;
    for (i = 0; i < len; i++)
        sum = (uint8_t)(sum + p[i]);
    return sum == 0;
}

static uint8_t *acpi_find_rsdp(void)
{
    uint32_t ebda = (uint32_t)inw(0x40E) << 4;
    uint8_t *p;

    if (ebda >= 0x400 && ebda <= 0xA0000) {
        for (p = (uint8_t *)ebda; p < (uint8_t *)(ebda + 1024); p += 16)
            if (acpi_tag_eq(p, "RSD PTR ") && acpi_checksum_ok(p, 20))
                return p;
    }
    for (p = (uint8_t *)0xE0000; p < (uint8_t *)0x100000; p += 16)
        if (acpi_tag_eq(p, "RSD PTR ") && acpi_checksum_ok(p, 20))
            return p;
    return 0;
}

static int acpi_try_shutdown(void)
{
    uint8_t *rsdp, *rsdt, *fadt = 0;
    uint32_t rsdt_addr, len, count, i;
    uint16_t pm1a, pm1b;

    rsdp = acpi_find_rsdp();
    if (!rsdp)
        return -1;
    rsdt_addr = acpi_rd32(rsdp + 16);
    if (rsdt_addr == 0)
        return -1;
    rsdt = (uint8_t *)rsdt_addr;
    if (!acpi_tag_eq(rsdt, "RSDT"))
        return -1;

    len = acpi_rd32(rsdt + 4);
    if (len < 36)
        return -1;
    count = (len - 36) / 4;
    for (i = 0; i < count; i++) {
        uint8_t *tp = (uint8_t *)acpi_rd32(rsdt + 36 + i * 4);
        if (acpi_tag_eq(tp, "FACP")) {
            fadt = tp;
            break;
        }
    }
    if (!fadt)
        return -1;

    pm1a = (uint16_t)acpi_rd32(fadt + 64);
    pm1b = (uint16_t)acpi_rd32(fadt + 68);

    if (pm1a) {
        outw(pm1a, (uint16_t)((5 << 10) | (1 << 13)));
        outw(pm1a, (uint16_t)((0 << 10) | (1 << 13)));
    }
    if (pm1b) {
        outw(pm1b, (uint16_t)((5 << 10) | (1 << 13)));
        outw(pm1b, (uint16_t)((0 << 10) | (1 << 13)));
    }
    return 0;
}

static void shutdown_power(void)
{
    show_msg(i18n_str(STR_MSG_SHUTDOWN_OK));

    if (acpi_try_shutdown() == 0)
        return;

    outw(0xB004, 0x2000);
    outw(0x604, 0x2000);

    show_msg(i18n_str(STR_MSG_SHUTDOWN_FAIL));
}

static void desktop_setup(void)
{
    int users, desk;

    users = fs_resolve("P:/Users");
    if (users < 0)
        users = fs_create(0, "Users", FS_TYPE_DIR);
    if (users < 0)
        users = 0;

    desk = fs_resolve("P:/Users/desktop");
    if (desk < 0)
        desk = fs_create(users, "desktop", FS_TYPE_DIR);
    if (desk < 0)
        desk = users;

    desk_idx = desk;

    if (fs_nodes[desk].child_count == 0) {
        int f1 = fs_create(desk, "File_Explorer.afm", FS_TYPE_FILE);
        int f2 = fs_create(desk, "MiniDOS.afm", FS_TYPE_FILE);
        int f3 = fs_create(desk, "Settings.afm", FS_TYPE_FILE);
        if (f1 >= 0) strcpy(fs_nodes[f1].data, AFM_FILE_EXPLORER);
        if (f2 >= 0) strcpy(fs_nodes[f2].data, AFM_MINIDOS);
        if (f3 >= 0) strcpy(fs_nodes[f3].data, AFM_SETTINGS);
    }
}

static void build_icons(void)
{
    fs_node_t *d = &fs_nodes[desk_idx];
    int i;
    int rows = (SCREEN_H - ICON_MY - ICON_CELL_H) / ICON_CELL_H + 1;
    int col, row;

    if (rows < 1) rows = 1;
    icon_count = 0;

    for (i = 0; i < d->child_count && icon_count < MAX_ICONS; i++) {
        int ci = d->children[i];
        fs_node_t *n;
        icon_t *ic;
        int k;

        if (ci < 0 || ci >= MAX_NODES)
            continue;

        n = &fs_nodes[ci];
        ic = &icons[icon_count];
        col = icon_count / rows;
        row = icon_count % rows;

        ic->idx = ci;
        ic->type = n->type;
        ic->x = ICON_MX + col * ICON_CELL_W;
        ic->y = ICON_MY + row * ICON_CELL_H;
        ic->w = ICON_ART;
        ic->h = ICON_ART;

        for (k = 0; k < 30 && n->name[k]; k++)
            ic->label[k] = n->name[k];
        ic->label[k] = '\0';

        icon_count++;
    }
}

/* ------------------------------------------------------------------ */
/*  icons                                                             */
/* ------------------------------------------------------------------ */
static void draw_folder(int x, int y)
{
    gfx_fill_round(x + 6, y + 8, 36, 18, 7, COL_ICON_DIR);
    gfx_fill_round(x, y + 18, ICON_ART, ICON_ART - 18, 12, COL_ICON_DIR);
    gfx_fill_round(x + 5, y + 23, ICON_ART - 10, ICON_ART - 30, 10, COL_ICON_DIR2);
}

static void draw_file(int x, int y)
{
    gfx_fill_round(x + 14, y, ICON_ART - 28, ICON_ART, 8, COL_ICON_FILE);
    gfx_fill_round(x + ICON_ART - 28 - 14, y, 20, 20, 6, COL_ICON_FILE2);
    gfx_fill_rect(x + 22, y + 34, ICON_ART - 44, 5, COL_BORDER_IN);
    gfx_fill_rect(x + 22, y + 46, ICON_ART - 44, 5, COL_BORDER_IN);
    gfx_fill_rect(x + 22, y + 58, (ICON_ART - 44) * 2 / 3, 5, COL_BORDER_IN);
}

static void draw_app(int x, int y, color_t a, color_t b, const char *glyph)
{
    gfx_fill_round(x + 6, y + 6, ICON_ART - 12, ICON_ART - 12, 16, a);
    gfx_fill_round(x + 11, y + 11, ICON_ART - 22, ICON_ART - 22, 12, b);
    gfx_draw_string(x + (ICON_ART - gfx_text_width(glyph)) / 2, y + 22,
                    glyph, COL_BLACK, GFX_NO_BG);
}

static void draw_icon(const icon_t *ic, int sel)
{
    fs_node_t *n = &fs_nodes[ic->idx];
    int x = ic->x, y = ic->y;
    int tw, lx;

    if (sel)
        gfx_fill_round(x - 14, y - 10, ICON_ART + 28, ICON_ART + FONT_H + 20,
                       10, COL_SEL_BG);

    if (n->type == FS_TYPE_DIR) {
        draw_folder(x, y);
    } else if (strcmp(n->data, AFM_MINIDOS) == 0) {
        draw_app(x, y, COL_ICON_DOS, COL_ICON_DOS2, ">_");
    } else if (strcmp(n->data, AFM_FILE_EXPLORER) == 0) {
        draw_app(x, y, COL_ICON_APP2, COL_ICON_APP, "FE");
    } else if (strcmp(n->data, AFM_SETTINGS) == 0) {
        draw_app(x, y, COL_ICON_APP, COL_ICON_APP2, "ST");
    } else {
        draw_file(x, y);
    }

    tw = gfx_text_width(ic->label);
    lx = x + ICON_ART / 2 - tw / 2;
    if (lx < 4)
        lx = 4;
    gfx_draw_string(lx, y + ICON_ART + 6, ic->label, COL_TEXT, GFX_NO_BG);
}

/* ------------------------------------------------------------------ */
/*  taskbar                                                           */
/* ------------------------------------------------------------------ */
static void draw_taskbar(void)
{
    int cx, cy, cw, ch;
    const char *hint = i18n_str(STR_HINT_TASKBAR);
    int tx, ty, tw;

    gfx_fill_vgrad(0, 0, SCREEN_W, TB_H, COL_PANEL_TOP, COL_PANEL_BOT);
    gfx_hline(0, TB_H - 2, SCREEN_W, COL_BORDER);
    gfx_hline(0, TB_H - 1, SCREEN_W, COL_BORDER_IN);

    /* EC-OS button */
    gfx_fill_round(20, 8, 168, TB_H - 16, 12, COL_ACCENT);
    gfx_fill_round(22, 10, 164, TB_H - 20, 11, COL_ACCENT2);
    gfx_fill_round(24, 12, 160, TB_H - 24, 10, COL_ACCENT);
    gfx_draw_string(52, 12, "EC-OS", COL_WHITE, GFX_NO_BG);

    /* clock */
    clock_rect(&cx, &cy, &cw, &ch);
    gfx_draw_string(cx + (cw - gfx_text_width(clock_str)) / 2,
                    cy + (ch - FONT_H) / 2, clock_str, COL_TEXT, GFX_NO_BG);

    /* hint: right-click menu / ESC exit */
    tw = gfx_text_width(hint);
    tx = cx - tw - 16;
    ty = cy + (ch - FONT_H) / 2;
    if (tx < 200)
        tx = 200;
    gfx_draw_string(tx, ty, hint, COL_TEXT_DIM, GFX_NO_BG);
}

/* ------------------------------------------------------------------ */
/*  context menu                                                      */
/* ------------------------------------------------------------------ */
static void draw_menu(void)
{
    int i;
    int h = menu_items * MENU_ITEM_H;

    gfx_fill_round(menu_x, menu_y, MENU_W, h, 12, COL_BORDER);
    gfx_fill_round_grad(menu_x + 2, menu_y + 2, MENU_W - 4, h - 4, 10,
                        COL_MENU_TOP, COL_MENU_BOT);

    for (i = 0; i < menu_items; i++) {
        int my = menu_y + i * MENU_ITEM_H;
        int hover = in_rect(mouse_x, mouse_y, menu_x, my, MENU_W, MENU_ITEM_H);

        if (hover) {
            gfx_fill_round(menu_x + 6, my + 5, MENU_W - 12, MENU_ITEM_H - 10,
                           8, COL_HOVER_BOT);
            gfx_fill_round(menu_x + 6, my + 5, MENU_W - 12, MENU_ITEM_H - 10,
                           8, COL_HOVER_TOP);
        }
        gfx_draw_string(menu_x + 24, my + (MENU_ITEM_H - FONT_H) / 2,
                        menu_text[i],
                        hover ? COL_WHITE : COL_TEXT, GFX_NO_BG);
    }
}

/* ------------------------------------------------------------------ */
/*  compositor : repaint an arbitrary rectangle of the scene          */
/* ------------------------------------------------------------------ */
static void paint_rect(int x, int y, int w, int h)
{
    int i, yy;
    int x0 = x, y0 = y, x1 = x + w, y1 = y + h;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x0 >= x1 || y0 >= y1)
        return;

    gfx_set_clip(x0, y0, x1 - x0, y1 - y0);

    for (yy = y0; yy < y1; yy++)
        gfx_hline(x0, yy, x1 - x0, bg_at(yy));

    for (i = 0; i < icon_count; i++)
        draw_icon(&icons[i], i == selected_icon);

    for (i = 0; i < MAX_WINDOWS; i++)
        if (windows[i].visible)
            window_draw(&windows[i]);

    draw_taskbar();

    if (menu_open)
        draw_menu();

    if (msg_ticks > 0) {
        int tx, ty, tw, th;
        toast_rect(&tx, &ty, &tw, &th);
        gfx_fill_round(tx, ty, tw, th, 12, COL_BORDER);
        gfx_fill_round_grad(tx + 2, ty + 2, tw - 4, th - 4, 10,
                            COL_TOAST_TOP, COL_TOAST_BOT);
        gfx_draw_string(tx + 28, ty + (th - FONT_H) / 2, msg, COL_TEXT, GFX_NO_BG);
    }

    if (rename_dialog_is_open())
        rename_dialog_draw();

    gfx_reset_clip();
    gfx_mark_dirty(x0, y0, x1 - x0, y1 - y0);
}

/* 全屏重绘入口：供 window_create() 在创建窗口后立即调用，
 * 保证新窗口无需拖动即可显示。                                       */
void desktop_redraw_all(void)
{
    paint_rect(0, 0, SCREEN_W, SCREEN_H);
}

/* Rebuild the desktop icon list and repaint the content area.
 * Used after a rename so the new label shows up immediately. */
void desktop_refresh_icons(void)
{
    build_icons();
    paint_rect(0, TB_H, SCREEN_W, SCREEN_H - TB_H);
}

/* ------------------------------------------------------------------ */
/*  hit testing                                                       */
/* ------------------------------------------------------------------ */
static int icon_at(int x, int y)
{
    int i;
    for (i = 0; i < icon_count; i++) {
        int ix = icons[i].x, iy = icons[i].y;
        if (in_rect(x, y, ix - 14, iy - 10, ICON_ART + 28, ICON_ART + FONT_H + 20))
            return i;
    }
    return -1;
}

static int top_window_index(int x, int y)
{
    int i;
    for (i = MAX_WINDOWS - 1; i >= 0; i--) {
        if (!windows[i].visible)
            continue;
        if (x >= windows[i].x && x < windows[i].x + windows[i].w &&
            y >= windows[i].y && y < windows[i].y + windows[i].h)
            return i;
    }
    return -1;
}

static int menu_item_at(int x, int y)
{
    if (!menu_open)
        return -1;
    if (x < menu_x || x >= menu_x + MENU_W)
        return -1;
    if (y < menu_y || y >= menu_y + menu_items * MENU_ITEM_H)
        return -1;
    return (y - menu_y) / MENU_ITEM_H;
}

static void bring_to_front(int idx)
{
    window_t tmp;
    int last = -1, i;

    for (i = 0; i < MAX_WINDOWS; i++)
        if (windows[i].visible)
            last = i;
    if (idx < 0 || idx >= MAX_WINDOWS || !windows[idx].visible)
        return;
    if (last < 0 || last == idx)
        return;

    tmp = windows[idx];
    windows[idx] = windows[last];
    windows[last] = tmp;
}

/* ------------------------------------------------------------------ */
/*  actions                                                           */
/* ------------------------------------------------------------------ */
static void action_create_file(void)
{
    char name[24];
    window_t *w = 0;
    int attempt;

    /* 用递增后缀挑一个不重名的文件名，创建后直接打开文本编辑器。
     * text_edit_open_new() 内部会调用 fs_create()，重名时返回 0。 */
    for (attempt = 1; attempt <= 99 && !w; attempt++) {
        int k = 0;
        const char *base = "New_File";

        while (*base && k < 16)
            name[k++] = *base++;

        if (attempt > 1) {
            char num[8];
            int t = attempt - 1, n = 0;
            name[k++] = '_';
            do { num[n++] = (char)('0' + (t % 10)); t /= 10; } while (t > 0);
            while (n > 0)
                name[k++] = num[--n];
        }

        name[k++] = '.';
        name[k++] = 't';
        name[k++] = 'x';
        name[k++] = 't';
        name[k] = '\0';

        w = text_edit_open_new(desk_idx, name);
    }

    if (!w)
        show_msg(i18n_str(STR_MSG_CREATE_FAIL));

    build_icons();
    desktop_redraw_all();
}

static void open_icon(const icon_t *ic)
{
    if (ic->type != FS_TYPE_FILE)
        return;
    open_file(ic->idx);
}

static void menu_execute(void)
{
    int item = menu_item_at(mouse_x, mouse_y);

    if (item < 0)
        return;

    if (menu_mode == 0) {
        if (item == 0)
            action_create_file();
        else if (item == 1) {
            fs_set_cwd(desk_idx);
            exit_desktop = 1;
        }
        else if (item == 4)
            shutdown_power();
    } else if (menu_mode == 1) {
        if (item == 0 && menu_target >= 0) {
            open_file(menu_target);
        } else if (item == 1 && menu_target >= 0) {
            fs_delete(menu_target);
            build_icons();
            paint_rect(0, TB_H, SCREEN_W, SCREEN_H - TB_H);
            selected_icon = -1;
        } else if (item == 2 && menu_target >= 0) {
            rename_dialog_open(menu_target);
        }
    } else if (menu_mode == 2) {
        if (item == 0 && menu_target >= 0)
            file_explorer_open_at(menu_target);
        else if (item == 1 && menu_target >= 0) {
            rename_dialog_open(menu_target);
        } else if (item == 3 && menu_target >= 0) {
            fs_delete(menu_target);
            build_icons();
            paint_rect(0, TB_H, SCREEN_W, SCREEN_H - TB_H);
            selected_icon = -1;
        }
    } else if (menu_mode == 3) {
        void (*pick)(int) = menu_pick;
        menu_pick = 0;
        menu_open = 0;
        if (pick)
            pick(item);
        paint_rect(0, TB_H, SCREEN_W, SCREEN_H - TB_H);
        return;
    }

    menu_open = 0;
    paint_rect(0, TB_H, SCREEN_W, SCREEN_H - TB_H);
}

static void open_menu(int x, int y)
{
    int h = icon_at(x, y);
    int i;

    menu_x = x;
    menu_y = y;
    menu_target = -1;
    menu_open = 1;
    menu_pick = 0;

    if (h >= 0) {
        selected_icon = h;
        menu_target = icons[h].idx;
        if (icons[h].type == FS_TYPE_DIR) {
            menu_mode = 2;
            menu_items = 4;
            for (i = 0; i < menu_items; i++) {
                strncpy(menu_text[i], i18n_str(m_dir[i]), 27);
                menu_text[i][27] = '\0';
            }
        } else {
            menu_mode = 1;
            menu_items = 4;
            for (i = 0; i < menu_items; i++) {
                strncpy(menu_text[i], i18n_str(m_file[i]), 27);
                menu_text[i][27] = '\0';
            }
        }
    } else {
        selected_icon = -1;
        menu_mode = 0;
        menu_items = 5;
        for (i = 0; i < menu_items; i++) {
            strncpy(menu_text[i], i18n_str(m_desktop[i]), 27);
            menu_text[i][27] = '\0';
        }
    }

    if (menu_x + MENU_W > SCREEN_W)
        menu_x = SCREEN_W - MENU_W;
    if (menu_y + menu_items * MENU_ITEM_H > SCREEN_H)
        menu_y = SCREEN_H - menu_items * MENU_ITEM_H;
    if (menu_x < 0) menu_x = 0;
    if (menu_y < TB_H) menu_y = TB_H;

    menu_hover = -1;
    paint_rect(0, TB_H, SCREEN_W, SCREEN_H - TB_H);
}

/* Open a generic (caller-defined) context menu at screen (x,y).  The menu
 * copies `labels` and invokes on_pick(item) when an entry is chosen.  This
 * lets other windows (e.g. the file explorer) reuse the global menu so it is
 * always painted on top of every window. */
void desktop_context_menu(int x, int y, const char *labels[], int n,
                          void (*on_pick)(int))
{
    int i;

    if (n < 1)
        return;
    if (n > MAX_MENU)
        n = MAX_MENU;

    menu_x = x;
    menu_y = y;
    menu_mode = 3;
    menu_target = -1;
    menu_items = n;
    menu_pick = on_pick;

    for (i = 0; i < n; i++) {
        strncpy(menu_text[i], labels[i], 27);
        menu_text[i][27] = '\0';
    }

    if (menu_x + MENU_W > SCREEN_W)
        menu_x = SCREEN_W - MENU_W;
    if (menu_y + menu_items * MENU_ITEM_H > SCREEN_H)
        menu_y = SCREEN_H - menu_items * MENU_ITEM_H;
    if (menu_x < 0) menu_x = 0;
    if (menu_y < TB_H) menu_y = TB_H;

    menu_hover = -1;
    menu_open = 1;
    paint_rect(0, TB_H, SCREEN_W, SCREEN_H - TB_H);
}

static void handle_left_release(void)
{
    int wi, h;
    window_t *w;

    if (menu_open) {
        if (menu_item_at(mouse_x, mouse_y) >= 0)
            menu_execute();
        else {
            menu_open = 0;
            paint_rect(0, TB_H, SCREEN_W, SCREEN_H - TB_H);
        }
        return;
    }

    wi = top_window_index(mouse_x, mouse_y);
    if (wi >= 0) {
        w = &windows[wi];
        if (window_in_close(w, mouse_x, mouse_y)) {
            int rx, ry, rw, rh;
            window_rect_with_shadow(w, &rx, &ry, &rw, &rh);
            window_close(w);
            selected_icon = -1;
            paint_rect(rx, ry, rw, rh);
            return;
        }
        if (mouse_y < w->y + TITLE_H) {
            if (w->on_title_press &&
                w->on_title_press(w, mouse_x - w->x, mouse_y - w->y)) {
                paint_rect(w->x, w->y, w->w + WIN_SHADOW, w->h + WIN_SHADOW);
                return;
            }
            bring_to_front(wi);
            paint_rect(0, 0, SCREEN_W, SCREEN_H);
            return;
        }
        if (w->on_click)
            w->on_click(w, mouse_x - w->x, mouse_y - (w->y + TITLE_H));
        if (windows[wi].visible) {
            paint_rect(w->x, w->y, w->w + WIN_SHADOW, w->h + WIN_SHADOW);
            bring_to_front(wi);
        }
        return;
    }

    h = icon_at(mouse_x, mouse_y);
    if (h >= 0) {
        if (h == selected_icon)
            open_icon(&icons[h]);
        else {
            selected_icon = h;
            paint_rect(0, TB_H, SCREEN_W, SCREEN_H - TB_H);
        }
        return;
    }

    /* EC-OS 按钮：弹出桌面菜单（含 Power off） */
    if (in_rect(mouse_x, mouse_y, 20, 8, 168, TB_H - 16)) {
        open_menu(20, TB_H);
        paint_rect(menu_x, menu_y, MENU_W, menu_items * MENU_ITEM_H);
        return;
    }

    if (selected_icon != -1) {
        selected_icon = -1;
        paint_rect(0, TB_H, SCREEN_W, SCREEN_H - TB_H);
    }
}

/* ------------------------------------------------------------------ */
/*  main loop                                                         */
/* ------------------------------------------------------------------ */
void desktop_run(void)
{
    int i;
    int last_cx = mouse_x, last_cy = mouse_y;

    mouse_init();
    i18n_init();
    desktop_setup();
    build_icons();

    for (i = 0; i < MAX_WINDOWS; i++)
        windows[i].visible = 0;

    selected_icon = -1;
    menu_open = 0;
    msg_ticks = 0;
    msg_repaint = 0;
    exit_desktop = 0;
    mb_left = 0;
    mb_right = 0;
    drag_win = -1;
    strcpy(clock_str, "--:--");

    paint_rect(0, 0, SCREEN_W, SCREEN_H);
    gfx_reset_clip();
    mouse_draw();
    gfx_flip();
    last_cx = mouse_x;
    last_cy = mouse_y;

    for (;;) {
        char nc[8];
        int key;

        /* ---- keyboard ---- */
        key = keyboard_poll();
        if (key != 0) {
            if (rename_dialog_is_open()) {
                rename_dialog_key(key);
            } else {
                window_t *kw = 0;
                for (i = MAX_WINDOWS - 1; i >= 0; i--) {
                    if (windows[i].visible && windows[i].on_key) {
                        kw = &windows[i];
                        break;
                    }
                }
                if (kw)
                    kw->on_key(kw, key);
                else if (minidos_is_open())
                    minidos_key(key);
                else if (key == 27) {
                    mouse_disable();
                    return;
                }
            }
        }

        /* ---- clock ---- */
        read_clock(nc);
        if (strcmp(nc, clock_str) != 0) {
            int cx, cy, cw, ch;
            strcpy(clock_str, nc);
            clock_rect(&cx, &cy, &cw, &ch);
            paint_rect(cx, cy, cw, ch);
        }

        /* ---- mouse buttons ---- */
        if (mouse_left && !mb_left) {
            int wi;
            mb_left = 1;
            press_x = mouse_x;
            press_y = mouse_y;
            was_drag = 0;

            if (rename_dialog_is_open()) {
                drag_win = -1;
            } else {
                wi = top_window_index(mouse_x, mouse_y);
                if (wi >= 0 && window_in_titlebar(&windows[wi], mouse_x, mouse_y) &&
                    !window_in_close(&windows[wi], mouse_x, mouse_y)) {
                    drag_win = wi;
                    drag_ox = mouse_x - windows[wi].x;
                    drag_oy = mouse_y - windows[wi].y;
                } else {
                    drag_win = -1;
                }
            }
        } else if (!mouse_left && mb_left) {
            mb_left = 0;
            drag_win = -1;
            if (!was_drag) {
                if (rename_dialog_is_open())
                    rename_dialog_click(mouse_x, mouse_y);
                else {
                    handle_left_release();
                    if (exit_desktop) {
                        mouse_disable();
                        return;
                    }
                }
            }
        }

        if (mouse_right && !mb_right) {
            mb_right = 1;
        } else if (!mouse_right && mb_right) {
            mb_right = 0;
            if (rename_dialog_is_open()) {
                /* modal rename dialog: swallow right-clicks */
            } else if (menu_open) {
                menu_open = 0;
                menu_pick = 0;
                paint_rect(0, TB_H, SCREEN_W, SCREEN_H - TB_H);
            } else {
                int wi = top_window_index(mouse_x, mouse_y);
                if (wi >= 0 && windows[wi].on_right_click) {
                    windows[wi].on_right_click(&windows[wi],
                        mouse_x - windows[wi].x,
                        mouse_y - (windows[wi].y + TITLE_H));
                } else {
                    open_menu(mouse_x, mouse_y);
                    paint_rect(menu_x, menu_y, MENU_W, menu_items * MENU_ITEM_H);
                }
            }
        }

        /* ---- window drag ---- */
        if (drag_win >= 0 && mb_left) {
            window_t *w = &windows[drag_win];
            if (w->visible && (mouse_x != last_cx || mouse_y != last_cy)) {
                int ox = w->x, oy = w->y;
                int nx = mouse_x - drag_ox;
                int ny = mouse_y - drag_oy;
                int ux0, uy0, ux1, uy1, rx, ry, rw, rh;

                if (nx < 0) nx = 0;
                if (ny < TB_H) ny = TB_H;
                if (nx + w->w > SCREEN_W) nx = SCREEN_W - w->w;
                if (ny + w->h > SCREEN_H) ny = SCREEN_H - w->h;

                was_drag = 1;
                w->x = nx;
                w->y = ny;

                window_rect_with_shadow(w, &rx, &ry, &rw, &rh);
                ux0 = ox; uy0 = oy; ux1 = ox + w->w + WIN_SHADOW; uy1 = oy + w->h + WIN_SHADOW;
                if (rx < ux0) ux0 = rx;
                if (ry < uy0) uy0 = ry;
                if (rx + rw > ux1) ux1 = rx + rw;
                if (ry + rh > uy1) uy1 = ry + rh;
                paint_rect(ux0, uy0, ux1 - ux0, uy1 - uy0);
            }
        } else if (!mb_left) {
            drag_win = -1;
        }

        /* ---- menu hover ---- */
        if (menu_open) {
            int hov = menu_item_at(mouse_x, mouse_y);
            if (hov != menu_hover) {
                menu_hover = hov;
                paint_rect(menu_x, menu_y, MENU_W, menu_items * MENU_ITEM_H);
            }
        }

        /* ---- message toast ---- */
        if (msg_ticks > 0) {
            int tx, ty, tw, th;
            msg_ticks--;
            toast_rect(&tx, &ty, &tw, &th);
            paint_rect(tx, ty, tw, th);
        } else if (msg_repaint) {
            int tx, ty, tw, th;
            toast_rect(&tx, &ty, &tw, &th);
            msg_repaint = 0;
            paint_rect(tx, ty, tw, th);
        }

        /* ---- cursor ---- */
        if (mouse_x != last_cx || mouse_y != last_cy) {
            paint_rect(last_cx - 2, last_cy - 2, MOUSE_W + MOUSE_SHADOW + 4,
                       MOUSE_H + MOUSE_SHADOW + 4);
            paint_rect(mouse_x - 2, mouse_y - 2, MOUSE_W + MOUSE_SHADOW + 4,
                       MOUSE_H + MOUSE_SHADOW + 4);
            last_cx = mouse_x;
            last_cy = mouse_y;
        }

        gfx_reset_clip();
        mouse_draw();
        gfx_flip();
    }
}
