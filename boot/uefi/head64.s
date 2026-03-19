/*
 * Linux0.12 — UEFI 移交后的 x86_64 内核入口（最小桩）
 * 约定：已在 long mode，分页由固件/loader 维持至 jmp 本入口前。
 */
	.code64
	.text

	.globl	startup_64
	.extern	main

startup_64:
	cli
	movabs	$stack_top, %rsp
	andq	$~0xf, %rsp
	xorl	%ebp, %ebp
	call	main
halt_loop:
	hlt
	jmp	halt_loop

	.section .bss
	.align	16
stack_bottom:
	.space	32768
stack_top:

	.section .note.GNU-stack,"",@progbits
