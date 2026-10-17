import sys
import struct
from pathlib import Path

BOOT      = Path("build/boot.bin")
KERNEL    = Path("build/kernel.bin")
OUT_DISK  = Path("build/disk.img")
OUT_FLOP  = Path("build/floppy.img")
IMG_SIZE  = 4096 * 1024 * 1024          # 4 GiB virtual disk
KERNEL_MAX = 512 * 512                   # 256 KiB, matches boot.asm KERNEL_SEC
RESERVED_SECTORS = 1000                  # boot + kernel 暂存区；文件系统数据
                                         # 位于扇区 1000 起（见 fs.c）

def both16(v): return struct.pack('<H', v) + struct.pack('>H', v)
def both32(v): return struct.pack('<I', v) + struct.pack('>I', v)
def le16(v):   return struct.pack('<H', v)
def le32(v):   return struct.pack('<I', v)

def dtime():
    return bytes([124, 1, 1, 0, 0, 0, 0])

def dir_record(name, extent, size, is_dir=False):
    flags = 0x02 if is_dir else 0x00
    namelen = len(name)
    rec_len = 33 + namelen
    if namelen % 2 == 0:
        rec_len += 1
    rec = bytearray()
    rec.append(rec_len)
    rec.append(0)
    rec += both32(extent)
    rec += both32(size)
    rec += dtime()
    rec.append(flags)
    rec.append(0)
    rec.append(0)
    rec += both16(1)
    rec.append(namelen)
    rec += name
    if namelen % 2 == 0:
        rec.append(0)
    return bytes(rec)

def make_iso(boot_img, label, out_path):
    SECTOR = 2048
    FLOPPY_SIZE = 1474560
    label_bytes = label.encode('ascii')[:32].ljust(32)

    PVD_SECTOR       = 16
    BOOT_REC_SECTOR  = 18
    BOOT_CAT_SECTOR  = 20
    PATH_TBL_SECTOR  = 21
    ROOT_DIR_SECTOR  = 22
    BOOT_IMG_SECTOR  = 23

    boot_img_sectors = (FLOPPY_SIZE + SECTOR - 1) // SECTOR
    total_sectors    = BOOT_IMG_SECTOR + boot_img_sectors

    pvd = bytearray(SECTOR)
    pvd[0] = 1
    pvd[1:6] = b'CD001'
    pvd[6] = 1
    pvd[8:40]  = b' ' * 32
    pvd[40:72] = label_bytes
    pvd[80:88] = both32(total_sectors)
    pvd[120:124] = both16(1)
    pvd[124:128] = both16(1)
    pvd[128:132] = both16(SECTOR)
    pvd[132:140] = both32(10)
    pvd[140:144] = le32(PATH_TBL_SECTOR)
    pvd[144:148] = le32(0)
    pvd[148:152] = struct.pack('>I', PATH_TBL_SECTOR)
    pvd[152:156] = struct.pack('>I', 0)
    pvd[156:190] = dir_record(b'\x00', ROOT_DIR_SECTOR, SECTOR, is_dir=True)[:34].ljust(34, b'\x00')
    pvd[190:318] = b' ' * 128
    pvd[318:446] = b' ' * 128
    pvd[446:574] = b' ' * 128
    pvd[574:702] = b' ' * 128
    pvd[702:739] = b' ' * 37
    pvd[739:776] = b' ' * 37
    pvd[776:813] = b' ' * 37
    pvd[813:830] = b'0' * 16 + b'\x00'
    pvd[830:847] = b'0' * 16 + b'\x00'
    pvd[847:864] = b'0' * 16 + b'\x00'
    pvd[864:881] = b'0' * 16 + b'\x00'
    pvd[881] = 1

    term = bytearray(SECTOR)
    term[0] = 255
    term[1:6] = b'CD001'
    term[6] = 1

    boot_rec = bytearray(SECTOR)
    boot_rec[0] = 0
    boot_rec[1:6] = b'CD001'
    boot_rec[6] = 1
    boot_rec[7:39] = b'EL TORITO SPECIFICATION'.ljust(32, b'\x00')
    boot_rec[0x47:0x4B] = le32(BOOT_CAT_SECTOR)

    ve = bytearray(32)
    ve[0] = 0x01                       # Validation Entry header id
    ve[1] = 0x00                       # platform id: 0 = x86
    ve[30] = 0x55
    ve[31] = 0xAA
    chk = (-sum(ve[i] | (ve[i + 1] << 8) for i in range(0, 32, 2))) & 0xFFFF
    ve[28] = chk & 0xFF
    ve[29] = (chk >> 8) & 0xFF

    e = bytearray(32)
    e[0] = 0x88                        # bootable
    e[1] = 0x02                        # 1.44 MB floppy emulation
    e[2:4] = le16(0)                   # load segment
    e[6:8] = le16(1)                   # sector count
    e[8:12] = le32(BOOT_IMG_SECTOR)    # load RBA

    bc = bytearray(SECTOR)
    bc[0:32] = bytes(ve)
    bc[32:64] = bytes(e)

    pt = bytearray()
    pt.append(1)
    pt.append(0)
    pt += le32(ROOT_DIR_SECTOR)
    pt += le16(1)
    pt.append(0)
    path_tbl = bytes(pt).ljust(SECTOR, b'\x00')

    root = bytearray()
    root += dir_record(b'\x00', ROOT_DIR_SECTOR, SECTOR, is_dir=True)
    root += dir_record(b'\x01', ROOT_DIR_SECTOR, SECTOR, is_dir=True)
    root_sector = bytes(root).ljust(SECTOR, b'\x00')

    out = bytearray()
    out += b'\x00' * (16 * SECTOR)
    out += pvd
    out += term
    out += boot_rec
    out += term
    out += bc
    out += path_tbl
    out += root_sector
    out += boot_img
    while len(out) % SECTOR != 0:
        out += b'\x00'

    Path(out_path).write_bytes(bytes(out))
    print(f"  ec-os.iso -> {out_path} ({len(out)} bytes)")

def main():
    boot = BOOT.read_bytes()
    kernel = KERNEL.read_bytes()

    assert len(boot) == 512, f"boot.bin must be 512 bytes, got {len(boot)}"
    assert boot[510:512] == b"\x55\xaa", "missing 0xAA55 boot signature"
    if len(kernel) > KERNEL_MAX:
        print(f"[ERROR] kernel.bin is {len(kernel)} bytes, max {KERNEL_MAX}")
        print(f"        edit boot.asm KERNEL_SEC and rebuild")
        sys.exit(1)

    # 磁盘布局：[扇区 0: boot][扇区 1..512: kernel][... 直到 RESERVED_SECTORS]。
    # 文件系统节点表由内核持久化在扇区 1000（LBA），即位于这段保留区*之外*。
    # 当 disk.img 已存在时，绝不能擦掉该区域——只覆写 boot 扇区和 kernel，
    # 保留扇区 1000 及之后的所有数据。
    target_head = RESERVED_SECTORS * 512

    if OUT_DISK.exists():
        with open(OUT_DISK, "r+b") as f:
            f.write(boot)
            f.write(kernel)
            written = f.tell()
            if written < target_head:
                f.write(b"\x00" * (target_head - written))
            # 绝不 seek/truncate：保留 FS 区和 4 GiB 大小
        print(f"  disk.img   -> {OUT_DISK} (boot+kernel updated, data preserved)")
    else:
        # 4 GiB sparse image: write the used head, then seek to the last byte
        # so NTFS keeps the unused zeros as a hole instead of allocating 4 GiB.
        with open(OUT_DISK, "wb") as f:
            f.write(boot)
            f.write(kernel)
            f.seek(IMG_SIZE - 1)
            f.write(b"\x00")
        print(f"  disk.img   -> {OUT_DISK} ({IMG_SIZE} bytes, sparse)")

    floppy = bytearray(1474560)
    floppy[0:512] = boot
    floppy[512:512 + len(kernel)] = kernel
    OUT_FLOP.write_bytes(floppy)
    print(f"  floppy.img -> {OUT_FLOP} (1474560 bytes)")

    if "--iso" in sys.argv:
        make_iso(bytes(floppy), "EC-OS", "build/ec-os.iso")

if __name__ == "__main__":
    main()