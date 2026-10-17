#ifndef EC_RENAME_DIALOG_H
#define EC_RENAME_DIALOG_H

/* Self-drawn modal "Rename" dialog.  It is an overlay composited by the
 * desktop on top of every window (not a window_t), so while it is open the
 * desktop must route keyboard and mouse events here first. */

void rename_dialog_open(int fs_idx);
int  rename_dialog_is_open(void);
void rename_dialog_key(int key);
void rename_dialog_click(int mx, int my);
void rename_dialog_draw(void);

#endif
