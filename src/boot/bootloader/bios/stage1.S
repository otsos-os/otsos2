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

_start:
	ljmp	$0x07c0, $stage1_start

stage1_start:
	cli
	movw	%cs, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movw	$0x7c00, %sp
	sti

	movb	%dl, boot_drive
	call	serial_init
	movw	$msg_start, %si
	call	puts
	movw	$msg_drive, %si
	call	puts
	movb	boot_drive, %al
	call	print_hex8
	movw	$msg_crlf, %si
	call	puts

	movw	$STAGE2_SECTORS, sectors_left
	movw	$STAGE2_SEG, load_seg
	movb	$2, sector_no

read_loop:
	movw	sectors_left, %ax
	testw	%ax, %ax
	jz	boot_stage2

	movw	load_seg, %ax
	movw	%ax, %es
	xorw	%bx, %bx

	movb	boot_drive, %dl
	movb	$0x02, %ah
	movb	$1, %al
	movb	$0, %ch
	movb	sector_no, %cl
	movb	$0, %dh
	int	$0x13
	jc	disk_error

	decw	sectors_left
	addw	$0x20, load_seg
	incb	sector_no
	jmp	read_loop

boot_stage2:
	movb	boot_drive, %dl
	ljmp	$STAGE2_SEG, $0x0000

disk_error:
	movb	%ah, disk_status
	movw	$msg_disk_error, %si
	call	puts
	movw	$msg_drive, %si
	call	puts
	movb	boot_drive, %al
	call	print_hex8
	movw	$msg_sector, %si
	call	puts
	movb	sector_no, %al
	call	print_hex8
	movw	$msg_error, %si
	call	puts
	movb	disk_status, %al
	call	print_hex8
	movw	$msg_crlf, %si
	call	puts
1:
	cli
	hlt
	jmp	1b

serial_init:
	movw	$0x3f9, %dx
	xorb	%al, %al
	outb	%al, %dx
	movw	$0x3fb, %dx
	movb	$0x80, %al
	outb	%al, %dx
	movw	$0x3f8, %dx
	movb	$0x03, %al
	outb	%al, %dx
	movw	$0x3f9, %dx
	xorb	%al, %al
	outb	%al, %dx
	movw	$0x3fb, %dx
	movb	$0x03, %al
	outb	%al, %dx
	movw	$0x3fa, %dx
	movb	$0xc7, %al
	outb	%al, %dx
	movw	$0x3fc, %dx
	movb	$0x0b, %al
	outb	%al, %dx
	ret

serial_putc:
	pushw	%dx
	pushw	%ax
1:
	movw	$0x3fd, %dx
	inb	%dx, %al
	testb	$0x20, %al
	jz	1b
	popw	%ax
	movw	$0x3f8, %dx
	outb	%al, %dx
	popw	%dx
	ret

putc:
	call	serial_putc
	movb	$0x0e, %ah
	movb	$0x00, %bh
	movb	$0x07, %bl
	int	$0x10
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

msg_start:
	.asciz	"OTSOS BIOS stage1\r\n"
msg_disk_error:
	.asciz	"disk read failed\r\n"
msg_drive:
	.asciz	" drive="
msg_sector:
	.asciz	" sector="
msg_error:
	.asciz	" err="
msg_crlf:
	.asciz	"\r\n"

boot_drive:
	.byte	0
sectors_left:
	.word	0
load_seg:
	.word	0
sector_no:
	.byte	0
disk_status:
	.byte	0

.org 510
	.word	0xaa55
