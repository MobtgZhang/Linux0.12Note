# UEFI / x86-64 引导

根目录 `make` 会构建（产物均在 **`build/`** 下）：

- `build/boot/uefi/BOOTX64.EFI` — UEFI 加载器（`loader.c` + `jump_kernel.S`）
- `build/tools/kernel.elf` — **64 位** stub 内核，链接于 `0x100000`（`head64.s`、`init/stub_main.c`）
- `build/esp.img` — FAT ESP，内含 `EFI/BOOT/BOOTX64.EFI` 与 `EFI/BOOT/kernel.elf`

## Contents

- **`reference_long_mode.S`** — 带英文注释的 x86-64 汇编片段，说明 UEFI 进入 long mode 后内核入口的形态；仅作参考，不参与主构建。
- **`loader_notes.c`** — 加载器设计备注（注释），可选 `gcc -c`。

## Building the reference (optional)

From the repository root:

```bash
make uefi-ref
```

Or from this directory:

```bash
make reference
```

The `Makefile` tries `x86_64-linux-gnu-as` or plain `as` with `--64` when supported. If assembly fails, run manually, e.g.:

```bash
x86_64-linux-gnu-as -o reference_long_mode.o reference_long_mode.S
```

## Full UEFI boot

A real UEFI application requires a **PE** image and UEFI APIs (`BootServices`, `RuntimeServices`). Typical setups use **gnu-efi** or EDK2. See `docs/UEFI_X86_64_PORT.md` for scope and roadmap.
