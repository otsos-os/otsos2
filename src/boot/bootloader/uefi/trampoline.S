/*
 * Copyright (c) 2026, otsos team
 *
 * BSD 2-clause license.
 */

.text

.code64
.global uefi_outb
uefi_outb:
	movb	%dl, %al
	movw	%cx, %dx
	outb	%al, %dx
	ret

.global uefi_inb
uefi_inb:
	movw	%cx, %dx
	inb	%dx, %al
	ret

.global uefi_jump32
uefi_jump32:
	cli
	movl	%ecx, %edi
	movl	%edx, %esi
	movl	%r8d, %ebp
	leaq	uefi_gdt_desc(%rip), %rcx
	lgdt	(%rcx)
	pushq	$0x08
	leaq	uefi_compat32(%rip), %rcx
	pushq	%rcx
	lretq

.code32
uefi_compat32:
	movw	$0x10, %cx
	movw	%cx, %ds
	movw	%cx, %es
	movw	%cx, %fs
	movw	%cx, %gs
	movw	%cx, %ss

	movl	%cr0, %ecx
	andl	$0x7fffffff, %ecx
	movl	%ecx, %cr0
	jmp	1f
1:
	movl	$0xc0000080, %ecx
	rdmsr
	andl	$0xfffffeff, %eax
	wrmsr

	movl	%cr4, %ecx
	andl	$0xffffffdf, %ecx
	movl	%ecx, %cr4

	movl	$0x00090000, %esp
	movl	%esi, %eax
	movl	%ebp, %ebx
	jmp	*%edi

.code64
.global uefi_halt
uefi_halt:
	cli
1:
	hlt
	jmp	1b

.data
.align 8
uefi_gdt:
	.quad	0x0000000000000000
	.quad	0x00cf9a000000ffff
	.quad	0x00cf92000000ffff
uefi_gdt_desc:
	.word	uefi_gdt_desc - uefi_gdt - 1
	.quad	uefi_gdt
