# EC-OS
﻿
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Language: C](https://img.shields.io/badge/Language-C%20%7C%20Assembly-green.svg)]()
[![Platform: x86](https://img.shields.io/badge/Platform-x86%20(i686)-lightgrey.svg)]()
﻿
A bare-metal operating system built completely from scratch by **EasyCloud Studio**. 
NASM bootloader + C kernel, featuring a modern 32-bit protected mode environment, a custom VBE graphics compositor, a window manager, and a proprietary `.afm` executable format.
﻿
> *"Made with ❤️ by a group of developers who don't even know each other's real names."*
﻿
---
﻿
#
﻿
## ✨ Features
﻿
### 🖥️ Core OS & Graphics
- **Bare-metal Bootloader**: 512-byte NASM boot sector, switches to protected mode, and initializes VBE.
- **High-Res Graphics**: 1280x1024x32bpp linear framebuffer, double buffering with dirty-rectangle tracking for smooth performance.
- **PS/2 Drivers**: Full keyboard and fractional-accumulator mouse driver for precise cursor control.
- **Hard Drive Persistence (WIP)**: Custom ATA PIO driver (LBA28) to save your files to the virtual disk so they survive reboots.
﻿
### 🪟 Modern GUI Environment
- **Window Manager**: Draggable windows with close buttons, Z-order sorting, drop shadows, and rounded corners.
- **Desktop Environment**: Custom icon grid, taskbar with system clock, right-click context menus, and a modern "Finder-style" file explorer with a sidebar.
- **i18n Framework**: Built-in internationalization support (English, French, German, etc.) with a centralized string table.
﻿
### 🛠️ Applications & Automation
- **`.afm` (Application File Module)**: A lightweight executable format allowing OS to launch internal apps from the desktop.
- **MiniDOS**: A built-in terminal shell for direct system interaction.
- **`.cfs` (Command File Script)**: A custom automation script format. Double-click a `.cfs` file containing `command:<cmd>` and the OS will automatically execute it in MiniDOS.
﻿
---
﻿
## 🚀 Building
﻿
### Prerequisites
- **NASM** (Assembler)
- **i686-elf-gcc** (Cross-compiler for 32-bit freestanding C)
- **Python 3** (For generating fonts and packing images)
- **GNU Make** (or use the provided `build.bat` on Windows)
﻿
### Build Commands
Ensure `nasm`, `i686-elf-gcc`, and `python` are in your system `PATH`.
﻿
```bash
# Linux / macOS / WSL
make
﻿
# Windows
build.bat
The build outputs are placed in the build/ directory:
﻿
ec-os.iso – Bootable ISO image (for distribution).
﻿
disk.vmdk – VMware virtual disk descriptor (points to the 4GB disk.img).
﻿
kernel.bin – Raw kernel binary.
﻿
💻 Running
VMware Workstation (Recommended)
Create a new virtual machine (Other -> DOS / Other 32-bit).
﻿
Point the Hard Disk to build/disk.vmdk.
﻿
Important: Adjust the VM settings to give the display at least 16MB of Video Memory.
﻿
Boot the VM.
﻿
Resolution Note: VMware's VBE implementation does not support 1920×1080×32. The current default is 1280×1024×32. If you want 1920×1080, use QEMU.
﻿
QEMU
bash
qemu-system-i386 -drive format=raw,file=build/disk.img -m 1024
📁 Project Structure
text
EC-OS/
├── boot/ # Bootloader (NASM)
├── kernel/ # Kernel entry (NASM)
├── system/ # Kernel main & drivers (C)
│ ├── ecdos.c # kernel_main()
│ ├── gfx.c/.h # 2D Graphics library
│ ├── window.c/.h # Window Manager
│ ├── desktop.c/.h # Desktop & Compositor
│ ├── fs.c/.h # In-memory File System
│ ├── ata.c/.h # ATA PIO Hard Drive Driver
│ ├── text_edit.c/.h # Text & Hex Editor
│ ├── file_explorer.c# Finder-style File Manager
│ ├── minidos.c/.h # MiniDOS Shell
│ ├── afm.c/.h # App dispatcher
│ └── ...
├── tools/ # Build utilities (Python)
└── build.bat # Windows build script
🗺️ Roadmap & Known Issues
EC-OS is under active development. Here's what we are working on:
﻿
☑ Persistence: ATA PIO driver for saving files to disk.
□ True Multitasking: Implement preemptive multitasking and user-mode processes.
□ Dynamic Font Loading: Current font is ASCII-only (CJK/Cyrillic fallback to English). We plan to load .ttf or bitmap fonts from the hard drive.
□ Networking: Support for E1000/RTL8139 to access the internet.
□ Port Treestructure: Port the custom programming language compiler/interpreter into EC-OS.
📜 License
This project is licensed under the MIT License. See the LICENSE file for details.
If you use this code in your own project, please keep the copyright notice and a link back to this repository.
﻿
🤝 Contributing & Contact
Contributions are welcome! Feel free to fork the repo, open issues, or submit pull requests.
﻿
GitHub: Minecraft-and-pocket-world-mango
﻿
Website: escd.top
