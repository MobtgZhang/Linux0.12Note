#!/bin/bash
# 生成 FAT32 ESP 镜像，写入 BOOTX64.EFI 与 kernel.elf
# 依赖: mtools (mformat, mmd, mcopy)

set -euo pipefail
IMG="${1:?usage: mkesp.sh <esp.img> <BOOTX64.EFI> <kernel.elf>}"
EFI_BIN="${2:?}"
KERN_ELF="${3:?}"

command -v mformat >/dev/null 2>&1 || {
	echo "缺少 mtools：sudo apt install mtools" >&2
	exit 1
}

rm -f "$IMG"
truncate -s 64M "$IMG"
mformat -i "$IMG" -F -v EFIBOOT ::
mmd -i "$IMG" ::/EFI ::/EFI/BOOT
mcopy -i "$IMG" -o "$EFI_BIN" ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "$IMG" -o "$KERN_ELF" ::/EFI/BOOT/kernel.elf
echo "ESP 镜像已生成: $IMG"
