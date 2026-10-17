#include "filetype.h"
#include "lib.h"

int is_binary(const char *data, int len)
{
    int n = len > 512 ? 512 : len;
    int i;

    if (n <= 0)
        return 0;

    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)data[i];
        if (c == 0)
            break;
        if (c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0D)
            return 1;
    }
    return 0;
}

int ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s);
    size_t m = strlen(suffix);

    if (m > n)
        return 0;
    return strcmp(s + (n - m), suffix) == 0;
}
