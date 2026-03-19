# Linux 0.12：UEFI x86-64 引导与移植说明

本仓库为 **UEFI + x86_64** 版本，内核源码基于 [sky-big/Linux-0.12](https://github.com/sky-big/Linux-0.12)，仅保留 UEFI 启动链，不包含 BIOS/i386 引导代码。

---

## 当前引导流水线

| 阶段 | 文件/二进制 | 作用 |
|------|-------------|------|
| 1 | `boot/uefi/loader.c` → **`build/boot/uefi/BOOTX64.EFI`** | UEFI 应用：从 ESP 读取 `kernel.elf`，映射 PT_LOAD，ExitBootServices 后跳入内核入口 |
| 2 | `boot/uefi/jump_kernel.S` | 退出 Boot Services 后跳转到 64 位内核 |
| 3 | `boot/uefi/head64.s` + `init/stub_main.c` 或 `init/main.c` → **`build/tools/kernel.elf`** | 入口 `startup_64` 调用 `main()`；默认为桩实现（串口输出），完整内核为 `init/main.c` 的 `main()` |

镜像：`tools/mkesp.sh` 生成 **`build/esp.img`**（FAT，含 BOOTX64.EFI 与 kernel.elf）。

---

## 完整内核源码与 x86-64 移植

以下目录为 Linux 0.12 完整内核源码（与 sky-big/Linux-0.12 结构一致），当前使用 **x86_64 工具链**（`Makefile.header` 中 `-m64`、`as --64`）：

- **fs/** — 文件系统
- **kernel/** — 调度、系统调用、陷阱、驱动（blk_drv、chr_drv、math）
- **mm/** — 内存管理
- **lib/** — 内核库
- **include/** — 头文件
- **init/main.c** — 完整内核入口（原 32 位设计，依赖 setup 放在 0x90000 的启动参数与 INT 0x80）

要构建并运行**完整内核**（而不仅是桩），需要完成 x86-64 移植，主要包括：

1. **汇编**：将 `kernel/sys_call.s`（INT 0x80 → syscall/sysret）、`kernel/asm.s`、`mm/page.s` 及驱动中的 `.s` 改为 64 位（.code64、64 位栈与调用约定）。
2. **系统调用**：`include/unistd.h` 中 `_syscall0`/`_syscall1` 等宏当前为 32 位内联汇编，需改为 x86-64 syscall 约定。
3. **启动参数**：原 `main()` 从 0x90000 读取 setup 写入的参数；UEFI 下需改为由 loader 传递（或暂时写死/最小参数）。
4. **头文件与 C 代码**：`include/asm/`、段/门等仍为 32 位假设，需随 GDT/IDT/分页等改为 long mode 设计。

默认 `make` 仅构建桩内核，保证 UEFI 引导链可运行；完整内核需在上述移植完成后，在根 Makefile 中增加链接 `head64.o + main.o + mm.o + fs.o + kernel.o + lib.a` 等目标。

---

## 构建与运行

- **构建**：`make` → 生成 `build/esp.img`（UEFI + 桩内核）
- **QEMU**：`make qemu-uefi`（需安装 OVMF）
- **参考汇编**：`make uefi-ref` 仅编译 `boot/uefi/reference_long_mode.S`

---

## 相关文件

- `boot/uefi/loader.c` — UEFI 加载器
- `boot/uefi/jump_kernel.S` — 跳转内核
- `boot/uefi/head64.s` — 64 位内核入口（调用 `main`）
- `boot/uefi/kernel64.lds` — 内核链接脚本
- `init/stub_main.c` — 桩 `main()`（串口输出）
- `init/main.c` — 完整内核 `main()`（待 x86-64 移植后参与链接）
