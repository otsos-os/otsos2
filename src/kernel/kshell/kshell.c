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
$define %type kms_console_t as struct with KMS console state

$define %func kshell_width as function with args void
$define %func kshell_height as function with args void
$define %func kshell_sync_cells as procedure with args void
$define %func kshell_con as function with args void
$define %func kshell_draw_cell as procedure with args int, int, char, u8
$define %func kshell_redraw as procedure with args void
$define %func kshell_set_visible as procedure with args int
$define %func kshell_put_entry_at as procedure with args char, int, int
$define %func kshell_console_clear as procedure with args void
$define %func kshell_newline as procedure with args void
$define %func kshell_console_putc as procedure with args char
$define %func kshell_console_write as procedure with args const char *
$define %func kshell_console_write_int as procedure with args int
$define %func kshell_console_write_ptr as procedure with args const void *
$define %func kshell_request_open as procedure with args void
$define %func kshell_try_open_if_requested as function with args void
$define %func kshell_capture_begin as procedure with args char *, u32
$define %func kshell_capture_end as function with args int *
$define %func kshell_help_list as procedure with args void
$define %func kshell_help_command as procedure with args const char *
$define %func kshell_execute_core as function with args int, char *[]
$define %func kshell_execute as function with args int, char *[]
$define %func kshell_discard_keyboard_buffer as procedure with args void
$define %func kshell_run as procedure with args void

*/

/* !SPACE!

$space %internal kshell_width, kshell_height, kshell_sync_cells
$space %internal kshell_con, kshell_draw_cell, kshell_redraw
$space %internal kshell_set_visible, kshell_put_entry_at
$space %internal kshell_newline, kshell_capture_begin
$space %internal kshell_capture_end, kshell_help_list
$space %internal kshell_help_command, kshell_execute_core
$space %internal kshell_execute, kshell_discard_keyboard_buffer
$space %export kshell_console_clear, kshell_console_putc
$space %export kshell_console_write, kshell_console_write_int
$space %export kshell_console_write_ptr, kshell_request_open
$space %export kshell_try_open_if_requested, kshell_run

*/

#include <kernel/console.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/drivers/tty.h>
#include <kernel/drivers/video/drm/drm.h>
#include <kernel/drivers/video/drm/kms/console.h>
#include <kernel/kshell/kshell.h>
#include <mlibc/mlibc.h>

static volatile int	kshell_open_requested;
static int		kshell_is_active;
static int		kshell_visible;
static int		kshell_cursor_x;
static int		kshell_cursor_y;
static u8		kshell_color = 0x0F;
static u16		*kshell_cells;
static int		kshell_cells_w;
static int		kshell_cells_h;
static char		*kshell_capture_buf;
static u32		kshell_capture_cap;
static u32		kshell_capture_len;
static int		kshell_capture_overflow;
static kms_console_t	*g_con;

static int
kshell_width(void)
{
	int	w;

	w = console_get_width();
	if (w <= 0) {
		w = 80;
	}
	return (w);
}

static int
kshell_height(void)
{
	int	h;

	h = console_get_height();
	if (h <= 0) {
		h = 25;
	}
	return (h);
}

static void
kshell_sync_cells(void)
{
	int	w, h, i;
	u16	blank;

	w = kshell_width();
	h = kshell_height();

	if (kshell_cells && kshell_cells_w == w &&
	    kshell_cells_h == h) {
		return;
	}

	if (kshell_cells) {
		kmem_free(kshell_cells);
		kshell_cells = NULL;
	}

	kshell_cells = (u16 *)kmem_calloc(
	    (unsigned long)(w * h), sizeof(u16));
	kshell_cells_w = w;
	kshell_cells_h = h;

	if (kshell_cells) {
		blank = ((u16)kshell_color << 8) | ' ';
		for (i = 0; i < w * h; i++) {
			kshell_cells[i] = blank;
		}
	}

	kshell_cursor_x = 0;
	kshell_cursor_y = 0;
}

static kms_console_t *
kshell_con(void)
{
	if (g_con) {
		return (g_con);
	}
	g_con = kms_kernel_console();
	return (g_con);
}

static void
kshell_draw_cell(int x, int y, char c, u8 color)
{
	kms_console_t	*con;

	con = kshell_con();
	if (!con) {
		return;
	}
	kms_console_glyph(con, (u32)(x * 8), (u32)(y * 16), c,
	    console_color_rgb(color), 0x000000);
}

static void
kshell_redraw(void)
{
	int	x, y;

	kshell_sync_cells();
	if (!kshell_cells) {
		return;
	}

	for (y = 0; y < kshell_cells_h; y++) {
		for (x = 0; x < kshell_cells_w; x++) {
			u16	cell;
			char	c;
			u8	color;

			cell = kshell_cells[y *
			    kshell_cells_w + x];
			c = (char)(cell & 0xFF);
			color = (u8)((cell >> 8) & 0xFF);
			kshell_draw_cell(x, y, c, color);
		}
	}
	if (g_con) {
		kms_console_flush(g_con);
	}
}

static void
kshell_set_visible(int visible)
{
	if (kshell_visible == visible) {
		return;
	}
	kshell_visible = visible;
	if (kshell_visible) {
		kshell_redraw();
	} else {
		tty_restore_active_display();
	}
}

static void
kshell_put_entry_at(char c, int x, int y)
{
	kshell_sync_cells();
	if (x >= 0 && y >= 0 && x < kshell_cells_w &&
	    y < kshell_cells_h && kshell_cells) {
		kshell_cells[y * kshell_cells_w + x] =
		    ((u16)kshell_color << 8) | (u8)c;
	}
	if (kshell_visible) {
		kshell_draw_cell(x, y, c, kshell_color);
	}
}

void
kshell_console_clear(void)
{
	int	w, h, x, y;

	w = kshell_width();
	h = kshell_height();
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			kshell_put_entry_at(' ', x, y);
		}
	}
	kshell_cursor_x = 0;
	kshell_cursor_y = 0;
	if (g_con) {
		kms_console_flush(g_con);
	}
}

static void
kshell_newline(void)
{
	kshell_cursor_x = 0;
	kshell_cursor_y++;
	if (kshell_cursor_y >= kshell_height()) {
		kshell_console_clear();
	}
}

void
kshell_console_putc(char c)
{
	if (kshell_capture_buf) {
		if (c == '\r') {
			return;
		}
		if (kshell_capture_len < kshell_capture_cap) {
			kshell_capture_buf[kshell_capture_len++] =
			    c;
		} else {
			kshell_capture_overflow = 1;
		}
		return;
	}

	if (c == '\n') {
		kshell_newline();
		if (kshell_visible && g_con) {
			kms_console_flush(g_con);
		}
		return;
	}

	if (c == '\r') {
		kshell_cursor_x = 0;
		if (kshell_visible && g_con) {
			kms_console_flush(g_con);
		}
		return;
	}

	if (c == '\b') {
		if (kshell_cursor_x > 0) {
			kshell_cursor_x--;
			kshell_put_entry_at(' ',
			    kshell_cursor_x, kshell_cursor_y);
		}
		if (kshell_visible && g_con) {
			kms_console_flush(g_con);
		}
		return;
	}

	if (c < 32 || c > 126) {
		return;
	}

	kshell_put_entry_at(c, kshell_cursor_x,
	    kshell_cursor_y);
	kshell_cursor_x++;
	if (kshell_cursor_x >= kshell_width()) {
		kshell_newline();
		if (kshell_visible && g_con) {
			kms_console_flush(g_con);
		}
	} else if (kshell_visible && g_con) {
		kms_console_flush(g_con);
	}
}

void
kshell_console_write(const char *s)
{
	int	i;

	if (!s) {
		return;
	}
	for (i = 0; s[i] != '\0'; i++) {
		kshell_console_putc(s[i]);
	}
}

void
kshell_console_write_int(int value)
{
	char	buf[32];

	itoa(value, buf, 10);
	kshell_console_write(buf);
}

void
kshell_console_write_ptr(const void *ptr)
{
	const char	*hex;
	u64		value;
	int		i;

	hex = "0123456789ABCDEF";
	value = (u64)ptr;
	kshell_console_write("0x");
	for (i = 15; i >= 0; i--) {
		u8	nibble;

		nibble = (u8)((value >> (i * 4)) & 0xF);
		kshell_console_putc(hex[nibble]);
	}
}

void
kshell_request_open(void)
{
	kshell_open_requested = 1;
}

int
kshell_try_open_if_requested(void)
{
	if (!kshell_open_requested) {
		return (0);
	}

	kshell_open_requested = 0;
	if (!kshell_is_active) {
		kshell_is_active = 1;
		kshell_set_visible(1);
		kshell_run();
		kshell_set_visible(0);
		kshell_is_active = 0;
		return (1);
	}

	kshell_set_visible(!kshell_visible);
	return (1);
}

static void
kshell_capture_begin(char *buf, u32 cap)
{
	kshell_capture_buf = buf;
	kshell_capture_cap = cap;
	kshell_capture_len = 0;
	kshell_capture_overflow = 0;
}

static u32
kshell_capture_end(int *overflow)
{
	if (overflow) {
		*overflow = kshell_capture_overflow;
	}
	kshell_capture_buf = NULL;
	kshell_capture_cap = 0;
	return (kshell_capture_len);
}

static void
kshell_help_list(void)
{
	kshell_console_write("commands:\n");
	kshell_console_write("  help\n");
	kshell_console_write("  clear\n");
	kshell_console_write("  echo\n");
	kshell_console_write("  drm_switch\n");
	kshell_console_write("  exit\n");
	kshell_console_write("redirection:\n");
	kshell_console_write("  <command> > /absolute/path\n");
	kshell_console_write("use: help <command>\n");
}

static void
kshell_help_command(const char *cmd)
{
	if (strcmp(cmd, "help") == 0) {
		kshell_console_write("help\n");
		kshell_console_write("  usage: help "
		    "[command]\n");
		kshell_console_write("  show command list or "
		    "detailed help for one command\n");
		kshell_console_write("  supports output redirect: "
		    "> /absolute/path\n");
		return;
	}

	if (strcmp(cmd, "clear") == 0) {
		kshell_console_write("clear\n");
		kshell_console_write("  usage: clear\n");
		kshell_console_write("  clear kshell screen\n");
		return;
	}

	if (strcmp(cmd, "echo") == 0) {
		kshell_console_write("echo\n");
		kshell_console_write("  usage: echo <text>\n");
		kshell_console_write("  usage: echo "
		    "&<math expression>\n");
		kshell_console_write("  usage: echo %%videoM | "
		    "%%dkey | %%prun | %%uname | %%drm\n");
		kshell_console_write("  prints text, evaluates "
		    "math, or prints kernel value\n");
		return;
	}

	if (strcmp(cmd, "drm_switch") == 0) {
		kshell_console_write("drm_switch\n");
		kshell_console_write("  usage: drm_switch "
		    "<id>\n");
		kshell_console_write("  switch active DRM driver "
		    "by list id from echo %%drm\n");
		return;
	}

	if (strcmp(cmd, "exit") == 0) {
		kshell_console_write("exit\n");
		kshell_console_write("  usage: exit\n");
		kshell_console_write("  close kshell\n");
		return;
	}

	kshell_console_write("help: unknown command: ");
	kshell_console_write(cmd);
	kshell_console_write("\n");
}

static int
kshell_execute_core(int argc, char *argv[])
{
	if (argc == 0) {
		return (0);
	}

	if (strcmp(argv[0], "help") == 0) {
		if (argc == 1) {
			kshell_help_list();
		} else {
			kshell_help_command(argv[1]);
		}
		return (0);
	}

	if (strcmp(argv[0], "clear") == 0) {
		kshell_console_clear();
		return (0);
	}

	if (strcmp(argv[0], "echo") == 0) {
		return (kshell_echo_command(argc, argv));
	}

	if (strcmp(argv[0], "drm_switch") == 0) {
		return (kshell_drm_switch_command(argc, argv));
	}

	if (strcmp(argv[0], "exit") == 0) {
		return (1);
	}

	kshell_console_write("kshell: unknown command: ");
	kshell_console_write(argv[0]);
	kshell_console_write("\n");
	return (-1);
}

static int
kshell_execute(int argc, char *argv[])
{
	int	cmd_argc, redir_index, i, rc, overflow;
	const char	*redir_path;
	char	capture_buf[4096];
	u32	out_size;

	cmd_argc = argc;
	redir_index = -1;
	redir_path = NULL;

	for (i = 0; i < argc; i++) {
		if (argv[i][0] != '>') {
			continue;
		}

		redir_index = i;
		if (argv[i][1] != '\0') {
			redir_path = &argv[i][1];
			if (i + 1 != argc) {
				kshell_console_write("kshell: "
				    "unexpected tokens after "
				    "redirection\n");
				return (-1);
			}
		} else {
			if (i + 1 >= argc) {
				kshell_console_write("kshell: "
				    "missing file path after "
				    "'>'\n");
				return (-1);
			}
			redir_path = argv[i + 1];
			if (i + 2 != argc) {
				kshell_console_write("kshell: "
				    "unexpected tokens after "
				    "redirection\n");
				return (-1);
			}
		}
		cmd_argc = redir_index;
		break;
	}

	if (!redir_path) {
		return (kshell_execute_core(cmd_argc, argv));
	}

	if (cmd_argc == 0) {
		kshell_console_write("kshell: empty command "
		    "before redirection\n");
		return (-1);
	}

	if (redir_path[0] != '/') {
		kshell_console_write("kshell: redirection path "
		    "must be absolute\n");
		return (-1);
	}

	kshell_capture_begin(capture_buf, sizeof(capture_buf));
	rc = kshell_execute_core(cmd_argc, argv);
	overflow = 0;
	out_size = kshell_capture_end(&overflow);

	if (chainfs_write_file(redir_path,
	    (const u8 *)capture_buf, out_size) != 0) {
		kshell_console_write("kshell: failed to write "
		    "redirection file: ");
		kshell_console_write(redir_path);
		kshell_console_write("\n");
		return (-1);
	}

	if (overflow) {
		kshell_console_write("kshell: output truncated "
		    "while redirecting\n");
	}

	return (rc);
}

static void
kshell_discard_keyboard_buffer(void)
{
	while (keyboard_getchar() != 0) {
	}
}

void
kshell_run(void)
{
	char	line[KSHELL_MAX_LINE];
	char	*argv[KSHELL_MAX_ARGS];
	int	len, rc;
	char	c;

	kshell_set_visible(1);
	kshell_console_clear();
	kshell_console_write("Entering kshell (ring0). Type "
	    "'help'.\n");
	kshell_console_write("Hotkey Ctrl+Shift+Backspace "
	    "toggles show/hide.\n");

	while (1) {
		len = 0;
		rc = 0;

		memset(line, 0, sizeof(line));
		kshell_console_write("kshell> ");

		while (1) {
			c = 0;

			while (1) {
				if (!kshell_visible) {
					__asm__ volatile("hlt");
					continue;
				}
				c = keyboard_getchar();
				if (c != 0) {
					break;
				}
				__asm__ volatile("hlt");
			}

			if (c == '\r' || c == '\n') {
				kshell_console_putc('\n');
				break;
			}

			if (c == '\b' || c == 0x7F) {
				if (len > 0) {
					len--;
					line[len] = '\0';
					kshell_console_putc('\b');
				}
				continue;
			}

			if (c >= 32 && c < 127) {
				if (len < (KSHELL_MAX_LINE - 1)) {
					line[len++] = c;
					kshell_console_putc(c);
				}
			}
		}

		rc = kshell_execute(
		    kshell_parse_line(line, argv,
		    KSHELL_MAX_ARGS), argv);
		if (rc == 1) {
			break;
		}
	}

	kshell_discard_keyboard_buffer();
	keyboard_reset_state();
	kshell_open_requested = 0;
	kshell_set_visible(0);
}
