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

/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type registers_t as struct with CPU register snapshot
$define %type kms_console_t as struct with KMS console state

$define %func dump_memory as procedure with args u64, int
$define %func print_stack_trace as procedure with args u64
$define %func print_panic_logo as procedure with args void
$define %func kernel_panic as procedure with args registers_t *
$define %func panic as procedure with args const char *, ...

*/

/* !SPACE!

$space %export dump_memory, print_stack_trace
$space %export print_panic_logo, kernel_panic, panic

*/

#include <kernel/console/console.h>
#include <kernel/console/terminal.h>
#include <kernel/drivers/console/kms_console.h>
#include <kernel/drivers/video/drm/rapi/rapi.h>
#include <kernel/interrupts/idt.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

static const char *exception_messages[] = {
	"Division By Zero",
	"Debug",
	"Non Maskable Interrupt",
	"Breakpoint",
	"Into Detected Overflow",
	"Out of Bounds",
	"Invalid Opcode",
	"No Coprocessor",
	"Double Fault",
	"Coprocessor Segment Overrun",
	"Bad TSS",
	"Segment Not Present",
	"Stack Fault",
	"General Protection Fault",
	"Page Fault",
	"Unknown Interrupt",
	"Coprocessor Fault",
	"Alignment Check",
	"Machine Check",
	"SIMD Floating-Point",
	"Virtualization",
	"Control Protection",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Security",
	"Reserved"
};

void
dump_memory(u64 addr, int count)
{
	u8	*ptr;
	int	i;
	char	buf[3];

	ptr = (u8 *)addr;
	for (i = 0; i < count; i++) {
		if (i > 0 && i % 16 == 0) {
			klog("\n\r");
			klog("\n");
		}

		itoa(ptr[i], buf, 16);

		if (ptr[i] < 16) {
			klog("0");
			klog("0");
		}
		klog("%s ", buf);
		klog("%s ", buf);
	}
	klog("\n\r");
	klog("\n");
}

void
print_stack_trace(u64 rbp)
{
	u64	*frame;
	int	i;

	klog("\n\rStack Trace:\n\r");
	klog("\nStack Trace:\n");

	frame = (u64 *)rbp;
	for (i = 0; i < 8 && frame; i++) {
		u64	rip;

		if ((u64)frame < 0x100000 ||
		    (u64)frame > 0x800000) {
			break;
		}
		rip = frame[1];
		klog("  [%d] %p\n\r", i, (void *)rip);
		klog("  [%d] %p\n", i, (void *)rip);
		frame = (u64 *)frame[0];
	}
}

void
print_panic_logo(void)
{
	const char	*logo;
	int		width, height, x, y, cur_x, cur_y;
	const char	*p;
	kms_console_t	*con;

	logo = "\n" "\n"
	    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@+@@@@@##@@@@@:@@@@@@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@\n"
	    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%#@@@@**@@@@+@@@@@@@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@\n"
	    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@##@@@**@@@*@@@@@@@@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@\n"
	    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@@@@@@@@@@@@@@@@@@@@@@@@%++++****++++++****+++++*@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@@@@@@@@@@@@@@@@@@@@@@@@*#@@@@@%%@@@@@@@%%%@@@@@+%@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@@@@@@@@@@@@@@@@@@@@@@@@+@@@@@:#@:*@@@@==@+=@@@@+#@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@@@@@@@@@@@@@@@@@@@@@@@@+@@@@@:#@:*@@@@==@+=@@@@+#@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@@@@@%%%=---=##%@@@@@@@@**@@@@@@@@@@@@@@@@@@@@@%+%@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@@@#---------:---+%@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@@+--=---------=---#@@@@@@@@@@+#@@@@@@@@@@@@=%@@@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@#--%%%------=%%#---#@@@@@@@@@=@@@@@@@@@@@@@=%@@@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@%----*=--+++---*=---+%@@@@@@@@=%@@@@@@@@@@@@=%@@@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@%-=++**++#%#==#+++=--*@@@@@@@@@%@@@#*****@@%@@@@@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@%-**+**+=-#-=+#++*+---#@@@@@@@@@+@@@@#%@+@@+@#%@@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@%--+*=+****###+=*+-----+%@@@@@@*#@@@=#@@#@#@@@+@@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@%===-----------------------+*#@#%@@@@#%@@%#@@@+@@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@%================================+%@@@*@@*%@@##@@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@#===================================#%@@@@@@@@@@@@@@@"
	    "@@%*===%@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@%*====================================*#@@@@@@@@@@@@%"
	    "#==*%#%@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@@#+=====================================+*%@@@@@@@@%+"
	    "===*%%@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@@@#+========+====+=======++===============++*%%%%%+=="
	    "==++*%@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@@@@%*++++++++++++#*+++++++#*+++++++++++++++++++++++++"
	    "+######%@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@@%#*%%*++++++++++#%*+++++++##++++++++++++++++++++++++"
	    "++++++++*%@@@@@@@"
	    "@@@@@\n"
	    "@@@%#****#%#*+++++++++##*++++++++%*+++++++++++++++++++*++"
	    "+++++++***#@@@@@@"
	    "@@@@@\n"
	    "@%#********#@@##*******#%#**++****%****++*++******##@@@@@"
	    "@%*******###%@@@@"
	    "@@@@@\n"
	    "@@%%%%%%%%@@@@@@@%%####**%%%**#**%%******####%%%@@@@@@@@@"
	    "@@@@@@%%%%%%@@@@@"
	    "@@@@@\n"
	    "@@@@@@@@@@@@@@@@@@@@@@@@@@@##@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@@@@@@@@@@@@@@@@%+++*@@=..+%@@+++*@@*+++*@@@++=+@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@@@@@@@@@@@@@@@@.*@%::@#::@@@.=@@@@=.+@%.=@::%@@@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@@@@@@@@@@@@@@@%.#@@::@%::@@@*::.#@=.*@@.=@#::.+%@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@@@@@@@@@@@@@@@@:=#*:+@%::#@%###:.%+.=#*.*@#**-.*@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n"
	    "@@@@@@@@@@@@@@@@@@@@%%%@@@@@%%@@%%%%@@@@%%%@@@%%%%@@@@@@@"
	    "@@@@@@@@@@@@@@@@@"
	    "@@@@@\n";

	width = console_get_width();
	height = console_get_height();
	x = width - 81;
	if (x < 0) {
		x = 0;
	}
	y = height - 36;
	if (y < 0) {
		y = 0;
	}

	cur_x = x;
	cur_y = y;

	for (p = logo; *p; p++) {
		if (*p == '\n') {
			cur_y++;
			cur_x = x;
		} else {
			con = kms_kernel_console();
			if (con) {
				rapi_console_glyph(con,
				    (u32)(cur_x * 8),
				    (u32)(cur_y * 16), *p,
				    console_color_rgb(0x1F),
				    0x000000);
			}
			cur_x++;
		}
	}
	con = kms_kernel_console();
	if (con) {
		kms_console_flush(con);
	}
}

void
kernel_panic(registers_t *regs)
{
	const char	*msg;
	u64		cr0, cr2, cr3, cr4;
	struct {
		u16	limit;
		u64	base;
	} __attribute__((packed)) gdtr, idtr;

	__asm__ volatile("cli");

	console_set_color(0x1F);
	clear_scr();

	msg = (regs->int_no < 32) ?
	    exception_messages[regs->int_no] :
	    "Unexpected Interrupt";

	klog("\n\r:::::::::::::::::::::::: KERNEL "
	    "PANIC ::::::::::::::::::::::::\n\r");
	klog(":::::::::::::::::::::::: KERNEL PANIC "
	    "::::::::::::::::::::::::\n");

	klog("Exception: %s\n\r", msg);
	klog("Exception: %s\n", msg);

	klog("RIP: %p  CS: %x  RFLAGS: %p  ERR: %x\n\r",
	    (void *)regs->rip, (int)regs->cs,
	    (void *)regs->rflags, (int)regs->err_code);
	klog("RIP: %p  CS: %x  RFLAGS: %p  ERR: %x\n",
	    (void *)regs->rip, (int)regs->cs,
	    (void *)regs->rflags, (int)regs->err_code);

	klog("RAX: %p RBX: %p RCX: %p RDX: %p\n\r",
	    (void *)regs->rax, (void *)regs->rbx,
	    (void *)regs->rcx, (void *)regs->rdx);
	klog("RAX: %p RBX: %p RCX: %p RDX: %p\n",
	    (void *)regs->rax, (void *)regs->rbx,
	    (void *)regs->rcx, (void *)regs->rdx);

	klog("RSI: %p RDI: %p RBP: %p RSP: %p\n\r",
	    (void *)regs->rsi, (void *)regs->rdi,
	    (void *)regs->rbp, (void *)regs->rsp);
	klog("RSI: %p RDI: %p RBP: %p RSP: %p\n",
	    (void *)regs->rsi, (void *)regs->rdi,
	    (void *)regs->rbp, (void *)regs->rsp);

	__asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
	__asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
	__asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
	__asm__ volatile("mov %%cr4, %0" : "=r"(cr4));

	klog("CR0: %p  CR2: %p  CR3: %p  CR4: %p\n\r",
	    (void *)cr0, (void *)cr2, (void *)cr3,
	    (void *)cr4);
	klog("CR0: %p  CR2: %p  CR3: %p  CR4: %p\n",
	    (void *)cr0, (void *)cr2, (void *)cr3,
	    (void *)cr4);

	if (regs->int_no == 14) {
		klog("PF Details: ");
		if (!(regs->err_code & 1)) {
			klog("Not-Present ");
		} else {
			klog("Protection-Violation ");
		}
		if (regs->err_code & 2) {
			klog("Write ");
		}
		if (regs->err_code & 16) {
			klog("Instruction-Fetch ");
		}
		klog("\n\r");

		klog("PF Details: ");
		if (!(regs->err_code & 1)) {
			klog("Not-Present ");
		} else {
			klog("Protection-Violation ");
		}
		if (regs->err_code & 2) {
			klog("Write ");
		}
		if (regs->err_code & 16) {
			klog("Instruction-Fetch ");
		}
		klog("\n");
	}

	__asm__ volatile("sgdt %0" : "=m"(gdtr));
	__asm__ volatile("sidt %0" : "=m"(idtr));
	klog("GDTR: %p (Limit: %x)  IDTR: %p "
	    "(Limit: %x)\n\r",
	    (void *)gdtr.base, gdtr.limit,
	    (void *)idtr.base, idtr.limit);
	klog("GDTR: %p (Limit: %x)  IDTR: %p (Limit: %x)\n",
	    (void *)gdtr.base, gdtr.limit,
	    (void *)idtr.base, idtr.limit);

	klog("\n\rCode dump at RIP:\n\r");
	klog("\nCode dump at RIP:\n");
	dump_memory(regs->rip, 16);

	klog("\n\rStack dump at RSP:\n\r");
	klog("\nStack dump at RSP:\n");
	dump_memory(regs->rsp, 32);

	print_stack_trace(regs->rbp);

	klog("\n\rSystem Halted.\n\r");
	klog("\nSystem Halted.\n");

	terminal_flush_kernel();
	print_panic_logo();

	while (1) {
		__asm__ volatile("hlt");
	}
}

void
panic(const char *format, ...)
{
	__builtin_va_list	args;
	char			buffer[512];
	int			i;
	const char		*p;

	__asm__ volatile("cli");

	console_set_color(0x1F);
	clear_scr();

	klog("\n\r:::::::::::::::::::::::: KERNEL "
	    "PANIC ::::::::::::::::::::::::\n\r");
	klog(":::::::::::::::::::::::: KERNEL PANIC "
	    "::::::::::::::::::::::::\n");

	klog("Message: ");
	klog("Message: ");

	__builtin_va_start(args, format);

	i = 0;
	p = format;

	while (*p && i < 511) {
		if (*p == '%') {
			p++;
			switch (*p) {
			case 's': {
				const char	*str;

				str = __builtin_va_arg(args,
				    const char *);
				while (*str && i < 511) {
					buffer[i++] = *str++;
				}
				break;
			}
			case 'd': {
				int	val, j;
				char	tmp[16];

				val = __builtin_va_arg(args, int);
				if (val < 0) {
					buffer[i++] = '-';
					val = -val;
				}
				j = 0;
				do {
					tmp[j++] = '0' +
					    (val % 10);
					val /= 10;
				} while (val && j < 16);
				while (j > 0 && i < 511) {
					buffer[i++] = tmp[--j];
				}
				break;
			}
			case 'x': {
				u32		val;
				int		j;
				const char	hex[] =
				    "0123456789ABCDEF";
				char		tmp[16];

				val = __builtin_va_arg(args, u32);
				j = 0;
				do {
					tmp[j++] = hex[val % 16];
					val /= 16;
				} while (val && j < 16);
				while (j > 0 && i < 511) {
					buffer[i++] = tmp[--j];
				}
				break;
			}
			case '%':
				buffer[i++] = '%';
				break;
			default:
				buffer[i++] = '%';
				if (i < 511) {
					buffer[i++] = *p;
				}
				break;
			}
		} else {
			buffer[i++] = *p;
		}
		p++;
	}
	buffer[i] = '\0';

	__builtin_va_end(args);

	klog("%s\n\r", buffer);
	klog("%s\n", buffer);

	klog("\n\rSystem Halted.\n\r");
	klog("\nSystem Halted.\n");

	terminal_flush_kernel();
	print_panic_logo();

	while (1) {
		__asm__ volatile("hlt");
	}
}
