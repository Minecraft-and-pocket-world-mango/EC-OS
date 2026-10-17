#ifndef EC_DESKTOP_H
#define EC_DESKTOP_H

void desktop_run(void);
void desktop_redraw_all(void);
void desktop_refresh_icons(void);
void desktop_toast(const char *s);
void desktop_context_menu(int x, int y, const char *labels[], int n,
                          void (*on_pick)(int));

#endif