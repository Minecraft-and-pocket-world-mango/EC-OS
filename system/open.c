#include "open.h"
#include "filetype.h"
#include "text_edit.h"
#include "afm.h"
#include "minidos.h"
#include "fs.h"
#include "lib.h"

#define CMD_PREFIX "command:"

void open_file(int fs_idx)
{
    fs_node_t *n;
    int len, i;

    if (fs_idx < 0 || fs_idx >= MAX_NODES)
        return;
    n = &fs_nodes[fs_idx];
    if (n->type != FS_TYPE_FILE)
        return;

    if (ends_with(n->name, ".afm")) {
        afm_run(fs_idx);
        return;
    }

    len = 0;
    while (len < NODE_DATA_SIZE - 1 && n->data[len])
        len++;

    i = 0;
    while (i < len && (n->data[i] == ' ' || n->data[i] == '\t' ||
                       n->data[i] == '\n' || n->data[i] == '\r'))
        i++;

    if (i + 8 <= len && strncmp(n->data + i, CMD_PREFIX, 8) == 0) {
        minidos_exec(n->data + i + 8);
        return;
    }

    if (is_binary(n->data, len)) {
        warn_dialog_open(fs_idx);
        return;
    }

    text_edit_open(fs_idx);
}
