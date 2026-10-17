#include "minidos.h"
#include "window.h"
#include "gfx.h"
#include "fs.h"
#include "lib.h"
#include "desktop.h"
#include "i18n.h"

#define MD_BG     RGB(0x0C,0x10,0x16)
#define MD_COLS   56
#define MD_ROWS   18

static window_t *md_win = 0;
static char mdbuf[MD_ROWS][MD_COLS];
static int  md_lines = 0;
static char md_input[MD_COLS];
static int  md_n = 0;

static window_t *md_find(void)
{
    int i;
    for (i = 0; i < MAX_WINDOWS; i++)
        if (windows[i].visible && windows[i].id == 2)
            return &windows[i];
    return 0;
}

static void md_draw(window_t *w)
{
    char scratch[MD_COLS + 4];
    char *p;
    int i, n = 0;

    for (i = 0; i < md_lines; i++)
        gfx_draw_string(w->x + 14, w->y + TITLE_H + 8 + i * FONT_H,
                        mdbuf[i], COL_TEXT, GFX_NO_BG);

    scratch[0] = '>';
    scratch[1] = ' ';
    p = md_input;
    while (n < MD_COLS - 2 && *p) {
        scratch[2 + n] = *p++;
        n++;
    }
    scratch[2 + n] = '\0';

    gfx_draw_string(w->x + 14, w->y + TITLE_H + 8 + md_lines * FONT_H,
                    scratch, COL_ACCENT2, GFX_NO_BG);
}

static void md_add(const char *s)
{
    int i, n = 0;

    if (md_lines == MD_ROWS - 1) {
        for (i = 0; i < MD_ROWS - 2; i++)
            memcpy(mdbuf[i], mdbuf[i + 1], MD_COLS);
        md_lines--;
    }

    while (n < MD_COLS - 1 && s[n]) {
        mdbuf[md_lines][n] = s[n];
        n++;
    }
    mdbuf[md_lines][n] = '\0';
    md_lines++;
}

static void md_print(const char *s) { md_add(s); }

static void md_exec(char *cmd)
{
    char tok[16];
    char *rest;
    int i;

    if (cmd[0] == '\0')
        return;

    while (*cmd == ' ')
        cmd++;
    i = 0;
    while (*cmd && *cmd != ' ' && i < 15)
        tok[i++] = *cmd++;
    tok[i] = '\0';
    while (*cmd == ' ')
        cmd++;
    rest = cmd;

    if (strcmp(tok, "exit") == 0) {
        if (md_win)
            window_close(md_win);
        return;
    }
    if (strcmp(tok, "cls") == 0) {
        md_lines = 0;
        return;
    }
    if (strcmp(tok, "output") == 0 || strcmp(tok, "echo") == 0) {
        md_print(rest);
        return;
    }
    if (strcmp(tok, "go") == 0) {
        int idx;
        if (*rest == '\0' || strcmp(rest, "~") == 0) {
            fs_set_cwd(0);
            md_print("OK");
            return;
        }
        idx = fs_resolve(rest);
        if (idx < 0 || fs_nodes[idx].type != FS_TYPE_DIR) {
            md_print("Not found");
            return;
        }
        fs_set_cwd(idx);
        md_print("OK");
        return;
    }
    if (strcmp(tok, "show") == 0) {
        int idx = *rest == '\0' ? fs_cwd() : fs_resolve(rest);
        fs_node_t *d;
        if (idx < 0 || fs_nodes[idx].type != FS_TYPE_DIR) {
            md_print("Not found");
            return;
        }
        d = &fs_nodes[idx];
        for (i = 0; i < d->child_count; i++) {
            char label[MAX_NAME + 8];
            int ci = d->children[i];
            const char *nm;
            int k = 0;
            if (ci < 0 || ci >= MAX_NODES)
                continue;
            label[k++] = '[';
            label[k++] = (fs_nodes[ci].type == FS_TYPE_DIR) ? 'D' : 'F';
            label[k++] = ']';
            label[k++] = ' ';
            nm = fs_nodes[ci].name;
            while (*nm && k < (int)sizeof(label) - 1)
                label[k++] = *nm++;
            label[k] = '\0';
            md_print(label);
        }
        return;
    }
    if (strcmp(tok, "ver") == 0) {
        md_print("EC-OS MiniDOS 26Y10B");
        return;
    }
    if (strcmp(tok, "birthday") == 0) {
        md_print("It's the author's birthday on April 26, 2015.");
        md_print("Just imagine, how young is the author to have");
        md_print("developed EC-OS? What else can't you accomplish?");
        md_print("Come on! I have high hopes for you~");
        return;
    }

    md_print("Unknown command");
}

void minidos_open(void)
{
    if (md_find())
        return;

    md_win = window_create((SCREEN_W - 960) / 2, (SCREEN_H - 660) / 2,
                           960, 660, i18n_str(STR_TITLE_DOS), MD_BG, md_draw, 0, 2);
    if (!md_win)
        return;

    md_lines = 0;
    md_n = 0;
    md_input[0] = '\0';

    md_print("EC-OS MiniDOS 26Y10B");
    md_print("Type 'exit' to close.");

    /* 内容初始化后重绘，保证新窗口立即显示正确内容 */
    desktop_redraw_all();
}

int minidos_is_open(void)
{
    return md_find() != 0;
}

void minidos_exec(const char *cmd)
{
    char buf[MD_COLS];

    if (!md_find())
        minidos_open();
    if (!md_find())
        return;

    md_add(cmd);
    strncpy(buf, cmd, MD_COLS - 1);
    buf[MD_COLS - 1] = '\0';
    md_exec(buf);
    desktop_redraw_all();
}

void minidos_key(int key)
{
    window_t *w;
    char c;

    w = md_find();
    if (!w)
        return;
    md_win = w;

    if (key == 27) {
        window_close(w);
        desktop_redraw_all();
        return;
    }
    if (key <= 0)
        return;

    c = (char)key;

    if (c == '\n') {
        md_input[md_n] = '\0';
        md_add(md_input);
        md_exec(md_input);
        md_input[0] = '\0';
        md_n = 0;
    } else if (c == '\b') {
        if (md_n > 0) {
            md_n--;
            md_input[md_n] = '\0';
        }
    } else if (c >= 32 && c < 127) {
        if (md_n < MD_COLS - 2) {
            md_input[md_n++] = c;
            md_input[md_n] = '\0';
        }
    }

    /* 按键后立即重绘，保证输入与 'exit' 关闭窗口都能及时反映 */
    desktop_redraw_all();
}
