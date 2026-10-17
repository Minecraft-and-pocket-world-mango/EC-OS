#include "ata.h"
#include "io.h"

/* Primary 总线 I/O 端口 */
#define ATA_DATA        0x1F0
#define ATA_SECCOUNT    0x1F2
#define ATA_LBA_LO      0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HI      0x1F5
#define ATA_DRIVE       0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7
#define ATA_ALT_STATUS  0x3F6

#define ATA_SR_BSY      0x80
#define ATA_SR_DRDY     0x40
#define ATA_SR_DF       0x20
#define ATA_SR_DRQ      0x08
#define ATA_SR_ERR      0x01

#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30
#define ATA_CMD_FLUSH   0xE7

#define ATA_TIMEOUT     1000000

static void ata_delay400(void)
{
    /* 连续读取 4 次备用状态端口，得到约 400ns 的延时，
     * 用于驱动器/磁头选择后的稳定等待。 */
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
}

static int ata_wait_ready(void)
{
    uint32_t i;
    for (i = 0; i < ATA_TIMEOUT; i++) {
        if ((inb(ATA_STATUS) & ATA_SR_BSY) == 0)
            return 0;
    }
    return -1;
}

static int ata_wait_drq(void)
{
    uint32_t i;
    uint8_t st;

    for (i = 0; i < ATA_TIMEOUT; i++) {
        st = inb(ATA_STATUS);
        if (st & ATA_SR_BSY)
            continue;
        if (st & (ATA_SR_ERR | ATA_SR_DF))
            return -1;
        if (st & ATA_SR_DRQ)
            return 0;
    }
    return -1;
}

int ata_init(void)
{
    uint8_t st;

    outb(ATA_DRIVE, 0xE0);   /* 主盘，LBA 模式 */
    ata_delay400();

    st = inb(ATA_STATUS);
    if (st == 0x00 || st == 0xFF)
        return 0;            /* 无硬盘 / 浮空总线 */

    if (ata_wait_ready() < 0)
        return 0;

    return 1;
}

int ata_read_sectors(uint32_t lba, uint8_t count, void *buf)
{
    uint16_t *p = (uint16_t *)buf;
    uint8_t s;
    int i;

    for (s = 0; s < count; s++) {
        if (ata_wait_ready() < 0)
            return -1;

        outb(ATA_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
        ata_delay400();
        outb(ATA_SECCOUNT, 1);
        outb(ATA_LBA_LO, (uint8_t)(lba & 0xFF));
        outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
        outb(ATA_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
        outb(ATA_COMMAND, ATA_CMD_READ);

        if (ata_wait_drq() < 0)
            return -1;

        for (i = 0; i < 256; i++)
            p[i] = inw(ATA_DATA);

        p += 256;
        lba++;
    }
    return 0;
}

int ata_write_sectors(uint32_t lba, uint8_t count, const void *buf)
{
    const uint16_t *p = (const uint16_t *)buf;
    uint8_t s;
    int i;

    for (s = 0; s < count; s++) {
        if (ata_wait_ready() < 0)
            return -1;

        outb(ATA_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
        ata_delay400();
        outb(ATA_SECCOUNT, 1);
        outb(ATA_LBA_LO, (uint8_t)(lba & 0xFF));
        outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
        outb(ATA_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
        outb(ATA_COMMAND, ATA_CMD_WRITE);

        if (ata_wait_drq() < 0)
            return -1;

        for (i = 0; i < 256; i++)
            outw(ATA_DATA, p[i]);

        p += 256;
        lba++;

        outb(ATA_COMMAND, ATA_CMD_FLUSH);
        if (ata_wait_ready() < 0)
            return -1;
    }
    return 0;
}
