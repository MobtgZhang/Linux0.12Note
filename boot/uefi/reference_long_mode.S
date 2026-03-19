/*
 * reference_long_mode.S
 *
 * EDUCATIONAL REFERENCE ONLY — not linked into the Linux 0.12 floppy Image.
 *
 * Linux 0.12 boots in 32-bit protected mode via BIOS (bootsect -> setup -> head.s).
 * A UEFI x86_64 firmware typically invokes your bootloader already in *long mode*
 * with flat segments and paging set up by firmware until you call ExitBootServices().
 *
 * This file documents, in assembly form, the *kind* of state you must establish
 * for a hypothetical 64-bit kernel entry. It is not a complete UEFI application.
 *
 * Pair with: loader_notes.c (phase 2 loader outline) and head.s (phase 0 startup_32).
 */

	.code64

	.section .text

/*
 * UEFI x86_64 calling convention for the image entry point (Microsoft x64 ABI):
 *   RCX = EFI_HANDLE ImageHandle
 *   RDX = EFI_SYSTEM_TABLE *SystemTable
 *   R8  = EFI_HANDLE (optional, depends on entry type for some toolchains)
 *   R9  = (unused / reserved)
 *
 * Before jumping to "real" kernel C code you usually:
 *   1) Use Boot Services to allocate/load the kernel and fetch the memory map.
 *   2) Call ExitBootServices() — after this, only Runtime Services and your own
 *      mappings are safe to rely on; many firmware pointers become invalid.
 *   3) Build your own page tables if you need a permanent map (e.g. higher-half).
 *   4) Set RSP to a known-good stack in *identity-mapped* or fully mapped RAM.
 *   5) Clear BSS, jump to kernel entry (e.g. startup_64).
 *
 * Contrast with linux 0.12 head.s (startup_32):
 *   - 0.12 enables paging with 2-level tables (directory + tables), identity map 16MB.
 *   - x86_64 uses 4-level paging (PML4 -> PDPT -> PD -> PT) for canonical addresses.
 */

	.globl example_uefi_kernel_entry_stub
example_uefi_kernel_entry_stub:
	/*
	 * Placeholder: in a real loader you would not "ret" to firmware; you would
	 * never return. Here we only show that CS/DS are expected to be 64-bit
	 * flat segments with base 0 when coming from UEFI.
	 */
	cli
	/* Stack (RSP) must point to writable RAM that remains mapped after ExitBootServices. */
	movq	$0, %rbp
	/* Kernel would continue at a linked symbol, e.g. call startup_64 */
	ret

	.section .note.GNU-stack,"",@progbits
