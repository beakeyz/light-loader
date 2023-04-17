# TODO: make loader a standalone makeproject
DIRECTORY_GUARD=mkdir -p $(@D)

define DEFAULT_VAR =
    ifeq ($(origin $1),default)
        override $(1) := $(2)
    endif
    ifeq ($(origin $1),undefined)
        override $(1) := $(2)
    endif
endef

$(eval $(call DEFAULT_VAR,CC,./cross_compiler/bin/x86_64-pc-lightos-gcc))
$(eval $(call DEFAULT_VAR,LD,ld))
$(eval $(call DEFAULT_VAR,OBJCOPY,objcopy))

SSE_ENABLED := true

override INTERNALLDFLAGS :=                \
	-Tlight-efi/gnuefi/elf_x86_64_efi.lds  \
    -nostdlib                              \
    -z max-page-size=0x1000                \
    -m elf_x86_64                          \
    -static                                \
    -pie                                   \
    --no-dynamic-linker                    \
    -z text																 \


override INTERNALCFLAGS :=  \
    -std=gnu11              \
	-nostdlib 				\
    -ffreestanding          \
    -fno-stack-protector    \
    -fno-stack-check        \
    -fshort-wchar           \
    -fno-lto                \
    -fPIE                   \
    -m64                    \
    -march=x86-64           \
    -mabi=sysv              \
    -mno-80387              \
    -mno-mmx                \
    -mno-red-zone           \
    -MMD                    \
    -DGNU_EFI_USE_MS_ABI    \
    -I./src/                \
    -Ilight-efi/inc        \
    -Ilight-efi/inc/x86_64

OUT := ./out
BIN_OUT := ./out/bin

SRC_DIR := ./src/

LOADER_C_SRC := $(shell find $(SRC_DIR) -type f -name '*.c')
LOADER_S_SRC := $(shell find $(SRC_DIR) -type f -name '*.S')
LOADER_ASM_SRC := $(shell find $(SRC_DIR) -type f -name '*.asm')

LOADER_C_OBJ := $(patsubst %.c,$(OUT)/%.o,$(LOADER_C_SRC))
LOADER_S_OBJ := $(patsubst %.S,$(OUT)/%.o,$(LOADER_S_SRC))
LOADER_ASM_OBJ := $(patsubst %.asm,$(OUT)/%.o,$(LOADER_ASM_SRC))

LOADER_EFI_OBJS := light-efi/gnuefi/crt0-efi-x86_64.o light-efi/gnuefi/reloc_x86_64.o
override HEADER_DEPS := $(patsubst %.c,$(OUT)/%.d,$(LOADER_C_SRC))

$(LOADER_EFI_OBJS):
	$(MAKE) -C light-efi/gnuefi ARCH=x86_64

# TODO: add crosscompiler and compile assembly
-include $(HEADER_DEPS)
$(OUT)/%.o: %.c
	@$(DIRECTORY_GUARD)
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -c $< -o $@

$(OUT)/%.o: %.S
	@$(DIRECTORY_GUARD)
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -c $< -o $@

$(OUT)/%.o: %.asm
	@$(DIRECTORY_GUARD)
	nasm $< -o $@ -f elf64

.PHONY: build
build: $(LOADER_EFI_OBJS) $(LOADER_C_OBJ) $(LOADER_S_OBJ) $(LOADER_ASM_OBJ)
	$(LD) $^ $(LDFLAGS) $(INTERNALLDFLAGS) -o ./light-loader.elf
	$(OBJCOPY) -O binary ./light-loader.elf ./LIGHT.EFI

.PHONY: clean 
clean:
	@echo "[LOADER] cleaning elf..."
	@rm -rf ./light-loader.elf
	@echo "[LOADER] cleaning objs..."
	@rm -rf $(LOADER_EFI_OBJS) $(LOADER_S_OBJ) $(LOADER_C_OBJ) $(HEADER_DEPS) 
	@echo "[LOADER] done cleaning!"

# For using the EFI loader on real hardware, make sure to use the same 
# File structure as done here: EFI/BOOT/BOOTX64.EFI
# This makes sure the firmware is able to find our loader, otherwise it dies =)
test.hdd:
	rm -f test.hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=test.hdd
	parted -s test.hdd mklabel gpt
	parted -s test.hdd mkpart primary 2048s 100%

BOOTRT_DIR=bootrt
KERNEL_ELF_NAME=kernel.elf

# TODO: redo this test function for the real thing
test:
	@echo $(LOADER_C_SRC)
	@echo $(LOADER_S_SRC)
	$(MAKE) test.hdd
	sudo rm -rf $(BOOTRT_DIR)/
	sudo rm -rf loopback_dev
	mkdir -p $(BOOTRT_DIR)/
	sudo losetup -Pf --show test.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.fat -F 32 `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 $(BOOTRT_DIR) 
	# sudo mkdir $(BOOTRT_DIR)/boot
	sudo mkdir -p $(BOOTRT_DIR)/EFI/BOOT
	sudo cp LIGHT.EFI $(BOOTRT_DIR)/EFI/BOOT/BOOTX64.EFI
	sudo cp $(KERNEL_ELF_NAME) $(BOOTRT_DIR)/kernel.elf
	sync
	sudo umount $(BOOTRT_DIR)/
	sudo losetup -d `cat loopback_dev`
	rm -rf $(BOOTRT_DIR) loopback_dev

	qemu-system-x86_64 -m 512M -net none -M q35 -hda test.hdd -bios ovmf/OVMF.fd -serial stdio

# Creates a disk image that can be loaded onto a fat-formatted USB-drive and then
# be loaded from that. It takes the directory ./bootrt as it's sysroot, so any 
# config- or binary files will first be copied there before creating the image.
.PHONY: install
install:
	echo TODO
