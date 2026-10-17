#include "afm.h"
#include "fs.h"
#include "lib.h"
#include "file_explorer.h"
#include "minidos.h"
#include "settings.h"

int afm_run(int fs_idx)
{
    const char *data;

    if (fs_idx < 0 || fs_idx >= MAX_NODES)
        return -1;
    if (fs_nodes[fs_idx].type != FS_TYPE_FILE)
        return -1;

    data = fs_nodes[fs_idx].data;

    if (strcmp(data, AFM_FILE_EXPLORER) == 0) {
        file_explorer_open();
        return 0;
    }
    if (strcmp(data, AFM_MINIDOS) == 0) {
        minidos_open();
        return 0;
    }
    if (strcmp(data, AFM_SETTINGS) == 0) {
        settings_open();
        return 0;
    }

    return -1;
}