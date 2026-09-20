/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

.code16
.section .text, "ax"
.global _start

#ifndef STAGE2_SECTORS
#error "STAGE2_SECTORS is required"
#endif

#define STAGE2_SEG	0x0800
#define PARAM_MAGIC	0x31534f4f

_start:
	jmp	stage1_start
	nop

.org 0x08
stage1_params:
	.long	PARAM_MAGIC
	.long	1
	.long	0
	.word	STAGE2_SECTORS
	.word	0

.org 0x18
stage1_start:
	cli
	ljmp	$0x07c0, $stage1_flat

stage1_flat:
	movw	%cs, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movw	$0x7c00, %sp
	sti

	movb	%dl, boot_drive
	movb	$'1', %al
	call	putc

	movl	stage1_params + 4, %eax
	movl	%eax, lba_lo
	movl	stage1_params + 8, %eax
	movl	%eax, lba_hi
	movw	stage1_params + 12, %ax
	movw	%ax, sectors_left
	movw	$STAGE2_SEG, load_seg

read_loop:
	movw	sectors_left, %ax
	testw	%ax, %ax
	jz	boot_stage2

	movl	lba_lo, %eax
	movl	%eax, dap_lba_lo
	movl	lba_hi, %eax
	movl	%eax, dap_lba_hi
	movw	$1, dap_count
	movw	$0, dap_offset
	movw	load_seg, %ax
	movw	%ax, dap_segment

	movb	boot_drive, %dl
	movw	$dap, %si
	movb	$0x42, %ah
	int	$0x13
	jnc	read_done
	call	read_chs
	jc	disk_error

read_done:
	decw	sectors_left
	addw	$0x20, load_seg
	incl	lba_lo
	jnz	read_loop
	incl	lba_hi
	jmp	read_loop

boot_stage2:
	movb	boot_drive, %dl
	ljmp	$STAGE2_SEG, $0x0000

read_chs:
	pushw	%bx
	pushw	%cx
	pushw	%dx

	cmpl	$0, lba_hi
	jne	chs_fail

	movb	chs_spt, %al
	testb	%al, %al
	jnz	chs_have_geom

	movb	boot_drive, %dl
	movb	$0x08, %ah
	int	$0x13
	jc	chs_defaults
	movb	%cl, %al
	andb	$0x3f, %al
	testb	%al, %al
	jz	chs_defaults
	movb	%al, chs_spt
	movb	%dh, %al
	incb	%al
	movb	%al, chs_heads
	jmp	chs_have_geom

chs_defaults:
	movb	$63, chs_spt
	movb	$16, chs_heads

chs_have_geom:
	movl	lba_lo, %eax
	movzbl	chs_spt, %ebx
	xorl	%edx, %edx
	divl	%ebx
	movb	%dl, %cl
	incb	%cl

	movzbl	chs_heads, %ebx
	xorl	%edx, %edx
	divl	%ebx
	movb	%dl, %dh

	cmpl	$1023, %eax
	ja	chs_fail

	movb	%al, %ch
	shrw	$8, %ax
	andb	$0x03, %al
	shlb	$6, %al
	orb	%al, %cl

	movw	load_seg, %ax
	movw	%ax, %es
	xorw	%bx, %bx
	movb	boot_drive, %dl
	movw	$0x0201, %ax
	int	$0x13
	jc	chs_fail

	popw	%dx
	popw	%cx
	popw	%bx
	clc
	ret

chs_fail:
	popw	%dx
	popw	%cx
	popw	%bx
	stc
	ret

disk_error:
	movb	%ah, disk_status
	movw	$msg_disk_error, %si
	call	puts
	movb	boot_drive, %al
	call	print_hex8
	movb	disk_status, %al
	call	print_hex8
1:
	cli
	hlt
	jmp	1b

putc:
	pushw	%ax
	pushw	%dx
	xorw	%dx, %dx
	movb	$0x01, %ah
	int	$0x14
	popw	%dx
	popw	%ax
	pushw	%ax
	movb	$0x0e, %ah
	movb	$0x07, %bl
	xorb	%bh, %bh
	int	$0x10
	popw	%ax
	ret

puts:
	lodsb
	testb	%al, %al
	jz	1f
	call	putc
	jmp	puts
1:
	ret

print_hex8:
	pushw	%ax
	shrb	$4, %al
	call	print_nibble
	popw	%ax
	call	print_nibble
	ret

print_nibble:
	andb	$0x0f, %al
	cmpb	$10, %al
	jb	1f
	addb	$87, %al
	jmp	putc
1:
	addb	$48, %al
	jmp	putc

msg_disk_error:
	.asciz	"\r\nE13 "

dap:
	.byte	0x10
	.byte	0
dap_count:
	.word	0
dap_offset:
	.word	0
dap_segment:
	.word	0
dap_lba_lo:
	.long	0
dap_lba_hi:
	.long	0

boot_drive:
	.byte	0
disk_status:
	.byte	0
chs_spt:
	.byte	0
chs_heads:
	.byte	0
sectors_left:
	.word	0
load_seg:
	.word	0
lba_lo:
	.long	0
lba_hi:
	.long	0

.global stage1_end
stage1_end:

.org 510
	.word	0xaa55
