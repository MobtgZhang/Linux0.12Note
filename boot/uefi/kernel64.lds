/* x86_64 内核桩链接脚本 — 物理/虚拟 1MB 加载（与 loader 约定一致） */
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(startup_64)

SECTIONS
{
	. = 0x100000;

	.text : ALIGN(4096) {
		*(.text .text.*)
	}

	.rodata : ALIGN(4096) {
		*(.rodata .rodata.*)
	}

	.data : ALIGN(4096) {
		*(.data .data.*)
	}

	.bss : ALIGN(4096) {
		*(.bss .bss.*)
		*(COMMON)
	}

	/DISCARD/ : {
		*(.note.GNU-stack)
		*(.eh_frame .eh_frame.*)
	}
}
