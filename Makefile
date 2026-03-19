#
# Linux 0.12 UEFI x86_64 — build/boot/uefi/BOOTX64.EFI + build/tools/kernel.elf + build/esp.img
#
# 构建类型（切换后建议 make clean 再编）：
#   make release   或  make BUILD_TYPE=release   — Release：最少日志
#   make debug     或  make BUILD_TYPE=debug     — Debug：UEFI + 内核桩详细日志
#
include Makefile.header

# 所有本 Makefile 产生的二进制与中间文件放在 build/ 下
BUILD_DIR        := build
BUILD_BOOT_UEFI  := $(BUILD_DIR)/boot/uefi
BUILD_INIT       := $(BUILD_DIR)/init
BUILD_TOOLS      := $(BUILD_DIR)/tools

BUILD_TYPE ?= release

ifeq ($(BUILD_TYPE),debug)
STUB_BUILD_FLAGS := -DBOOT_DEBUG=1
EFI_BUILD_FLAGS := -DBOOT_DEBUG=1
else
STUB_BUILD_FLAGS := -DBOOT_RELEASE=1
EFI_BUILD_FLAGS := -DBOOT_RELEASE=1
endif

CFLAGS	+= -ffreestanding
CPP	+= -Iinclude

.PHONY: all release debug clean qemu-uefi qemu-uefi-debug uefi-ref check-gnuefi clean-subdirs help

# gnu-efi 在发行版中的路径略有差异
GNUEFI_CRT0 ?= $(firstword $(wildcard /usr/lib/crt0-efi-x86_64.o /usr/lib/gnuefi/crt0-efi-x86_64.o))
GNUEFI_LDS  ?= $(firstword $(wildcard /usr/lib/elf_x86_64_efi.lds /usr/lib/gnuefi/elf_x86_64_efi.lds))
GNUEFI_LIBS ?= -L/usr/lib -lgnuefi -lefi

# crt0 以 SysV ABI 调用 efi_main；Boot Services 需经 uefi_call_wrapper 转为固件约定
EFI_CPPFLAGS := -I/usr/include/efi -I/usr/include/efi/x86_64 -I/usr/include/efi/protocol \
	-ffreestanding -fpic -fshort-wchar -fno-stack-protector -fno-stack-check \
	-mno-red-zone -DEFI_FUNCTION_WRAPPER $(EFI_BUILD_FLAGS)

# 合并镜像 ovmf/OVMF.fd 常可用 -bios；拆分的 CODE_4M 需 pflash 配对，故优先合并包
OVMF_CODE ?= $(firstword $(wildcard \
	/usr/share/ovmf/OVMF.fd \
	/usr/share/OVMF/OVMF_CODE_4M.fd \
	/usr/share/OVMF/OVMF_CODE.fd \
	/usr/share/qemu/edk2-x86_64-code.fd))

all: $(BUILD_DIR)/esp.img

release:
	@$(MAKE) BUILD_TYPE=release $(BUILD_DIR)/esp.img

debug:
	@$(MAKE) BUILD_TYPE=debug $(BUILD_DIR)/esp.img

check-gnuefi:
	@test -f /usr/include/efi/efi.h || (echo "请安装: sudo apt install gnu-efi"; exit 1)
	@test -n "$(GNUEFI_CRT0)" && test -f "$(GNUEFI_CRT0)" || (echo "未找到 crt0-efi-x86_64.o，请安装 gnu-efi"; exit 1)
	@test -n "$(GNUEFI_LDS)" && test -f "$(GNUEFI_LDS)" || (echo "未找到 elf_x86_64_efi.lds"; exit 1)

$(BUILD_BOOT_UEFI) $(BUILD_INIT) $(BUILD_TOOLS):
	@mkdir -p $@

$(BUILD_BOOT_UEFI)/loader.o: boot/uefi/loader.c check-gnuefi | $(BUILD_BOOT_UEFI)
	@printf '    %b %b\n' "\033[34mCC\033[0m" "\033[33m$@\033[0m" 1>&2
	@gcc $(EFI_CPPFLAGS) -c -o $@ $<

$(BUILD_BOOT_UEFI)/jump_kernel.o: boot/uefi/jump_kernel.S check-gnuefi | $(BUILD_BOOT_UEFI)
	@printf '    %b %b\n' "\033[34mCC\033[0m" "\033[33m$@\033[0m" 1>&2
	@gcc $(EFI_CPPFLAGS) -c -o $@ $<

$(BUILD_BOOT_UEFI)/loader.so: $(BUILD_BOOT_UEFI)/loader.o $(BUILD_BOOT_UEFI)/jump_kernel.o | $(BUILD_BOOT_UEFI)
	@printf '    %b %b\n' "\033[34mLINK\033[0m" "\033[37;1m$@\033[0m" 1>&2
	@gcc -nostdlib -Wl,-nostdlib -Wl,-znocombreloc -Wl,-T$(GNUEFI_LDS) -Wl,-shared -Wl,-Bsymbolic \
		-m64 -mno-red-zone $(GNUEFI_CRT0) $(BUILD_BOOT_UEFI)/loader.o $(BUILD_BOOT_UEFI)/jump_kernel.o \
		-o $@ $(GNUEFI_LIBS) -lgcc

$(BUILD_BOOT_UEFI)/BOOTX64.EFI: $(BUILD_BOOT_UEFI)/loader.so | $(BUILD_BOOT_UEFI)
	@printf '    %b %b\n' "\033[32;1mOBJCOPY\033[0m" "\033[37;1m$@\033[0m" 1>&2
	@objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
		-j .rel -j .rela -j .reloc \
		--target=efi-app-x86_64 $< $@

$(BUILD_BOOT_UEFI)/head64.o: boot/uefi/head64.s | $(BUILD_BOOT_UEFI)
	$(AS) -o $@ $<

$(BUILD_INIT)/stub_main.o: init/stub_main.c | $(BUILD_INIT)
	$(CC) $(CFLAGS) $(STUB_BUILD_FLAGS) -c -o $@ $<

$(BUILD_TOOLS)/kernel.elf: $(BUILD_BOOT_UEFI)/head64.o $(BUILD_INIT)/stub_main.o boot/uefi/kernel64.lds | $(BUILD_TOOLS)
	$(LD) -nostdlib -z max-page-size=0x1000 -T boot/uefi/kernel64.lds \
		$(BUILD_BOOT_UEFI)/head64.o $(BUILD_INIT)/stub_main.o -o $@

$(BUILD_DIR)/esp.img: $(BUILD_BOOT_UEFI)/BOOTX64.EFI $(BUILD_TOOLS)/kernel.elf tools/mkesp.sh
	@chmod +x tools/mkesp.sh
	@tools/mkesp.sh $(BUILD_DIR)/esp.img $(BUILD_BOOT_UEFI)/BOOTX64.EFI $(BUILD_TOOLS)/kernel.elf

qemu-uefi: $(BUILD_DIR)/esp.img
	@test -n "$(OVMF_CODE)" || (echo "未找到 OVMF 固件，请安装 ovmf 或 qemu-efi 并检查路径"; exit 1)
	qemu-system-x86_64 -machine q35 -cpu qemu64 -m 128M \
		-bios $(OVMF_CODE) \
		-drive file=$(BUILD_DIR)/esp.img,if=virtio,format=raw \
		-serial stdio -monitor none -no-reboot

# Debug 构建后运行（先 make debug）
qemu-uefi-debug: $(BUILD_DIR)/esp.img
	@test -n "$(OVMF_CODE)" || (echo "未找到 OVMF 固件，请安装 ovmf 或 qemu-efi 并检查路径"; exit 1)
	qemu-system-x86_64 -machine q35 -cpu qemu64 -m 128M \
		-bios $(OVMF_CODE) \
		-drive file=$(BUILD_DIR)/esp.img,if=virtio,format=raw \
		-serial stdio -monitor none -no-reboot -d guest_errors

uefi-ref:
	@$(MAKE) -C boot/uefi reference

clean: clean-subdirs
	@rm -f Kernel_Image System.map system.S tmp_make core
	@rm -rf $(BUILD_DIR)
	@rm -f esp.img boot/uefi/*.o boot/uefi/loader.so boot/uefi/BOOTX64.EFI
	@rm -f init/stub_main.o init/main.o tools/kernel.elf

clean-subdirs:
	@for i in mm fs kernel lib; do $(MAKE) clean -C $$i 2>/dev/null || true; done

help:
	@echo "构建: make release | make debug  （默认 BUILD_TYPE=release，切换后建议 make clean）"
	@echo "产物: $(BUILD_DIR)/esp.img、$(BUILD_BOOT_UEFI)/BOOTX64.EFI、$(BUILD_TOOLS)/kernel.elf"
	@echo "运行: make qemu-uefi  或  make qemu-uefi-debug（额外 guest_errors 日志）"
	@echo "说明: 当前为桩内核，无根文件系统，不会出现 bash /usr/bin；串口见 -serial stdio"

start: qemu-uefi

dep:
	@sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	@(for i in init/*.c;do echo -n "$(BUILD_INIT)/";$(CPP) -M $$i;done) >> tmp_make
	@cp tmp_make Makefile

### Dependencies
$(BUILD_INIT)/stub_main.o: init/stub_main.c
