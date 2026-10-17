NASM       := nasm
CC         := i686-elf-gcc
OBJCOPY    := i686-elf-objcopy
BUILD      := build

# resolution: 0 = 1280x1024 (VMware native), 1 = 1920x1080
RES_HIGH   ?= 0

ifeq ($(RES_HIGH),1)
NSW := 1920
NSH := 1080
else
NSW := 1280
NSH := 1024
endif

NASMFLAGS_BOOT := -DKRES_W=$(NSW) -DKRES_H=$(NSH)

CFLAGS     := -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
              -fno-asynchronous-unwind-tables -m32 -mno-sse -mno-mmx \
              -mno-80387 -Wall -Wextra -O2 -I system -DRES_HIGH=$(RES_HIGH)

SRCS := lib vbe vga keyboard fs shell ecdos gfx font font16x32 mouse \
        window afm file_explorer minidos desktop settings i18n icon_data \
        filetype open text_edit idt ata rename_dialog

C_OBJS := $(SRCS:%=$(BUILD)/%.o)

all: $(BUILD)/ec-os.iso $(BUILD)/disk.vmdk

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.bin: boot/boot.asm | $(BUILD)
	$(NASM) -f bin $(NASMFLAGS_BOOT) $< -o $@

$(BUILD)/kernel_entry.o: kernel/kernel_entry.asm | $(BUILD)
	$(NASM) -f elf32 $< -o $@

system/font16x32.c: tools/gen_font.py system/font.c
	python3 tools/gen_font.py

system/icon_data.c system/icon_data.h: tools/png2icon.py system/pic/file.png
	python3 tools/png2icon.py

$(BUILD)/%.o: system/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(BUILD)/kernel_entry.o $(C_OBJS) system/linker.ld
	$(CC) -T system/linker.ld -nostdlib -Wl,--build-id=none \
	    -o $@ $(BUILD)/kernel_entry.o $(C_OBJS) -lgcc

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/disk.img $(BUILD)/floppy.img: $(BUILD)/boot.bin $(BUILD)/kernel.bin tools/pack.py
	python3 tools/pack.py

$(BUILD)/ec-os.iso: $(BUILD)/floppy.img
	python3 tools/pack.py --iso

$(BUILD)/disk.vmdk: $(BUILD)/disk.img img2vmdk.py
	python3 img2vmdk.py

clean:
	rm -rf $(BUILD)

.PHONY: all clean