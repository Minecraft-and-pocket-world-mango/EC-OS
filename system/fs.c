#include "fs.h"
#include "lib.h"
#include "ata.h"
#include "idt.h"

#define FS_SECTOR_SIZE   512
#define FS_DISK_LBA      1000     /* 持久化 FS 区起始扇区 (LBA)          */
#define FS_DISK_SECTORS  72       /* 保留范围：LBA 1000..1071            */

/* 整个节点表必须能放进磁盘上保留的范围内（编译期检查）。 */
typedef char fs_region_check[
    (MAX_NODES * sizeof(fs_node_t) <= FS_DISK_SECTORS * FS_SECTOR_SIZE)
        ? 1 : -1];

fs_node_t fs_nodes[MAX_NODES];
static int cwd = 0;
static int fs_disk_ok = 0;

static int fs_sector_count(void)
{
    return (int)((sizeof(fs_nodes) + FS_SECTOR_SIZE - 1) / FS_SECTOR_SIZE);
}

static int fs_validate(void)
{
    int i;

    /* 合法镜像的第一个节点必是根目录 "P:"。空盘或外来盘读回来是
     * 全 0（type == FS_TYPE_FREE），必须丢弃以保留默认数据。 */
    if (fs_nodes[0].type != FS_TYPE_DIR)
        return 0;
    if (fs_nodes[0].name[0] != 'P')
        return 0;

    for (i = 0; i < MAX_NODES; i++) {
        if (fs_nodes[i].type > FS_TYPE_DIR)
            return 0;
    }
    return 1;
}

void fs_load_from_disk(void)
{
    fs_disk_ok = 0;

    if (!ata_init())
        return;                 /* 无 ATA 硬盘：降级为纯内存，静默返回 */

    fs_disk_ok = 1;

    if (ata_read_sectors(FS_DISK_LBA, (uint8_t)fs_sector_count(), fs_nodes) < 0) {
        fs_init();
        return;
    }

    if (!fs_validate())
        fs_init();              /* 空盘 / 外来盘：保留 fs_init() 默认数据 */
}

void fs_flush_to_disk(void)
{
    if (!fs_disk_ok)
        return;                 /* 无硬盘：降级为纯内存，不写盘 */

    if (ata_write_sectors(FS_DISK_LBA, (uint8_t)fs_sector_count(), fs_nodes) < 0)
        diag_mark('W');         /* 写失败：串口打个 'W' 便于诊断 */
}

static int fs_lookup(int parent, const char *name)
{
    int i;
    fs_node_t *n;

    if (parent < 0 || parent >= MAX_NODES)
        return -1;

    n = &fs_nodes[parent];
    if (n->type != FS_TYPE_DIR)
        return -1;

    for (i = 0; i < n->child_count; i++) {
        int ci = n->children[i];
        if (ci < 0 || ci >= MAX_NODES)
            continue;
        if (strcmp(fs_nodes[ci].name, name) == 0)
            return ci;
    }
    return -1;
}

void fs_init(void)
{
    int i;

    memset(fs_nodes, 0, sizeof(fs_nodes));

    for (i = 0; i < MAX_NODES; i++) {
        fs_nodes[i].parent = -1;
        fs_nodes[i].child_count = 0;
    }

    fs_nodes[0].type = FS_TYPE_DIR;
    fs_nodes[0].parent = -1;
    fs_nodes[0].child_count = 0;
    fs_nodes[0].name[0] = 'P';
    fs_nodes[0].name[1] = '\0';

    cwd = 0;
}

int fs_cwd(void)
{
    return cwd;
}

void fs_set_cwd(int idx)
{
    if (idx >= 0 && idx < MAX_NODES && fs_nodes[idx].type == FS_TYPE_DIR)
        cwd = idx;
}

int fs_resolve(const char *path)
{
    int cur;
    const char *p;
    char comp[MAX_NAME];

    if (path == 0)
        return cwd;

    p = path;

    if (p[0] == 'P' && p[1] == ':') {
        p += 2;
        cur = 0;
    } else if (p[0] == '~') {
        p += 1;
        cur = 0;
    } else if (p[0] == '/') {
        cur = 0;
    } else {
        cur = cwd;
    }

    while (*p == '/')
        p++;

    while (*p != '\0') {
        int i = 0;

        while (*p != '\0' && *p != '/') {
            if (i < MAX_NAME - 1)
                comp[i++] = *p;
            p++;
        }
        comp[i] = '\0';

        while (*p == '/')
            p++;

        if (i == 0)
            continue;

        cur = fs_lookup(cur, comp);
        if (cur < 0)
            return -1;
    }

    return cur;
}

int fs_create(int parent, const char *name, uint8_t type)
{
    int i;
    int idx = -1;
    fs_node_t *p;

    if (parent < 0 || parent >= MAX_NODES)
        return -1;

    p = &fs_nodes[parent];
    if (p->type != FS_TYPE_DIR)
        return -1;
    if (p->child_count >= MAX_CHILDREN)
        return -1;
    if (name == 0 || name[0] == '\0')
        return -1;
    if (strlen(name) >= MAX_NAME)
        return -1;
    if (type != FS_TYPE_FILE && type != FS_TYPE_DIR)
        return -1;
    if (fs_lookup(parent, name) >= 0)
        return -1;

    for (i = 0; i < MAX_NODES; i++) {
        if (fs_nodes[i].type == FS_TYPE_FREE) {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return -1;

    memset(&fs_nodes[idx], 0, sizeof(fs_node_t));
    strcpy(fs_nodes[idx].name, name);
    fs_nodes[idx].type = type;
    fs_nodes[idx].parent = (int8_t)parent;
    fs_nodes[idx].child_count = 0;

    p->children[p->child_count] = (int8_t)idx;
    p->child_count++;

    fs_flush_to_disk();
    return idx;
}

int fs_delete(int idx)
{
    int i;
    fs_node_t *n;
    fs_node_t *p;
    int parent;

    if (idx <= 0 || idx >= MAX_NODES)
        return -1;

    n = &fs_nodes[idx];
    if (n->type == FS_TYPE_FREE)
        return -1;
    if (n->type == FS_TYPE_DIR && n->child_count > 0)
        return -1;

    parent = n->parent;
    if (parent < 0 || parent >= MAX_NODES)
        return -1;

    p = &fs_nodes[parent];

    for (i = 0; i < p->child_count; i++) {
        if (p->children[i] == idx)
            break;
    }
    if (i >= p->child_count)
        return -1;

    for (; i < p->child_count - 1; i++)
        p->children[i] = p->children[i + 1];
    p->child_count--;

    memset(n, 0, sizeof(fs_node_t));
    n->parent = -1;
    n->child_count = 0;

    fs_flush_to_disk();
    return 0;
}

int fs_rename(int idx, const char *new_name)
{
    fs_node_t *n;
    int parent;
    int found;

    if (idx < 0 || idx >= MAX_NODES)
        return -1;

    n = &fs_nodes[idx];
    if (n->type == FS_TYPE_FREE)
        return -1;
    if (new_name == 0 || new_name[0] == '\0')
        return -1;
    if (strlen(new_name) >= MAX_NAME)
        return -1;

    parent = n->parent;
    found = fs_lookup(parent, new_name);
    if (found >= 0 && found != idx)
        return -1;

    strcpy(n->name, new_name);
    fs_flush_to_disk();
    return 0;
}

int fs_copy(int src_idx, int parent, const char *name)
{
    int new_idx;

    if (src_idx < 0 || src_idx >= MAX_NODES)
        return -1;
    if (fs_nodes[src_idx].type != FS_TYPE_FILE)
        return -1;

    new_idx = fs_create(parent, name, FS_TYPE_FILE);
    if (new_idx < 0)
        return -1;

    memcpy(fs_nodes[new_idx].data, fs_nodes[src_idx].data, NODE_DATA_SIZE);
    fs_flush_to_disk();
    return new_idx;
}

int fs_capacity_mb(void)
{
    return 4096;
}