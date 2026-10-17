#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "fs.h"
#include "lib.h"
#include "desktop.h"
#include "vbe.h"
#include "gfx.h"

#define LINE_MAX    128
#define TOKEN_MAX   8

static char  line_buf[LINE_MAX];
static char *tokens[TOKEN_MAX];
static int   token_count = 0;

static void read_line(void)
{
    int len = 0;

    for (;;) {
        char c = keyboard_getchar();

        if (c == '\n') {
            line_buf[len] = '\0';
            vga_putchar('\n');
            return;
        }
        if (c == '\b') {
            if (len > 0) {
                len--;
                vga_backspace();
            }
            continue;
        }
        if (len < LINE_MAX - 1) {
            line_buf[len++] = c;
            vga_putchar(c);
        }
    }
}

static int tokenize(char *s)
{
    int n = 0;
    char *p = s;

    while (*p != '\0') {
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0')
            break;
        if (n < TOKEN_MAX)
            tokens[n++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t')
            p++;
        if (*p != '\0')
            *p++ = '\0';
    }
    return n;
}

static void show_tree(int idx, int depth)
{
    int i;
    fs_node_t *n;

    if (idx < 0 || idx >= MAX_NODES)
        return;

    n = &fs_nodes[idx];

    for (i = 0; i < depth * 2; i++)
        vga_putchar(' ');

    vga_print(n->name);
    vga_putchar('\n');

    if (n->type == FS_TYPE_DIR)
        for (i = 0; i < n->child_count; i++)
            show_tree(n->children[i], depth + 1);
}

static void cmd_output(void)
{
    int i;
    for (i = 1; i < token_count; i++) {
        if (i > 1)
            vga_putchar(' ');
        vga_print(tokens[i]);
    }
    vga_putchar('\n');
}

static void cmd_show(void)
{
    int idx = (token_count >= 2) ? fs_resolve(tokens[1]) : fs_cwd();
    if (idx < 0) {
        vga_print("Path not found.\n");
        return;
    }
    show_tree(idx, 0);
    vga_putchar('\n');
}

static void cmd_open(void)
{
    int idx;
    if (token_count < 2) {
        vga_print("File not found.\n");
        return;
    }
    idx = fs_resolve(tokens[1]);
    if (idx < 0 || fs_nodes[idx].type != FS_TYPE_FILE) {
        vga_print("File not found.\n");
        return;
    }
    vga_print(fs_nodes[idx].data);
    vga_putchar('\n');
}

static void cmd_create(void)
{
    uint8_t type;
    int parent;
    const char *name;

    if (token_count < 3) {
        vga_print("Create failed.\n");
        return;
    }
    if (tokens[1][0] == 'f')
        type = FS_TYPE_FILE;
    else if (tokens[1][0] == 'p')
        type = FS_TYPE_DIR;
    else {
        vga_print("Create failed.\n");
        return;
    }

    if (token_count >= 4) {
        parent = fs_resolve(tokens[2]);
        name = tokens[3];
    } else {
        parent = fs_cwd();
        name = tokens[2];
    }

    if (parent < 0 || fs_create(parent, name, type) < 0)
        vga_print("Create failed.\n");
}

static void cmd_go(void)
{
    int idx;
    if (token_count < 2 || strcmp(tokens[1], "~") == 0) {
        fs_set_cwd(0);
        return;
    }
    idx = fs_resolve(tokens[1]);
    if (idx < 0 || fs_nodes[idx].type != FS_TYPE_DIR) {
        vga_print("Path not found.\n");
        return;
    }
    fs_set_cwd(idx);
}

static void cmd_write(void)
{
    int idx;
    char *dst;
    size_t len = 0;
    int i;

    if (token_count < 3) {
        vga_print("Write failed.\n");
        return;
    }
    idx = fs_resolve(tokens[1]);
    if (idx < 0 || fs_nodes[idx].type != FS_TYPE_FILE) {
        vga_print("File not found.\n");
        return;
    }

    dst = fs_nodes[idx].data;
    dst[0] = '\0';

    for (i = 2; i < token_count; i++) {
        const char *tok = tokens[i];
        size_t tlen = strlen(tok);
        size_t j;

        if (i > 2) {
            if (len >= NODE_DATA_SIZE - 1)
                break;
            dst[len++] = ' ';
        }
        if (tlen > NODE_DATA_SIZE - 1 - len)
            tlen = NODE_DATA_SIZE - 1 - len;
        for (j = 0; j < tlen; j++)
            dst[len + j] = tok[j];
        len += tlen;
        dst[len] = '\0';
    }

    fs_flush_to_disk();
}

static void cmd_del(void)
{
    int idx;
    if (token_count < 2) {
        vga_print("Delete failed.\n");
        return;
    }
    idx = fs_resolve(tokens[1]);
    if (idx < 0 || fs_delete(idx) < 0) {
        vga_print("Delete failed.\n");
        return;
    }
    vga_print("Deleted.\n");
}

static void cmd_rename(void)
{
    int idx;
    if (token_count < 3) {
        vga_print("Rename failed.\n");
        return;
    }
    idx = fs_resolve(tokens[1]);
    if (idx < 0 || fs_rename(idx, tokens[2]) < 0) {
        vga_print("Rename failed.\n");
        return;
    }
    vga_print("Renamed.\n");
}

static void cmd_copy(void)
{
    int idx;
    if (token_count < 3) {
        vga_print("Copy failed.\n");
        return;
    }
    idx = fs_resolve(tokens[1]);
    if (idx < 0 || fs_copy(idx, fs_cwd(), tokens[2]) < 0) {
        vga_print("Copy failed.\n");
        return;
    }
    vga_print("Copied.\n");
}

static void cmd_pwd(void)
{
    int stack[MAX_NODES];
    int depth = 0;
    int idx = fs_cwd();

    while (idx >= 0 && fs_nodes[idx].parent >= 0) {
        stack[depth++] = idx;
        idx = fs_nodes[idx].parent;
    }

    if (depth == 0) {
        vga_print("P:/\n");
        return;
    }

    vga_print("P:");
    while (depth > 0) {
        vga_putchar('/');
        vga_print(fs_nodes[stack[--depth]].name);
    }
    vga_putchar('\n');
}

static void cmd_ver(void)
{
    char buf[8];

    vga_print("EC-OS 26Y10B\n");
    vga_print("(c) EasyCloud Studio\n");
    vga_print("NASM + i686-elf-gcc, VBE ");
    itoa_u32(SCREEN_W, buf); vga_print(buf);
    vga_print("x");
    itoa_u32(SCREEN_H, buf); vga_print(buf);
    vga_print("x32\n");
}

static void cmd_sysinfo(void)
{
    char buf[16];

    vga_print("EC-OS System Information\n");
    vga_print("--------------------------\n");
    vga_print(" Name     : EC-OS\n");
    vga_print(" Version  : 26Y10B\n");
    vga_print(" CPU      : i686 (x86, 32-bit)\n");
    vga_print(" Display  : ");
    itoa_u32(SCREEN_W, buf); vga_print(buf);
    vga_print(" x ");
    itoa_u32(SCREEN_H, buf); vga_print(buf);
    vga_print(" x 32bpp\n");
    vga_print(" RAM      : 1024 MB\n");
    vga_print(" Disk     : ");
    itoa_u32((uint32_t)fs_capacity_mb(), buf);
    vga_print(buf);
    vga_print(" MB\n");
    vga_print(" FS nodes : ");
    itoa_u32(MAX_NODES, buf);
    vga_print(buf);
    vga_putchar('\n');
    vga_print(" Detected : ");
    itoa_u32(vbe_detected_ram_mb(), buf);
    vga_print(buf);
    vga_print(" MB RAM\n");
    vga_print(" FB pitch : ");
    itoa_u32(vbe_pitch(), buf);
    vga_print(buf);
    vga_print(" bytes\n");
}

static void cmd_birthday(void)
{
    vga_print("It's the author's birthday on April 26, 2015.\n");
    vga_print("Just imagine, how young is the author to have\n");
    vga_print("developed EC-OS? What else can't you accomplish?\n");
    vga_print("Come on! I have high hopes for you~\n");
}

static void cmd_help(void)
{
    vga_print("EC-OS Commands\n");
    vga_print(" output <text>\n");
    vga_print(" echo <text>\n");
    vga_print(" show [path]\n");
    vga_print(" open <path>\n");
    vga_print(" create <f|p> [dir] <name>\n");
    vga_print(" write <path> <text>\n");
    vga_print(" del <path>\n");
    vga_print(" rename <old> <new>\n");
    vga_print(" copy <src> <dst>\n");
    vga_print(" go [path]\n");
    vga_print(" pwd\n");
    vga_print(" ver\n");
    vga_print(" sysinfo\n");
    vga_print(" cls\n");
    vga_print(" help\n");
    vga_print(" birthday\n");
    vga_print(" desktop\n");
}

static void cmd_desktop(void)
{
    vga_print("Entering Desktop...\n");
    desktop_run();
    vga_clear();
    keyboard_flush();
    vga_print("EC-OS 26Y10B (C kernel)\n");
    vga_print("Commands: help | sysinfo | desktop\n");
    vga_putchar('\n');
}

void shell_run(void)
{
    vga_print("EC-OS 26Y10B (C kernel)\n");
    vga_print("(c) EasyCloud Studio\n");
    vga_print("Commands: help | sysinfo | desktop\n");
    vga_putchar('\n');

    for (;;) {
        vga_print("P:/> ");
        read_line();
        token_count = tokenize(line_buf);

        if (token_count == 0)
            continue;

        if (strcmp(tokens[0], "output") == 0)
            cmd_output();
        else if (strcmp(tokens[0], "echo") == 0)
            cmd_output();
        else if (strcmp(tokens[0], "show") == 0)
            cmd_show();
        else if (strcmp(tokens[0], "open") == 0)
            cmd_open();
        else if (strcmp(tokens[0], "create") == 0)
            cmd_create();
        else if (strcmp(tokens[0], "write") == 0)
            cmd_write();
        else if (strcmp(tokens[0], "del") == 0)
            cmd_del();
        else if (strcmp(tokens[0], "rename") == 0)
            cmd_rename();
        else if (strcmp(tokens[0], "copy") == 0)
            cmd_copy();
        else if (strcmp(tokens[0], "go") == 0)
            cmd_go();
        else if (strcmp(tokens[0], "pwd") == 0)
            cmd_pwd();
        else if (strcmp(tokens[0], "ver") == 0)
            cmd_ver();
        else if (strcmp(tokens[0], "sysinfo") == 0)
            cmd_sysinfo();
        else if (strcmp(tokens[0], "cls") == 0)
            vga_clear();
        else if (strcmp(tokens[0], "help") == 0)
            cmd_help();
        else if (strcmp(tokens[0], "desktop") == 0)
            cmd_desktop();
        else if (strcmp(tokens[0], "birthday") == 0)
            cmd_birthday();
        else
            vga_print("Unknown command.\n");
    }
}
