.section .text
.global _start
.type _start, @function
_start:
	xorq	%rbp, %rbp
	subq	$8, %rsp
	call	__libc_start
1:
	jmp	1b
