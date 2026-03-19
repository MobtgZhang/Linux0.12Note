/*
 * ExitBootServices 之后由 loader.c 调用（GCC 默认 SysV ABI）：
 *   RDI = 新 RSP，RSI = 内核入口地址
 */
	.code64
	.text

	.globl	uefi_handoff_kernel
uefi_handoff_kernel:
	cli
	movq	%rdi, %rsp
	xorq	%rbp, %rbp
	jmp	*%rsi

	.section .note.GNU-stack,"",@progbits
