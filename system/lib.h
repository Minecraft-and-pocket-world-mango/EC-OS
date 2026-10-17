#ifndef EC_LIB_H
#define EC_LIB_H

#include <stddef.h>
#include <stdint.h>

void  *memset(void *s, int c, size_t n);
void  *memcpy(void *dest, const void *src, size_t n);
void  *memmove(void *dest, const void *src, size_t n);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *dest, const char *src);
char  *strncpy(char *dest, const char *src, size_t n);
size_t strlen(const char *s);

/* decimal helpers (no floating point) */
int    itoa_u32(uint32_t v, char *out);      /* writes NUL, returns length */
int    itoa_u32_pad(uint32_t v, int width, char pad, char *out);

#endif
