#ifndef EC_WINDOW_H
#define EC_WINDOW_H

#include <stdint.h>
#include "gfx.h"

#define MAX_WINDOWS  8
#define TITLE_H      44
#define WIN_RADIUS   10
#define WIN_SHADOW   8

/* title-bar Save button geometry (window-local coordinates) */
#define SAVE_BTN_X   96
#define SAVE_BTN_Y   9
#define SAVE_BTN_W   72
#define SAVE_BTN_H   26

typedef struct window window_t;

struct window {
    int      x, y, w, h;
    char     title[64];
    int      visible;
    int      active;
    color_t  content;
    void   (*on_draw)(window_t *self);
    void   (*on_click)(window_t *self, int mx, int my);
    void   (*on_key)(window_t *self, int key);
    void   (*on_title)(window_t *self);
    int    (*on_title_press)(window_t *self, int mx, int my);
    void   (*on_right_click)(window_t *self, int mx, int my);
    int      id;
};

extern window_t windows[MAX_WINDOWS];

window_t *window_create(int x, int y, int w, int h, const char *title,
                        color_t content,
                        void (*on_draw)(window_t *),
                        void (*on_click)(window_t *, int, int),
                        int id);
void window_draw(window_t *w);
void window_close(window_t *w);
int  window_in_titlebar(window_t *w, int x, int y);
int  window_in_close(window_t *w, int x, int y);
void window_rect_with_shadow(window_t *w, int *rx, int *ry, int *rw, int *rh);

#endif
