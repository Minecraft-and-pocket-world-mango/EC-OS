#include "mouse.h"
#include "io.h"
#include "gfx.h"

#define MOUSE_STATUS  0x64
#define MOUSE_DATA    0x60
#define IO_TIMEOUT    100000

int mouse_x = SCREEN_W / 2;
int mouse_y = SCREEN_H / 2;
int mouse_left = 0;
int mouse_right = 0;

static int mouse_active = 0;
static int packet[3];
static int packet_index = 0;
static int mouse_fx = 0;      /* 8.8 fixed point */
static int mouse_fy = 0;

/* '#' outline, 'w' fill, '.' transparent */
static const char *cursor_art[MOUSE_H] = {
    "#............",
    "##...........",
    "#w#..........",
    "#ww#.........",
    "#www#........",
    "#wwww#.......",
    "#wwwww#......",
    "#wwwwww#.....",
    "#wwwwwww#....",
    "#wwwwwwww#...",
    "#wwwwwwwww#..",
    "#wwwwww####..",
    "#www#ww#.....",
    "#ww#.#ww#....",
    "#w#..#ww#....",
    "##...#ww#....",
    ".#...#ww#....",
    ".....#ww#....",
    "......##.....",
    ".............",
};

static int wait_write(void)
{
    int i;
    for (i = 0; i < IO_TIMEOUT; i++)
        if ((inb(MOUSE_STATUS) & 0x02) == 0)
            return 1;
    return 0;
}

static int wait_read(void)
{
    int i;
    for (i = 0; i < IO_TIMEOUT; i++)
        if (inb(MOUSE_STATUS) & 0x01)
            return 1;
    return 0;
}

static int mouse_cmd(uint8_t cmd)
{
    if (!wait_write())
        return 0;
    outb(MOUSE_STATUS, 0xD4);
    if (!wait_write())
        return 0;
    outb(MOUSE_DATA, cmd);
    if (!wait_read())
        return 0;
    inb(MOUSE_DATA);
    return 1;
}

void mouse_init(void)
{
    mouse_active = 0;
    packet_index = 0;

    if (!wait_write())
        return;
    outb(MOUSE_STATUS, 0xA8);          /* enable aux port (no ACK) */

    while (inb(MOUSE_STATUS) & 0x01)
        inb(MOUSE_DATA);

    if (!mouse_cmd(0xF6))              /* set defaults */
        return;
    if (!mouse_cmd(0xF4))              /* enable reporting */
        return;

    packet_index = 0;
    mouse_x = SCREEN_W / 2;
    mouse_y = SCREEN_H / 2;
    mouse_fx = mouse_x << 8;
    mouse_fy = mouse_y << 8;
    mouse_left = 0;
    mouse_right = 0;
    mouse_active = 1;
}

void mouse_disable(void)
{
    if (!mouse_active)
        return;
    mouse_cmd(0xF5);
    mouse_active = 0;
}

void mouse_byte(uint8_t b)
{
    int dx, dy;

    if (!mouse_active)
        return;

    if (packet_index == 0) {
        if ((b & 0x08) == 0)
            return;
        packet[0] = b;
        packet_index = 1;
        return;
    }

    packet[packet_index] = (int)b;
    packet_index++;
    if (packet_index < 3)
        return;
    packet_index = 0;

    mouse_left = packet[0] & 0x01;
    mouse_right = (packet[0] >> 1) & 0x01;

    dx = (int)(int8_t)packet[1];
    dy = (int)(int8_t)packet[2];

    /* gain x2, 8.8 fixed point, no floating point */
    mouse_fx += dx << 9;
    mouse_fy -= dy << 9;

    mouse_x = mouse_fx >> 8;
    mouse_y = mouse_fy >> 8;

    if (mouse_x < 0) { mouse_x = 0; mouse_fx = 0; }
    if (mouse_y < 0) { mouse_y = 0; mouse_fy = 0; }
    if (mouse_x >= SCREEN_W) { mouse_x = SCREEN_W - 1; mouse_fx = mouse_x << 8; }
    if (mouse_y >= SCREEN_H) { mouse_y = SCREEN_H - 1; mouse_fy = mouse_y << 8; }
}

void mouse_draw(void)
{
    int row, col;

    /* soft shadow */
    for (row = 0; row < MOUSE_H; row++) {
        const char *p = cursor_art[row];
        for (col = 0; col < MOUSE_W; col++) {
            char ch = p[col];
            if (ch == '.' || ch == '\0')
                continue;
            gfx_blend(mouse_x + col + MOUSE_SHADOW,
                      mouse_y + row + MOUSE_SHADOW, COL_SHADOW, 120);
        }
    }

    /* arrow */
    for (row = 0; row < MOUSE_H; row++) {
        const char *p = cursor_art[row];
        for (col = 0; col < MOUSE_W; col++) {
            char ch = p[col];
            if (ch == '#')
                gfx_pixel(mouse_x + col, mouse_y + row, COL_BLACK);
            else if (ch == 'w')
                gfx_pixel(mouse_x + col, mouse_y + row, COL_WHITE);
        }
    }
}
