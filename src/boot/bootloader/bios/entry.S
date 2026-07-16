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

.section .entry16, "ax"
.code16
.global stage2_entry

#define BOOT_INFO_DRIVE		0
#define BOOT_INFO_MEM_LOWER	4
#define BOOT_INFO_MEM_UPPER	8
#define BOOT_INFO_FB_ADDR	12
#define BOOT_INFO_FB_PITCH	16
#define BOOT_INFO_FB_WIDTH	20
#define BOOT_INFO_FB_HEIGHT	24
#define BOOT_INFO_FB_BPP	28
#define BOOT_INFO_FB_TYPE	32
#define BIOS_E820_MAX		64

stage2_entry:
	cli
	xorw	%ax, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movw	$0xfc00, %sp

	movzbl	%dl, %eax
	movl	%eax, bios_boot_info + BOOT_INFO_DRIVE

	call	enable_a20
	call	detect_memory
	call	setup_vbe

	cli
	lgdt	gdt_desc
	movl	%cr0, %eax
	orl	$1, %eax
	movl	%eax, %cr0
	ljmpl	$0x08, $protected_entry

enable_a20:
	inb	$0x92, %al
	orb	$0x02, %al
	outb	%al, $0x92
	ret

detect_memory:
	int	$0x12
	movzwl	%ax, %eax
	movl	%eax, bios_boot_info + BOOT_INFO_MEM_LOWER

	call	detect_e820

	movw	$0xe801, %ax
	int	$0x15
	jc	mem_fallback
	cmpw	$0, %cx
	jne	1f
	movw	%ax, %cx
	movw	%bx, %dx
1:
	movzwl	%cx, %eax
	movzwl	%dx, %edx
	shll	$6, %edx
	addl	%edx, %eax
	cmpl	$0, %eax
	je	mem_fallback
	movl	%eax, bios_boot_info + BOOT_INFO_MEM_UPPER
	ret

detect_e820:
	pushw	%es
	pushw	%di
	xorw	%ax, %ax
	movw	%ax, %es
	movl	$0, bios_mmap_count
	xorl	%ebx, %ebx
	movw	$bios_mmap_entries, %di

e820_loop:
	cmpl	$BIOS_E820_MAX, bios_mmap_count
	jae	e820_done
	xorw	%ax, %ax
	movw	%ax, %es
	movl	$1, 20(%di)
	movl	$0xe820, %eax
	movl	$0x534d4150, %edx
	movl	$24, %ecx
	int	$0x15
	jc	e820_done
	cmpl	$0x534d4150, %eax
	jne	e820_done
	cmpl	$20, %ecx
	jb	e820_done
	movl	8(%di), %eax
	orl	12(%di), %eax
	jz	e820_next
	cmpl	$24, %ecx
	jb	1f
	testl	$1, 20(%di)
	jz	e820_next
	jmp	2f
1:
	movl	$0, 20(%di)
2:
	addw	$24, %di
	incl	bios_mmap_count
e820_next:
	testl	%ebx, %ebx
	jnz	e820_loop

e820_done:
	popw	%di
	popw	%es
	ret

mem_fallback:
	movb	$0x88, %ah
	int	$0x15
	jc	1f
	movzwl	%ax, %eax
	movl	%eax, bios_boot_info + BOOT_INFO_MEM_UPPER
	ret
1:
	movl	$64512, bios_boot_info + BOOT_INFO_MEM_UPPER
	ret

setup_vbe:
	pushw	%ds
	xorw	%ax, %ax
	movw	%ax, %es
	movw	%ax, %ds
	movl	$0x32454256, 0x5000
	movw	$0x4f00, %ax
	movw	$0x5000, %di
	int	$0x10
	cmpw	$0x004f, %ax
	jne	vbe_done

	movw	0x500e, %si
	movw	0x5010, %ds
vbe_loop:
	movw	(%si), %cx
	cmpw	$0xffff, %cx
	je	vbe_fallback
	movw	%cx, vbe_mode_tmp
	pushw	%ds
	pushw	%si
	xorw	%ax, %ax
	movw	%ax, %es
	movw	$0x5600, %di
	movw	$0x4f01, %ax
	int	$0x10
	cmpw	$0x004f, %ax
	jne	vbe_next
	movw	0x5600, %ax
	andw	$0x0091, %ax
	cmpw	$0x0091, %ax
	jne	vbe_next
	cmpw	$1024, 0x5612
	jne	vbe_next
	cmpw	$768, 0x5614
	jne	vbe_next
	cmpb	$32, 0x5619
	jne	vbe_next

	movw	vbe_mode_tmp, %bx
	orw	$0x4000, %bx
	movw	$0x4f02, %ax
	int	$0x10
	cmpw	$0x004f, %ax
	jne	vbe_next
	movl	0x5628, %eax
	movl	%eax, bios_boot_info + BOOT_INFO_FB_ADDR
	movzwl	0x5610, %eax
	movl	%eax, bios_boot_info + BOOT_INFO_FB_PITCH
	movzwl	0x5612, %eax
	movl	%eax, bios_boot_info + BOOT_INFO_FB_WIDTH
	movzwl	0x5614, %eax
	movl	%eax, bios_boot_info + BOOT_INFO_FB_HEIGHT
	movzbl	0x5619, %eax
	movl	%eax, bios_boot_info + BOOT_INFO_FB_BPP
	movl	$1, bios_boot_info + BOOT_INFO_FB_TYPE
	addw	$4, %sp
	jmp	vbe_done

vbe_next:
	popw	%si
	popw	%ds
	addw	$2, %si
	jmp	vbe_loop

vbe_fallback:
	xorw	%ax, %ax
	movw	%ax, %ds
	movw	$0x4118, vbe_mode_tmp
	movw	$0x4f01, %ax
	movw	$0x4118, %cx
	movw	$0x5600, %di
	xorw	%ax, %ax
	movw	%ax, %es
	movw	$0x4f01, %ax
	int	$0x10
	cmpw	$0x004f, %ax
	jne	vbe_done
	cmpb	$32, 0x5619
	jne	vbe_done
	movw	$0x4f02, %ax
	movw	vbe_mode_tmp, %bx
	int	$0x10
	cmpw	$0x004f, %ax
	jne	vbe_done
	movl	0x5628, %eax
	movl	%eax, bios_boot_info + BOOT_INFO_FB_ADDR
	movzwl	0x5610, %eax
	movl	%eax, bios_boot_info + BOOT_INFO_FB_PITCH
	movzwl	0x5612, %eax
	movl	%eax, bios_boot_info + BOOT_INFO_FB_WIDTH
	movzwl	0x5614, %eax
	movl	%eax, bios_boot_info + BOOT_INFO_FB_HEIGHT
	movzbl	0x5619, %eax
	movl	%eax, bios_boot_info + BOOT_INFO_FB_BPP
	movl	$1, bios_boot_info + BOOT_INFO_FB_TYPE

vbe_done:
	popw	%ds
	ret

pm16_read_entry:
	movw	$0x20, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movl	%cr0, %eax
	andl	$0xfffffffe, %eax
	movl	%eax, %cr0
	ljmp	$0x0000, $real_read_entry

real_read_entry:
	xorw	%ax, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movw	$0xfc00, %sp
	movb	bios_boot_info + BOOT_INFO_DRIVE, %dl
	movw	$bios_disk_packet, %si
	movb	$0x42, %ah
	int	$0x13
	jnc	real_read_ok
	call	real_read_chs
	jnc	real_read_ok
	movb	%ah, %al
	jmp	real_read_status
real_read_ok:
	movb	$0, %al
real_read_status:
	movzbl	%al, %eax
	movl	%eax, bios_call_status

	cli
	lgdt	gdt_desc
	movl	%cr0, %eax
	orl	$1, %eax
	movl	%eax, %cr0
	ljmpl	$0x08, $pm_read_return

real_read_chs:
	pushw	%bx
	pushw	%cx
	pushw	%dx
	pushw	%di

	movb	bios_boot_info + BOOT_INFO_DRIVE, %dl
	movb	$0x08, %ah
	int	$0x13
	jc	chs_defaults
	movb	%cl, %al
	andb	$0x3f, %al
	cmpb	$0, %al
	je	chs_defaults
	movb	%al, chs_spt
	movb	%dh, %al
	incb	%al
	movb	%al, chs_heads
	jmp	chs_have_geom

chs_defaults:
	movb	$63, chs_spt
	movb	$16, chs_heads

chs_have_geom:
	movw	bios_disk_packet + 8, %ax
	xorw	%dx, %dx
	movzbw	chs_spt, %cx
	divw	%cx
	incw	%dx
	movb	%dl, chs_sector

	xorw	%dx, %dx
	movzbw	chs_heads, %cx
	divw	%cx
	cmpw	$1023, %ax
	ja	chs_bad
	movw	%ax, chs_cyl
	movb	%dl, chs_head

	movb	chs_cyl, %ch
	movb	chs_sector, %cl
	movw	chs_cyl, %ax
	andw	$0x0300, %ax
	shrw	$2, %ax
	orb	%al, %cl

	movb	chs_head, %dh
	movb	bios_boot_info + BOOT_INFO_DRIVE, %dl
	movw	bios_disk_packet + 4, %bx
	movw	bios_disk_packet + 6, %es
	movb	bios_disk_packet + 2, %al
	movb	$0x02, %ah
	int	$0x13
	jc	chs_done
	clc
	jmp	chs_done

chs_bad:
	movb	$0x04, %ah
	stc

chs_done:
	popw	%di
	popw	%dx
	popw	%cx
	popw	%bx
	ret

.code32
protected_entry:
	movw	$0x10, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss
	movl	$0x0009fc00, %esp
	call	bios_main
	jmp	bios_halt

.global bios_read_sectors
bios_read_sectors:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	movl	8(%ebp), %eax
	movl	%eax, bios_disk_packet + 8
	movl	$0, bios_disk_packet + 12

	movl	12(%ebp), %eax
	movw	%ax, bios_disk_packet + 2

	movl	16(%ebp), %eax
	movl	%eax, %ebx
	andl	$0x0f, %eax
	movw	%ax, bios_disk_packet + 4
	shrl	$4, %ebx
	movw	%bx, bios_disk_packet + 6

	movl	%esp, pm_saved_esp
	cli
	ljmp	$0x18, $pm16_read_entry

pm_read_return:
	movw	$0x10, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss
	movl	pm_saved_esp, %esp
	movl	bios_call_status, %eax

	popl	%edi
	popl	%esi
	popl	%ebx
	leave
	ret

.global bios_jump_kernel
bios_jump_kernel:
	movl	4(%esp), %ecx
	movl	8(%esp), %eax
	movl	12(%esp), %ebx
	cli
	movw	$0x10, %dx
	movw	%dx, %ds
	movw	%dx, %es
	movw	%dx, %fs
	movw	%dx, %gs
	movw	%dx, %ss
	movl	$0x0009fc00, %esp
	jmp	*%ecx

.global bios_halt
bios_halt:
	cli
1:
	hlt
	jmp	1b

.section .bios16data, "aw"
.align 8
.global bios_boot_info
bios_boot_info:
	.long	0
	.long	640
	.long	64512
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0

	.align 8
	.global bios_mmap_count
bios_mmap_count:
	.long	0
	.align 8
	.global bios_mmap_entries
bios_mmap_entries:
	.space	BIOS_E820_MAX * 24

	.align 8
	gdt:
	.quad	0x0000000000000000
	.quad	0x00cf9a000000ffff
	.quad	0x00cf92000000ffff
	.quad	0x00009a000000ffff
	.quad	0x000092000000ffff
gdt_desc:
	.word	gdt_desc - gdt - 1
	.long	gdt

.align 4
pm_saved_esp:
	.long	0
bios_call_status:
	.long	0
vbe_mode_tmp:
	.word	0
chs_cyl:
	.word	0
chs_spt:
	.byte	63
chs_heads:
	.byte	16
chs_sector:
	.byte	0
chs_head:
	.byte	0
bios_disk_packet:
	.byte	0x10
	.byte	0
	.word	0
	.word	0
	.word	0
	.long	0
	.long	0
