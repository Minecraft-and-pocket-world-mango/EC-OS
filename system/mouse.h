#ifndef EC_MOUSE_H
#define EC_MOUSE_H

#include <stdint.h>

#define MOUSE_W       13
#define MOUSE_H       20
#define MOUSE_SHADOW  2

extern int mouse_x;
extern int mouse_y;
extern int mouse_left;
extern int mouse_right;

void mouse_init(void);
void mouse_byte(uint8_t b);
void mouse_disable(void);
void mouse_draw(void);          /* composite the cursor into the back buffer */

#endif
