#ifndef EC_VGA_H
#define EC_VGA_H

void vga_clear(void);
void vga_putchar(char c);
void vga_print(const char *s);
void vga_backspace(void);

#endif