# Linux 0.12 — UEFI x86-64 版

本仓库为 **Linux 0.12** 的 **UEFI + x86_64** 启动版本，内核源码结构参考 [sky-big/Linux-0.12](https://github.com/sky-big/Linux-0.12)（中文注释、高版本 GCC 可编译）。

- **引导**：仅 UEFI，无 BIOS/软盘。源码在 `boot/uefi/`，编译产物在 **`build/boot/uefi/BOOTX64.EFI`**，从 ESP 加载 64 位内核。
- **架构**：x86_64（`Makefile.header` 使用 `-m64`、`elf_x86_64`）。
- **默认构建**：UEFI 加载器 + **最小桩内核**（`boot/uefi/head64.s` + `init/stub_main.c`），经 **COM1 串口**输出，用于验证 UEFI → long mode → 内核 全链路。
- **为何没有 bash / `/usr/bin`**：当前没有挂载根文件系统、没有用户态 init/shell；只有内核桩，**不会出现** Shell 提示符，属正常。要出现类 Bash 环境需后续提供根镜像、init 与用户态程序。
- **完整内核源码**：已包含 `fs/`、`kernel/`、`mm/`、`lib/`、`include/`、`init/main.c` 等；要构建并运行完整内核，需将汇编与系统调用等移植到 x86-64（见 `docs/UEFI_X86_64_PORT.md`）。

## Release / Debug 构建

| 目标 | 说明 |
|------|------|
| `make` 或 `make release` | **Release**：固件控制台最少提示（`UEFI: boot` / `UEFI: kernel`），串口一行 |
| `make debug` | **Debug**：UEFI 与内核桩输出 `[UEFI][D]` / `[KDBG]` 等详细日志 |

切换类型后建议先 **`make clean`** 再编，避免混用旧 `.o`。

```bash
make clean && make debug && make qemu-uefi   # 看完整日志（终端需带 -serial stdio）
make qemu-uefi-debug                         # 额外打开 QEMU guest_errors（需已编好 build/esp.img）
make help                                    # 简要帮助
```

**看日志**：桩内核只往 **串口 COM1** 打字符，请用 `make qemu-uefi`（已含 `-serial stdio`）；若只开图形窗口、不看终端，会以为「没输出」。

## 依赖

- `gcc` / `binutils`（x86_64，支持 `-m64`）
- **gnu-efi**：`sudo apt install gnu-efi`
- **mtools**（生成 FAT ESP 镜像）：`sudo apt install mtools`
- QEMU + OVMF：`sudo apt install ovmf qemu-system-x86`

## 构建与运行

```bash
make release      # 或 make（默认 release）→ build/esp.img
make debug        # Debug 日志版本 → build/esp.img
make qemu-uefi    # OVMF + virtio + 串口输出到当前终端
```

`make start` 等同于 `make qemu-uefi`。若 OVMF 路径不同：`OVMF_CODE=/path/to/OVMF.fd make qemu-uefi`。

## 目录与参考

- `boot/uefi/` — UEFI 加载器与 64 位内核入口源码；**产物**：`build/boot/uefi/`、`build/tools/kernel.elf`、`build/esp.img`
- `init/stub_main.c` — 桩入口 `main()`；完整内核入口在 `init/main.c`
- `fs/`、`kernel/`、`mm/`、`lib/`、`include/` — Linux 0.12 完整内核源码（x86-64 移植进行中）
- `docs/UEFI_X86_64_PORT.md` — UEFI 引导流水线与完整内核移植说明
- `boot/uefi/README.md` — UEFI 目录说明
- `make uefi-ref` — 仅汇编参考 `reference_long_mode.S`
