from pathlib import Path


def main():
    raw_path  = Path("build/disk.img")
    vmdk_path = Path("build/disk.vmdk")

    if not raw_path.exists():
        print("找不到 build/disk.img，请先运行 build.bat")
        return

    # Only the size is needed -- never load the 4 GiB image into memory.
    sector_count = raw_path.stat().st_size // 512

    heads   = 16
    sectors = 63
    cylinders = (sector_count + (heads * sectors) - 1) // (heads * sectors)

    # Point the descriptor straight at disk.img (the raw image) instead of
    # copying it to disk-flat.vmdk, which would duplicate all 4 GiB.
    descriptor = f"""# Disk DescriptorFile
version=1
encoding="UTF-8"
CID=fffffffe
parentCID=ffffffff
createType="monolithicFlat"

RW {sector_count} FLAT "{raw_path.name}" 0

ddb.virtualHWVersion = "4"
ddb.geometry.cylinders = "{cylinders}"
ddb.geometry.heads = "{heads}"
ddb.geometry.sectors = "{sectors}"
ddb.adapterType = "ide"
"""
    vmdk_path.write_text(descriptor)
    print("转换成功！")
    print(f"  描述符: {vmdk_path}")
    print(f"  数据  : {raw_path}")


if __name__ == "__main__":
    main()
