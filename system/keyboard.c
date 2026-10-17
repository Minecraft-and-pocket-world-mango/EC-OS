#include "keyboard.h"
#include "mouse.h"
#include "io.h"

#define KBD_STATUS  0x64
#define KBD_DATA    0x60

#define SC_LSHIFT   0x2A
#define SC_RSHIFT   0x36
#define SC_LSHIFT_UP 0xAA
#define SC_RSHIFT_UP 0xB6
#define SC_ESC      0x01

static const char sc_lower[128] = {
    [2] = '1', [3] = '2', [4] = '3', [5] = '4', [6] = '5',
    [7] = '6', [8] = '7', [9] = '8', [10] = '9', [11] = '0',
    [12] = '-', [13] = '=',
    [14] = '\b', [15] = '\t',
    [16] = 'q', [17] = 'w', [18] = 'e', [19] = 'r', [20] = 't',
    [21] = 'y', [22] = 'u', [23] = 'i', [24] = 'o', [25] = 'p',
    [26] = '[', [27] = ']',
    [28] = '\n',
    [30] = 'a', [31] = 's', [32] = 'd', [33] = 'f', [34] = 'g',
    [35] = 'h', [36] = 'j', [37] = 'k', [38] = 'l', [39] = ';',
    [40] = '\'', [41] = '`',
    [43] = '\\',
    [44] = 'z', [45] = 'x', [46] = 'c', [47] = 'v', [48] = 'b',
    [49] = 'n', [50] = 'm', [51] = ',', [52] = '.', [53] = '/',
    [55] = '*', [57] = ' '
};

static const char sc_upper[128] = {
    [2] = '!', [3] = '@', [4] = '#', [5] = '$', [6] = '%',
    [7] = '^', [8] = '&', [9] = '*', [10] = '(', [11] = ')',
    [12] = '_', [13] = '+',
    [14] = '\b', [15] = '\t',
    [16] = 'Q', [17] = 'W', [18] = 'E', [19] = 'R', [20] = 'T',
    [21] = 'Y', [22] = 'U', [23] = 'I', [24] = 'O', [25] = 'P',
    [26] = '{', [27] = '}',
    [28] = '\n',
    [30] = 'A', [31] = 'S', [32] = 'D', [33] = 'F', [34] = 'G',
    [35] = 'H', [36] = 'J', [37] = 'K', [38] = 'L', [39] = ':',
    [40] = '"', [41] = '~',
    [43] = '|',
    [44] = 'Z', [45] = 'X', [46] = 'C', [47] = 'V', [48] = 'B',
    [49] = 'N', [50] = 'M', [51] = '<', [52] = '>', [53] = '?',
    [55] = '*', [57] = ' '
};

static int shift = 0;

static int translate(unsigned char sc)
{
    char c;

    if (sc == 0x2A || sc == 0x36) {
        shift = 1;
        return 0;
    }
    if (sc == 0xAA || sc == 0xB6) {
        shift = 0;
        return 0;
    }
    if (sc == SC_ESC)
        return 27;
    if (sc & 0x80)
        return 0;

    c = shift ? sc_upper[sc] : sc_lower[sc];
    return (int)(unsigned char)c;
}

char keyboard_getchar(void)
{
    for (;;) {
        uint8_t st;
        int key;

        while (((st = inb(KBD_STATUS)) & 0x01) == 0)
            ;

        if (st & 0x20) {
            mouse_byte(inb(KBD_DATA));
            continue;
        }

        key = translate(inb(KBD_DATA));
        if (key != 0)
            return (char)key;
    }
}

int keyboard_poll(void)
{
    int result = 0;
    uint8_t st;

    while ((st = inb(KBD_STATUS)) & 0x01) {
        uint8_t data = inb(KBD_DATA);

        if (st & 0x20) {
            mouse_byte(data);
            continue;
        }

        if (result == 0) {
            int key = translate(data);
            if (key != 0)
                result = key;
        }
    }
    return result;
}

void keyboard_flush(void)
{
    int i;

    for (i = 0; i < 64; i++) {
        if ((inb(KBD_STATUS) & 0x01) == 0)
            break;
        inb(KBD_DATA);
    }
    shift = 0;
}