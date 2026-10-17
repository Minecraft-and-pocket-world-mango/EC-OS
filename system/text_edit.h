#ifndef EC_TEXT_EDIT_H
#define EC_TEXT_EDIT_H

#include "window.h"

window_t *text_edit_open(int fs_idx);
window_t *text_edit_open_new(int parent, const char *name);
int       text_edit_is_open(void);
void      warn_dialog_open(int fs_idx);

#endif
