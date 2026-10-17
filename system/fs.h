#ifndef EC_FS_H
#define EC_FS_H

#include <stdint.h>

#define MAX_NODES       64
#define MAX_NAME        32
#define MAX_CHILDREN    16
#define NODE_DATA_SIZE  512

#define FS_TYPE_FREE    0
#define FS_TYPE_FILE    1
#define FS_TYPE_DIR     2

typedef struct fs_node {
    char    name[MAX_NAME];
    uint8_t type;
    int8_t  parent;
    int8_t  children[MAX_CHILDREN];
    int8_t  child_count;
    char    data[NODE_DATA_SIZE];
} fs_node_t;

extern fs_node_t fs_nodes[MAX_NODES];

void fs_init(void);
void fs_load_from_disk(void);
void fs_flush_to_disk(void);
int  fs_cwd(void);
void fs_set_cwd(int idx);
int  fs_resolve(const char *path);
int  fs_create(int parent, const char *name, uint8_t type);
int  fs_delete(int idx);
int  fs_rename(int idx, const char *new_name);
int  fs_copy(int src_idx, int parent, const char *name);

/* virtual capacity, reported by sysinfo (does not reflect real storage) */
int  fs_capacity_mb(void);

#endif
