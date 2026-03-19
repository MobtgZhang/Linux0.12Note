/*
 * loader_notes.c — design notes for a minimal UEFI boot loader (phase 2, docs/UEFI_X86_64_PORT.md)
 *
 * NOT linked into tools/system or Kernel_Image. Safe to ignore for normal `make Image`.
 *
 * ---------------------------------------------------------------------------
 * Role vs legacy setup.S + bootsect
 * ---------------------------------------------------------------------------
 *  - bootsect/setup: real mode, INT 13h load, BIOS INTs, fixed buffer 0x90000–0x901FF.
 *  - UEFI loader: long mode already (x86_64), PE/COFF image loaded by firmware.
 *    Typical steps:
 *      1. EfiMain(ImageHandle, SystemTable) — locate and read kernel file from ESP or media.
 *      2. AllocatePages / LoadImage: place kernel (ELF64 or raw) in RAM below or above 4G
 *         per your linker script.
 *      3. GetMemoryMap, then ExitBootServices(MapKey) — after this, only RuntimeServices
 *         and your mappings are valid; firmware boot-time pointers may dangle.
 *      4. Build handoff struct (analogue of 0x90000 layout): physical memory map, GOP
 *         framebuffer base/size, optional ACPI RSDP pointer, root dev or cmdline.
 *      5. Jump to kernel entry (e.g. startup_64) with pointer to handoff in register per ABI.
 *
 * ---------------------------------------------------------------------------
 * Phase 3 kernel entry (conceptual)
 * ---------------------------------------------------------------------------
 *  - New head_64.S: establish own GDT, IDT, RSP, optional 4-level page tables, parse handoff.
 *  - Linux 0.12 head.s paging is 2-level identity map 16 MiB; x86_64 needs PML4/PDPT/PD/PT.
 *
 * Tooling: gnu-efi, EDK2, or a minimal PE linker script + crt0 for UEFI subsystems.
 */

/* Placeholder symbol so this file can be compiled with `gcc -c` during experiments. */
void uefi_loader_notes_placeholder(void) { }
