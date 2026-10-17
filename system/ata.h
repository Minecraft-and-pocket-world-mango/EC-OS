#ifndef EC_ATA_H
#define EC_ATA_H

#include <stdint.h>

/* 极简 ATA PIO（LBA28）驱动，操作 Primary Master IDE 硬盘。
 * 适配 disk.vmdk 暴露的 VMware "ide" 适配器。
 * 找不到硬盘时返回 0 并降级为纯内存模式，绝不 Panic。 */

int ata_init(void);
int ata_read_sectors(uint32_t lba, uint8_t count, void *buf);
int ata_write_sectors(uint32_t lba, uint8_t count, const void *buf);

#endif
