# EC-OS

A bare-metal operating system built from scratch by **EasyCloud Studio**.  
NASM bootloader + C kernel, with VBE graphics, PS/2 drivers, a window manager, and a custom `.afm` executable format.

> If you want to use or modify it, feel free to fork it.  
> If you just want to try it in VMware, download the `.iso` from the Releases page and boot it.

---

## Features

- **Bootloader** – 512-byte NASM boot sector, switches to protected mode, sets up VBE.
- **Kernel** – Written in C (i686-elf-gcc), with assembly entry point.
- **Graphics** – 32bpp linear framebuffer, double buffering, dirty-rectangle tracking.
- **Window Manager** – Draggable windows, close buttons, Z-order, rounded corners.
- **Desktop Environment** – Icons, taskbar, clock, right-click context menus.
- **File System** – In-memory node tree, supports files and directories.
- **PS/2 Drivers** – Keyboard and mouse, with fractional accumulator for smooth cursor.
- **Applications** – File Explorer, MiniDOS (a simple command shell), and more.
- **Custom Executable Format** – `.afm` files can be launched from the desktop.
- **VBE Support** – Reads mode info from the bootloader, validates resolution and bpp.

---

## Screenshots

*(Add screenshots here – desktop, windows, MiniDOS, etc.)*

---

## Building

### Prerequisites

- **NASM** (for bootloader and kernel entry)
- **i686-elf-gcc** (cross-compiler for 32-bit freestanding C)
- **GNU Make** (or use `build.bat` on Windows)
- **QEMU** (for testing) or **VMware Workstation**

### Build Commands

```bash
# Linux / macOS / WSL
make

# Windows (using build.bat)
build.bat
```

The build outputs are placed in the `build/` directory:
- `kernel.bin` – raw kernel binary
- `ec-os.iso` – bootable ISO image
- `disk.img` – floppy/disk image (for QEMU)

---

## Running

### QEMU

```bash
qemu-system-i386 -fda build/ec-os.iso -m 1024
```

### VMware Workstation

1. Create a new virtual machine.
2. Select **Other** → **DOS** (or **Other 32-bit**).
3. When asked for a disk, choose **Use an existing virtual disk** and select `build/disk.vmdk`.
4. Start the VM.

> **Important:** VMware's VBE implementation does **not** support 1920×1080×32.  
> The current default resolution is 1280×1024×32, which works in VMware.  
> If you want 1920×1080, use QEMU instead.

---

## Project Structure

```
EC-OS/
├── boot/              # Bootloader (NASM)
│   └── boot.asm
├── kernel/            # Kernel entry (NASM)
│   └── kernel_entry.asm
├── system/            # Kernel main (C)
│   ├── ecdos.c        # kernel_main()
│   ├── gfx.c/.h       # Graphics library
│   ├── fs.c/.h        # File system
│   ├── shell.c/.h     # Command shell
│   ├── desktop.c/.h   # Desktop environment
│   ├── window.c/.h    # Window manager
│   ├── mouse.c/.h     # PS/2 mouse driver
│   ├── keyboard.c/.h  # PS/2 keyboard driver
│   ├── vbe.c/.h       # VBE info reader
│   ├── vga.c/.h       # Text console
│   ├── minidos.c/.h   # MiniDOS application
│   ├── file_explorer.c/.h
│   ├── afm.c/.h       # .afm executable format
│   ├── font.c/.h      # 16x32 bitmap font
│   ├── lib.c/.h       # Minimal C library
│   ├── io.h           # Port I/O inline functions
│   └── linker.ld      # Linker script
├── tools/             # Build utilities (Python)
│   ├── gen_font.py
│   ├── pack.py
│   └── png2cursor.py
├── build/             # Build output (ignored by Git)
├── Makefile
├── build.bat
├── img2vmdk.py
└── README.md
```

---

## Known Issues

- **VMware resolution limit** – VMware's VBE only supports up to 1280×1024×32. Use QEMU for 1920×1080.
- **Mouse on some platforms** – The PS/2 mouse driver may not work on all emulators. QEMU and VMware are tested.
- **No networking** – EC-OS does not yet support network cards.
- **Limited file system** – The file system is in-memory only; changes are lost on reboot.

---

## License

This project is licensed under the **MIT License**.  
See the [LICENSE](LICENSE) file for details.

> If you use this code in your own project, please keep the copyright notice and a link back to this repository.

---

## Contributing

Contributions are welcome!  
Feel free to fork the repo, open issues, or submit pull requests.

If you find a bug or have a feature request, please [open an issue](https://github.com/Minecraft-and-pocket-world-mango/EC-OS/issues).

---

## Credits

**EasyCloud Studio** – [escd.top](http://escd.top)  
Made with ❤️ by a group of developers who don't even know each other's real names.

---

## Contact

- GitHub: [Minecraft-and-pocket-world-mango](https://github.com/Minecraft-and-pocket-world-mango)
- Website: [escd.top](http://escd.top)
