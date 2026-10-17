#include "settings.h"
#include "window.h"
#include "gfx.h"
#include "i18n.h"
#include "desktop.h"
#include "lib.h"

#define SET_W        760
#define SET_H        560

#define GX           24
#define GY           56
#define CELL_W       ((SET_W - 48 - 16) / 2)
#define CELL_H       60
#define CELL_GAP     8

#define CB_X         24
#define CB_Y         424
#define CB_BOX       30

#define COL_CELL_BG  RGB(0x22,0x27,0x33)
#define COL_CELL_BG2 RGB(0x2C,0x33,0x42)

static window_t *settings_win = 0;
static int autostart = 0;

int settings_autostart(void)
{
    return autostart;
}

static void refresh_titles(void)
{
    int i;
    for (i = 0; i < MAX_WINDOWS; i++) {
        if (!windows[i].visible)
            continue;
        if (windows[i].id == 1)
            strncpy(windows[i].title, i18n_str(STR_TITLE_FILES),
                    sizeof(windows[i].title) - 1);
        else if (windows[i].id == 2)
            strncpy(windows[i].title, i18n_str(STR_TITLE_DOS),
                    sizeof(windows[i].title) - 1);
        else if (windows[i].id == 3)
            strncpy(windows[i].title, i18n_str(STR_TITLE_SETTINGS),
                    sizeof(windows[i].title) - 1);
        windows[i].title[sizeof(windows[i].title) - 1] = '\0';
    }
}

static void cell_rect(int idx, int *x, int *y)
{
    int col = idx % 2;
    int row = idx / 2;
    *x = GX + col * (CELL_W + 16);
    *y = GY + row * (CELL_H + CELL_GAP);
}

static void set_draw(window_t *w)
{
    int i;
    int bx = w->x, by = w->y + TITLE_H;

    gfx_draw_string(bx + GX, by + 14, i18n_str(STR_SET_LANGUAGE),
                    COL_TEXT_DIM, GFX_NO_BG);

    for (i = 0; i < LANG_COUNT; i++) {
        int x, y;
        color_t bg;
        color_t fg;

        cell_rect(i, &x, &y);
        bg = (i == (int)i18n_get_lang()) ? COL_ACCENT2 : COL_CELL_BG;

        gfx_fill_round(bx + x, by + y, CELL_W, CELL_H, 10, bg);
        gfx_fill_round(bx + x + 2, by + y + 2, CELL_W - 4, CELL_H - 4, 9,
                       (i == (int)i18n_get_lang()) ? COL_ACCENT : COL_CELL_BG2);

        fg = (i == (int)i18n_get_lang()) ? COL_WHITE : COL_TEXT;
        gfx_draw_string(bx + x + 18, by + y + (CELL_H - FONT_H) / 2,
                        i18n_lang_name((lang_t)i), fg, GFX_NO_BG);
    }

    gfx_fill_round(bx + CB_X, by + CB_Y, CB_BOX, CB_BOX, 7,
                   autostart ? COL_ACCENT : COL_CELL_BG);
    gfx_outline_round(bx + CB_X, by + CB_Y, CB_BOX, CB_BOX, 7, COL_BORDER_IN);
    if (autostart) {
        gfx_draw_string(bx + CB_X + 4, by + CB_Y - 2, "x", COL_WHITE, GFX_NO_BG);
    }
    gfx_draw_string(bx + CB_X + CB_BOX + 16,
                    by + CB_Y + (CB_BOX - FONT_H) / 2,
                    i18n_str(STR_SET_AUTOSTART), COL_TEXT, GFX_NO_BG);
}

static void set_click(window_t *w, int mx, int my)
{
    int i;

    (void)w;

    for (i = 0; i < LANG_COUNT; i++) {
        int x, y;
        cell_rect(i, &x, &y);
        if (mx >= x && mx < x + CELL_W && my >= y && my < y + CELL_H) {
            i18n_set_lang((lang_t)i);
            refresh_titles();
            desktop_redraw_all();
            return;
        }
    }

    if (mx >= CB_X && mx < CB_X + CB_BOX + 16 + gfx_text_width(i18n_str(STR_SET_AUTOSTART)) &&
        my >= CB_Y && my < CB_Y + CB_BOX) {
        autostart = !autostart;
        desktop_redraw_all();
        return;
    }
}

void settings_open(void)
{
    if (settings_win && settings_win->visible && settings_win->id == 3)
        return;

    settings_win = window_create((SCREEN_W - SET_W) / 2, (SCREEN_H - SET_H) / 2,
                                 SET_W, SET_H, i18n_str(STR_TITLE_SETTINGS),
                                 RGB(0x16,0x1A,0x22), set_draw, set_click, 3);
}
