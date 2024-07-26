#
# TODO: Merge the loader with lightos
#

DIRECTORY_GUARD=mkdir -p $(@D)

define DEFAULT_VAR =
    ifeq ($(origin $1),default)
        override $(1) := $(2)
    endif
    ifeq ($(origin $1),undefined)
        override $(1) := $(2)
    endif
endef

THIS_DIR=$(shell pwd)

$(eval $(call DEFAULT_VAR,CC,$(THIS_DIR)/tools/cc/bin/x86_64-pc-lightos-gcc))
$(eval $(call DEFAULT_VAR,LD,ld))
$(eval $(call DEFAULT_VAR,OBJCOPY,objcopy))
$(eval $(call DEFAULT_VAR,ASM_COMP,nasm))

# efi, bios, ect.
FW_TYPE ?= efi
SSE_ENABLED := true
SEPERATE_PARTITIONS := true
FORCE_RAMDISK ?= false

EMU := qemu-system-x86_64

override INTERNAL_LDFLAGS :=	\
	-Tefi/lib/elf_x86_64_efi.lds	\
    -nostdlib                   	\
    -z max-page-size=0x1000     	\
    -m elf_x86_64               	\
    -static                     	\
    -pie                        	\
    --no-dynamic-linker         	\
    -z text							\

override INTERNAL_CFLAGS :=  \
    -std=gnu11              \
	-nostdlib 				\
	-Wall 		 			\
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
    -I./common/include/     \
    -I./efi/include/        \
	-I./efi/include/x86_64  \

OUT := ./out
BIN_OUT := ./out/bin
RESOURCE_DIR := res

OUT_ELF := light-loader.elf
OUT_EFI := LIGHT-LOADER.EFI
OUT_IMAGE := lloader.img
OUT_ISO := lloader.iso

BOOTRT_DIR=bootrt

# We copy these files into the root of the project for now
KERNEL_ELF_NAME=aniva.elf
KERNEL_RAMDISK_NAME=anivaRamdisk.igz
KERNEL_INTERNAL_RAMDISK_NAME=rdisk.igz

# Path to the lighthouse-os project folder
LIGHTOS_FULLPATH=/home/beakeyz/Source/c/lighthouse-os

LIGHTOS_KERNEL_PATH=$(LIGHTOS_FULLPATH)/out/$(KERNEL_ELF_NAME)
LIGHTOS_RAMDISK_PATH=$(LIGHTOS_FULLPATH)/out/$(KERNEL_RAMDISK_NAME)

SOURCE_DIRECTORIES := common efi
INCLUDE_DIRECTORIES := common/include efi/include

help: ## Prints help for targets with comments
	@cat $(MAKEFILE_LIST) | grep -E '^[a-zA-Z_-]+:.*?## .*$$' | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'

C_OBJ := $(patsubst %.c,$(OUT)/%.o,$(shell find $(SOURCE_DIRECTORIES) -type f -name '*.c'))
S_OBJ := $(patsubst %.S,$(OUT)/%.o,$(shell find $(SOURCE_DIRECTORIES) -type f -name '*.S'))
ASM_OBJ := $(patsubst %.asm,$(OUT)/%.o,$(shell find $(SOURCE_DIRECTORIES) -type f -name '*.asm'))

override HEADER_DEPS := $(patsubst %.o,%.d,$(C_OBJ))

-include $(HEADER_DEPS)
$(OUT)/%.o: %.c
	@echo -e Building: $@
	@$(DIRECTORY_GUARD)
	@$(CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

$(OUT)/%.o: %.S
	@echo -e Building: $@
	@$(DIRECTORY_GUARD)
	@$(CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

$(OUT)/%.o: %.asm
	@echo -e Building: $@
	@$(DIRECTORY_GUARD)
	@$(ASM_COMP) $< -o $@ -f elf64

check_lightos_fullpath: ## Check if the supplied path for the lighthouse-os project is valid
	@stat $(LIGHTOS_KERNEL_PATH)
	@stat $(LIGHTOS_RAMDISK_PATH)

.PHONY: all
all: build image debug

.PHONY: build 
build: $(C_OBJ) $(S_OBJ) $(ASM_OBJ) ## Build the objects
	@echo -e Linking...
	@mkdir -p $(BIN_OUT)
	@$(LD) $^ $(INTERNAL_LDFLAGS) -o $(BIN_OUT)/$(OUT_ELF)
	@$(OBJCOPY) -O binary $(BIN_OUT)/$(OUT_ELF) $(BIN_OUT)/$(OUT_EFI) 
	@echo -e Done =D 

$(BIN_OUT)/$(OUT_IMAGE):
	rm -f $@
	dd if=/dev/zero of=$@ iflag=fullblock bs=1M count=64 && sync

.PHONY: image
image: check_lightos_fullpath $(BIN_OUT)/$(OUT_IMAGE) ## Create a diskimage to debug the bootloader
	sudo rm -rf $(BOOTRT_DIR)/
	sudo rm -rf loopback_dev
	# Make mounting directory
	mkdir -p $(BOOTRT_DIR)/
	# Mount to loop device and save dev name
	sudo losetup -Pf --show $(BIN_OUT)/$(OUT_IMAGE) > loopback_dev
	sudo parted `cat loopback_dev` mklabel gpt
	sudo parted `cat loopback_dev` mkpart primary 2048s 100%
	sudo parted `cat loopback_dev` set 1 boot on
	sudo parted `cat loopback_dev` set 1 hidden on
	sudo parted `cat loopback_dev` set 1 esp on
	# Update and sync
	sudo partprobe `cat loopback_dev`
	# Format the device
	sudo mkfs.fat -F 32 `cat loopback_dev`p1
	sync
	# Mount the device to our dir
	sudo mount `cat loopback_dev`p1 $(BOOTRT_DIR) 
	# Copy stuff over to the device
	sudo mkdir -p $(BOOTRT_DIR)/EFI/BOOT
	sudo mkdir -p $(BOOTRT_DIR)/System
	sudo cp $(BIN_OUT)/$(OUT_EFI) $(BOOTRT_DIR)/EFI/BOOT/BOOTX64.EFI
	sudo cp $(LIGHTOS_KERNEL_PATH) $(BOOTRT_DIR)/$(KERNEL_ELF_NAME)
	sudo cp $(LIGHTOS_RAMDISK_PATH) $(BOOTRT_DIR)/$(KERNEL_INTERNAL_RAMDISK_NAME)
	sudo cp -r $(RESOURCE_DIR) $(BOOTRT_DIR)

ifeq ($(FORCE_RAMDISK), false)
	sudo cp -r $(LIGHTOS_FULLPATH)/system/* $(BOOTRT_DIR)
endif

	sync
	# Cleanup
	sudo umount $(BOOTRT_DIR)/
	sudo losetup -d `cat loopback_dev`
	rm -rf $(BOOTRT_DIR) loopback_dev

INSTALL_DEV ?= none

# End mark on the device where the LightOS system partition ends
LIGHTOS_SYSTEM_PART_END=2G

install_packed:
	@stat $(INSTALL_DEV)
	@echo Making partition table...
	sudo parted $(INSTALL_DEV) mklabel gpt
	#sudo parted $(INSTALL_DEV) mkpart Gap0 2048s 10M
	#sudo parted $(INSTALL_DEV) set 1 hidden on
	sudo parted $(INSTALL_DEV) mkpart LightOS_System 10M 100% 
	sudo parted $(INSTALL_DEV) set 1 boot on
	sudo parted $(INSTALL_DEV) set 1 hidden on
	sudo parted $(INSTALL_DEV) set 1 esp on
	sudo partprobe $(INSTALL_DEV)
	@echo Making System filesystem
	sudo mkfs.fat -F 32 $(INSTALL_DEV)1
	mkdir -p $(BOOTRT_DIR)

	@echo Mounting LightOS System partition
	sudo mount $(INSTALL_DEV)1 $(BOOTRT_DIR)

	sudo mkdir -p $(BOOTRT_DIR)/EFI/BOOT
	sudo cp $(BIN_OUT)/$(OUT_EFI) $(BOOTRT_DIR)/EFI/BOOT/BOOTX64.EFI
	sudo cp $(LIGHTOS_KERNEL_PATH) $(BOOTRT_DIR)/$(KERNEL_ELF_NAME)
	sudo cp $(LIGHTOS_RAMDISK_PATH) $(BOOTRT_DIR)/$(KERNEL_INTERNAL_RAMDISK_NAME)
	sudo cp -r $(RESOURCE_DIR) $(BOOTRT_DIR)

	sudo cp -r $(LIGHTOS_FULLPATH)/system/* $(BOOTRT_DIR)

	sudo umount $(BOOTRT_DIR)
	rm -rf $(BOOTRT_DIR)
	sync

install_seperated:
	@stat $(INSTALL_DEV)
	@echo Making partition table...
	sudo parted $(INSTALL_DEV) mklabel gpt
	sudo parted $(INSTALL_DEV) mkpart Gap0 2048s 10M
	sudo parted $(INSTALL_DEV) set 1 hidden on
	sudo parted $(INSTALL_DEV) mkpart LightOS_System 10M $(LIGHTOS_SYSTEM_PART_END)
	sudo parted $(INSTALL_DEV) set 2 boot on
	sudo parted $(INSTALL_DEV) set 2 hidden on
	sudo parted $(INSTALL_DEV) set 2 esp on
	sudo parted $(INSTALL_DEV) mkpart LightOS_Data $(LIGHTOS_SYSTEM_PART_END) 100%
	sudo parted $(INSTALL_DEV) set 3 hidden off
	sudo parted $(INSTALL_DEV) set 3 boot off
	sudo parted $(INSTALL_DEV) set 3 esp off
	sudo partprobe $(INSTALL_DEV)
	@echo Making System filesystem
	sudo mkfs.fat -F 32 $(INSTALL_DEV)2
	@echo Making Data filesystem
	sudo mkfs.fat -F 32 $(INSTALL_DEV)3
	mkdir -p $(BOOTRT_DIR)

	@echo Mounting LightOS System partition
	sudo mount $(INSTALL_DEV)2 $(BOOTRT_DIR)

	sudo mkdir -p $(BOOTRT_DIR)/EFI/BOOT
	sudo cp $(BIN_OUT)/$(OUT_EFI) $(BOOTRT_DIR)/EFI/BOOT/BOOTX64.EFI
	sudo cp $(LIGHTOS_KERNEL_PATH) $(BOOTRT_DIR)/$(KERNEL_ELF_NAME)
	sudo cp $(LIGHTOS_RAMDISK_PATH) $(BOOTRT_DIR)/$(KERNEL_INTERNAL_RAMDISK_NAME)
	sudo cp -r $(RESOURCE_DIR) $(BOOTRT_DIR)

	sudo umount $(BOOTRT_DIR)

	sudo mount $(INSTALL_DEV)3 $(BOOTRT_DIR)
	sudo cp -r $(LIGHTOS_FULLPATH)/system/* $(BOOTRT_DIR)

	sudo umount $(BOOTRT_DIR)
	rm -rf $(BOOTRT_DIR)
	sync


.PHONY: install
install: check_lightos_fullpath ## Install the bootloader onto a blockdevice (parameter INSTALL_DEV=<device_path>)
ifeq ($(INSTALL_DEV),none)
	@echo Please specify INSTALL_DEV=?
else
ifeq ($(SEPERATE_PARTITIONS),true)
	make install_seperated
else
	make install_packed
endif
endif

.PHONY: clean
clean: ## Remove any object files or binaries from the project
	@echo "[LOADER] cleaning elf..."
	@rm $(BIN_OUT)/$(OUT_ELF) 
	@rm $(BIN_OUT)/$(OUT_EFI) 
	@rm $(BIN_OUT)/$(OUT_IMAGE) 
	@echo "[LOADER] cleaning objs..."
	@rm -rf $(C_OBJ) $(S_OBJ) $(HEADER_DEPS) 
	@echo "[LOADER] done cleaning!"

.PHONY: debug
debug: ## Run lightloader in Qemu
	@$(EMU) -m 1G -net none -M q35 -usb $(BIN_OUT)/$(OUT_IMAGE) -bios ./ovmf/OVMF.fd -serial stdio -device usb-ehci -device usb-kbd -drive file=./install_test.img
