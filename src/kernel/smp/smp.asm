
[BITS 16]

section .smp_trampoline alloc exec

%define TRAMPOLINE_BASE	0x8000
%define OFF_CR3		0x100
%define OFF_STACK	0x108
%define OFF_CPU_INDEX	0x110
%define OFF_ENTRY	0x118
%define OFF_GDTR64	0x120
%define OFF_GDTR32	0x130
%define OFF_GDT32	0x140
%define OFF_GDT64	0x180

global ap_trampoline_start
global ap_trampoline_end

ap_trampoline_start:
	cli
	cld
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, 0x7C00

	lgdt [TRAMPOLINE_BASE + OFF_GDTR32]

	mov eax, cr0
	or al, 1
	mov cr0, eax

	jmp dword 0x08:(TRAMPOLINE_BASE + ap_trampoline_prot32 - ap_trampoline_start)

[BITS 32]
ap_trampoline_prot32:
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	mov eax, cr4
	or eax, (1 << 5) | (1 << 9) | (1 << 10)
	mov cr4, eax

	mov eax, [TRAMPOLINE_BASE + OFF_CR3]
	mov cr3, eax

	mov ecx, 0xC0000080
	rdmsr
	or eax, 1 << 8
	wrmsr

	mov eax, cr0
	and eax, 0xfffffff3
	or eax, (1 << 1) | (1 << 31)
	mov cr0, eax

	lgdt [TRAMPOLINE_BASE + OFF_GDTR64]

	jmp 0x08:(TRAMPOLINE_BASE + ap_trampoline_long64 - ap_trampoline_start)

[BITS 64]
ap_trampoline_long64:
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov ss, ax
	mov rax, TRAMPOLINE_BASE + OFF_STACK
	mov rsp, [rax]
	mov rax, TRAMPOLINE_BASE + OFF_CPU_INDEX
	mov rdi, [rax]
	mov rax, TRAMPOLINE_BASE + OFF_ENTRY
	mov rax, [rax]
	jmp rax

ap_trampoline_end:
