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
$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type process_state_t as enum with process states
$define %type process_t as struct with process control block
$define %type api_sysinfo as struct with system info strings
$define %type drm_driver_t as struct with DRM driver vtable

$define %func kshell_set_boot_info as procedure with args int
$define %func process_state_name as function with args process_state_t
$define %func parse_number as function with args const char **, int *
$define %func parse_term as function with args const char **, int *
$define %func parse_expr as function with args const char *, int *
$define %func print_kernel_var as function with args const char *
$define %func kshell_echo_command as function with args int, char *[]

*/

/* !SPACE!

$space %internal process_state_name, parse_number, parse_term
$space %internal parse_expr, print_kernel_var
$space %export kshell_set_boot_info, kshell_echo_command

*/

#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/video/drm/drm.h>
#include <kernel/drivers/video/drm/kms/crtc.h>
#include <kernel/kshell/kshell.h>
#include <kernel/api/api.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <mlibc/mlibc.h>

static int	g_kshell_is_multiboot2;

void
kshell_set_boot_info(int is_multiboot2)
{
	g_kshell_is_multiboot2 = is_multiboot2 ? 1 : 0;
}

static const char *
process_state_name(process_state_t state)
{
	switch (state) {
	case PROC_STATE_UNUSED:
		return ("UNUSED");
	case PROC_STATE_EMBRYO:
		return ("EMBRYO");
	case PROC_STATE_RUNNABLE:
		return ("RUNNABLE");
	case PROC_STATE_RUNNING:
		return ("RUNNING");
	case PROC_STATE_SLEEPING:
		return ("SLEEPING");
	case PROC_STATE_ZOMBIE:
		return ("ZOMBIE");
	default:
		return ("UNKNOWN");
	}
}

static int
parse_number(const char **p, int *ok)
{
	int	value, sign;
	const char	*s;

	value = 0;
	sign = 1;
	s = *p;

	while (*s == ' ' || *s == '\t') {
		s++;
	}

	if (*s == '-') {
		sign = -1;
		s++;
	}

	if (*s < '0' || *s > '9') {
		*ok = 0;
		return (0);
	}

	while (*s >= '0' && *s <= '9') {
		value = (value * 10) + (*s - '0');
		s++;
	}

	*p = s;
	return (value * sign);
}

static int
parse_term(const char **p, int *ok)
{
	int	lhs, rhs;
	char	op;
	const char	*s;

	lhs = parse_number(p, ok);
	s = *p;
	while (*ok) {
		while (*s == ' ' || *s == '\t') {
			s++;
		}
		op = *s;

		if (op != '*' && op != '/') {
			*p = s;
			return (lhs);
		}

		s++;
		rhs = parse_number(&s, ok);
		if (!*ok) {
			*p = s;
			return (0);
		}

		if (op == '*') {
			lhs *= rhs;
		} else {
			if (rhs == 0) {
				*ok = 0;
				*p = s;
				return (0);
			}
			lhs /= rhs;
		}
	}
	*p = s;
	return (lhs);
}

static int
parse_expr(const char *expr, int *ok)
{
	const char	*p;
	int		lhs, rhs;
	char		op;

	p = expr;
	lhs = parse_term(&p, ok);

	while (*ok) {
		while (*p == ' ' || *p == '\t') {
			p++;
		}
		op = *p;

		if (op != '+' && op != '-') {
			break;
		}

		p++;
		rhs = parse_term(&p, ok);
		if (!*ok) {
			return (0);
		}

		if (op == '+') {
			lhs += rhs;
		} else {
			lhs -= rhs;
		}
	}

	while (*p == ' ' || *p == '\t') {
		p++;
	}

	if (*p != '\0') {
		*ok = 0;
		return (0);
	}

	return (lhs);
}

static int
print_kernel_var(const char *name)
{
	const char	*drv;
	int		i;
	u32		count, active;

	if (strcmp(name, "videoM") == 0) {
		if (drm_is_ready()) {
			drv = drm_driver_get_selected_name();
			kshell_console_write(drv ? drv :
			    "unknown");
			kshell_console_write(", address: ");
			kshell_console_write_ptr((void *)(u64)
			    drm_crtc_get_hw_address());
			kshell_console_write(", ");
			kshell_console_write_int((int)
			    drm_crtc_get_width());
			kshell_console_write("x");
			kshell_console_write_int((int)
			    drm_crtc_get_height());
			kshell_console_write("x");
			kshell_console_write_int((int)
			    drm_crtc_get_bpp());
			kshell_console_write(", multiboot");
			kshell_console_write_int(
			    g_kshell_is_multiboot2 ? 2 : 1);
			kshell_console_write("\n");
		} else {
			kshell_console_write("unavailable\n");
		}
		return (0);
	}

	if (strcmp(name, "dkey") == 0) {
		drv = keyboard_get_driver_name();
		if (!drv) {
			drv = "none";
		}
		kshell_console_write("keyboard driver: ");
		kshell_console_write(drv);
		kshell_console_write("\n");
		return (0);
	}

	if (strcmp(name, "prun") == 0) {
		kshell_console_write("PID\tNAME\tSTATE\n");
		for (i = 0; i < MAX_PROCESSES; i++) {
			process_t	*proc;
			thread_t	*td;

			proc = &process_table[i];
			if (proc->pid == 0) {
				continue;
			}
			td = proc->main_thread;
			if (!td) {
				continue;
			}
			kshell_console_write_int(
			    (int)proc->pid);
			kshell_console_write("\t");
			kshell_console_write(proc->name);
			kshell_console_write("\t");
			kshell_console_write(
			    process_state_name(td->state));
			kshell_console_write("\n");
		}
		return (0);
	}

	if (strcmp(name, "uname") == 0) {
		struct api_sysinfo	uts;

		api_info_fill(&uts);
		kshell_console_write("sysname: ");
		kshell_console_write(uts.sysname);
		kshell_console_write("\n");
		kshell_console_write("nodename: ");
		kshell_console_write(uts.nodename);
		kshell_console_write("\n");
		kshell_console_write("release: ");
		kshell_console_write(uts.release);
		kshell_console_write("\n");
		kshell_console_write("version: ");
		kshell_console_write(uts.version);
		kshell_console_write("\n");
		kshell_console_write("machine: ");
		kshell_console_write(uts.machine);
		kshell_console_write("\n");
		kshell_console_write("domainname: ");
		kshell_console_write(uts.domainname);
		kshell_console_write("\n");
		return (0);
	}

	if (strcmp(name, "drm") == 0) {
		kshell_console_write("drm ready: ");
		kshell_console_write(drm_is_ready() ? "yes" :
		    "no");
		kshell_console_write("\n");

		kshell_console_write("active driver: ");
		kshell_console_write(
		    drm_driver_get_selected_name());
		kshell_console_write(" (id=");
		kshell_console_write_int(
		    drm_driver_get_selected_index());
		kshell_console_write(")\n");

		if (drm_is_ready()) {
			kshell_console_write("mode: ");
			kshell_console_write_int((int)
			    drm_crtc_get_width());
			kshell_console_write("x");
			kshell_console_write_int((int)
			    drm_crtc_get_height());
			kshell_console_write(", bpp=");
			kshell_console_write_int((int)
			    drm_crtc_get_bpp());
			kshell_console_write(", pitch=");
			kshell_console_write_int((int)
			    drm_crtc_get_pitch());
			kshell_console_write(", hw=");
			kshell_console_write_ptr((void *)(u64)
			    drm_crtc_get_hw_address());
			kshell_console_write("\n");
		}

		kshell_console_write("drivers:\n");
		count = drm_driver_count();
		if (count == 0) {
			kshell_console_write("  <none>\n");
			return (0);
		}

		active = (u32)drm_driver_get_selected_index();
		for (i = 0; i < (int)count; i++) {
			const drm_driver_t	*d;

			d = drm_driver_get_by_index((u32)i);
			if (!d) {
				continue;
			}
			kshell_console_write("  [");
			kshell_console_write_int(i);
			kshell_console_write("] ");
			kshell_console_write(d->name ? d->name :
			    "unnamed");
			kshell_console_write(", priority=");
			kshell_console_write_int(d->priority);
			if ((u32)i == active) {
				kshell_console_write(" (active)");
			}
			kshell_console_write("\n");
		}
		return (0);
	}

	return (-1);
}

int
kshell_echo_command(int argc, char *argv[])
{
	char	expr_buf[KSHELL_MAX_LINE];
	int	ok, pos, value, i;

	if (argc <= 1) {
		kshell_console_write("\n");
		return (0);
	}

	if (argv[1][0] == '&') {
		pos = 0;
		value = 0;
		ok = 1;

		for (i = 1; i < argc && pos < (KSHELL_MAX_LINE - 1);
		    i++) {
			const char	*src;

			src = argv[i];
			if (i == 1 && src[0] == '&') {
				src++;
			}

			while (*src != '\0' &&
			    pos < (KSHELL_MAX_LINE - 1)) {
				expr_buf[pos++] = *src++;
			}

			if (i + 1 < argc &&
			    pos < (KSHELL_MAX_LINE - 1)) {
				expr_buf[pos++] = ' ';
			}
		}
		expr_buf[pos] = '\0';

		value = parse_expr(expr_buf, &ok);
		if (!ok) {
			kshell_console_write("echo: invalid math "
			    "expression\n");
			return (-1);
		}
		kshell_console_write_int(value);
		kshell_console_write("\n");
		return (0);
	}

	if (argv[1][0] == '%' && argv[1][1] != '\0') {
		if (print_kernel_var(&argv[1][1]) != 0) {
			kshell_console_write("echo: unknown kernel "
			    "variable: ");
			kshell_console_write(argv[1]);
			kshell_console_write("\n");
			return (-1);
		}
		return (0);
	}

	for (i = 1; i < argc; i++) {
		kshell_console_write(argv[i]);
		if (i + 1 < argc) {
			kshell_console_write(" ");
		}
	}
	kshell_console_write("\n");
	return (0);
}
