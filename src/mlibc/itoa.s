	.file	"itoa.c"
	.intel_syntax noprefix
	.text
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align 8
.LC0:
	.string	"zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"
	.text
	.p2align 4
	.globl	itoa
	.type	itoa, @function
itoa:
	endbr64
	mov	eax, edi
	mov	edi, edx
	lea	edx, [rdx-2]
	mov	r8, rsi
	cmp	edx, 34
	ja	.L14
	.p2align 4,,10
	.p2align 3
.L2:
	cdq
	mov	r9d, eax
	mov	rcx, rsi
	add	rsi, 1
	idiv	edi
	add	edx, 35
	movsxd	rdx, edx
	movzx	edx, BYTE PTR .LC0[rdx]
	mov	BYTE PTR [rsi-1], dl
	test	eax, eax
	jne	.L2
	test	r9d, r9d
	js	.L15
.L4:
	mov	BYTE PTR [rsi], 0
	mov	rax, r8
	cmp	r8, rcx
	jnb	.L3
	.p2align 4,,10
	.p2align 3
.L5:
	movzx	edx, BYTE PTR [rcx]
	movzx	esi, BYTE PTR [rax]
	sub	rcx, 1
	add	rax, 1
	mov	BYTE PTR [rcx+1], sil
	mov	BYTE PTR [rax-1], dl
	cmp	rax, rcx
	jb	.L5
.L3:
	mov	rax, r8
	ret
	.p2align 4,,10
	.p2align 3
.L15:
	lea	rax, [rcx+2]
	mov	BYTE PTR [rsi], 45
	mov	rcx, rsi
	mov	rsi, rax
	jmp	.L4
	.p2align 4,,10
	.p2align 3
.L14:
	mov	BYTE PTR [rsi], 0
	mov	rax, r8
	ret
	.size	itoa, .-itoa
	.ident	"GCC: (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 8
	.long	1f - 0f
	.long	4f - 1f
	.long	5
0:
	.string	"GNU"
1:
	.align 8
	.long	0xc0000002
	.long	3f - 2f
2:
	.long	0x3
3:
	.align 8
4:
