/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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

$define %type char as 8 bit signed
$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed

$define %type terminal_state_t as struct with VT cell grid and ANSI state
$define %type terminal_line_buf_t as struct with line discipline buffer
$define %type terminal_input_queue_t as struct with keyboard input ring

$define %func terminal_state_is_valid as function with args int
$define %func terminal_con as function with args void
$define %func terminal_draw_cell as procedure with args int, int, char, u8
$define %func terminal_set_hw_cursor as procedure with args const state ptr
$define %func terminal_ansi_sgr as procedure with args state ptr, int ptr, int
$define %func terminal_erase_line as procedure with args state ptr, int, int
$define %func terminal_erase_display as procedure with args state ptr, int, int
$define %func terminal_ansi_param as function with args state ptr, int, int
$define %func terminal_ansi_execute as procedure with args state ptr, char, int
$define %func terminal_redraw as procedure with args const state ptr
$define %func terminal_scroll as procedure with args state ptr, int
$define %func terminal_putc_internal as procedure with args state ptr, char, int
$define %func terminal_emit as procedure with args char
$define %func terminal_emit_backspace as procedure with args void
$define %func terminal_request_switch as procedure with args int
$define %func terminal_draw_indicator as procedure with args int
$define %func terminal_switch_to as procedure with args int
$define %func terminal_numpad_digit as function with args u8, int
$define %func terminal_scancode_callback as procedure with args u8, int, int
$define %func terminal_lazy_init as procedure with args void
$define %func terminal_pump_keyboard as procedure with args void
$define %func terminal_getchar_blocking as function with args int
$define %func terminal_emit_to as procedure with args int, char
$define %func terminal_fill_line_buffer as procedure with args int
$define %func terminal_reset_one as procedure with args int
$define %func terminal_read as function with args void *, u32
$define %func terminal_write as function with args const void *, u32
$define %func terminal_init as procedure with args void
$define %func terminal_is_initialized as function with args void
$define %func terminal_reinit as procedure with args void
$define %func terminal_putc_from_kernel as procedure with args char
$define %func terminal_flush_kernel as procedure with args void
$define %func terminal_set_color as procedure with args u8
$define %func terminal_clear_active as procedure with args void
$define %func terminal_log_mirror as procedure with args char
$define %func terminal_putc_to as procedure with args int, char
$define %func terminal_puts_to as procedure with args int, const char *
$define %func terminal_set_active as procedure with args int
$define %func terminal_restore_active_display as procedure with args void
$define %func terminal_update as procedure with args void
$define %func terminal_get_input_channel as function with args void
$define %func terminal_power_get as function with args int
$define %func terminal_power_set as function with args int, int
$define %func terminal_power_reset as function with args int
$define %func terminal_power_suspend_all as function with args void

*/

/* !SPACE!

$space %internal terminal_state_is_valid, terminal_con, terminal_draw_cell
$space %internal terminal_set_hw_cursor, terminal_ansi_sgr
$space %internal terminal_erase_line, terminal_erase_display
$space %internal terminal_ansi_param, terminal_ansi_execute
$space %internal terminal_redraw, terminal_scroll, terminal_putc_internal
$space %internal terminal_emit, terminal_emit_backspace
$space %internal terminal_request_switch, terminal_draw_indicator
$space %internal terminal_switch_to, terminal_numpad_digit
$space %internal terminal_scancode_callback, terminal_lazy_init
$space %internal terminal_pump_keyboard, terminal_getchar_blocking
$space %internal terminal_emit_to, terminal_fill_line_buffer
$space %internal terminal_reset_one
$space %export terminal_read, terminal_write, terminal_init
$space %export terminal_is_initialized, terminal_reinit
$space %export terminal_putc_from_kernel, terminal_flush_kernel
$space %export terminal_set_color, terminal_clear_active, terminal_log_mirror
$space %export terminal_putc_to, terminal_puts_to
$space %export terminal_set_active, terminal_restore_active_display
$space %export terminal_update, terminal_get_input_channel
$space %export terminal_power_get, terminal_power_set, terminal_power_reset
$space %export terminal_power_suspend_all

*/

#include <kernel/api/errno.h>
#include <kernel/console/palette.h>
#include <kernel/console/terminal.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/timer.h>
#include <kernel/drivers/uart/uart.h>
#include <kernel/drivers/video/drm/drm.h>
#include <kernel/drivers/video/drm/rapi/rapi.h>
#include <kernel/drivers/console/kms_console.h>
#include <kernel/event/event.h>
#include <kernel/process.h>
#include <mlibc/mlibc.h>

#define	TERM_COUNT		10
#define	TERM_LINE_BUF_SIZE	256

static void	terminal_lazy_init(void);
void		terminal_update(void);
static void	*terminal_input_channel = &terminal_input_channel;
typedef struct {
	u16	*cells;
	int	width;
	int	height;
	int	cursor_x;
	int	cursor_y;
	u8	color;
	u32	fg_rgb;
	int	ansi_state;
	int	ansi_params[8];
	int	ansi_param_count;
	int	ansi_cur_param;
	int	state;	/*active/suspend/disabled */
} terminal_state_t;

typedef struct {
	char	data[TERM_LINE_BUF_SIZE];
	u32	len;
	u32	read_pos;
} terminal_line_buf_t;

typedef struct {
	char	buf[1024];
	int	head;
	int	tail;
} terminal_input_queue_t;

static terminal_state_t		terminals[TERM_COUNT];
static terminal_line_buf_t	terminal_line_bufs[TERM_COUNT];
static terminal_input_queue_t	terminal_inputs[TERM_COUNT];
static int			terminal_active = 0;
static int			terminal_initialized = 0;
static volatile int		terminal_switch_pending = -1;
static int			terminal_ctrl_down = 0;
static int			terminal_suppress_com1_mirror = 0;

static u64	terminal_indicator_end_time = 0;
static int	terminal_indicator_active = 0;

static int
terminal_state_is_valid(int state)
{
	return (state == TERM_STATE_ACTIVE || state == TERM_STATE_SUSPENDED ||
	    state == TERM_STATE_DISABLED);
}

static kms_console_t	*g_con;

static kms_console_t *
terminal_con(void)
{
	if (g_con) {
		return (g_con);
	}
	g_con = kms_kernel_console();
	return (g_con);
}

static void
terminal_draw_cell(int x, int y, char c, u8 color)
{
	kms_console_t	*con;
	terminal_state_t *tty;
	u32		 rgb;

	con = terminal_con();
	if (!con) {
		return;
	}

	tty = &terminals[terminal_active];
	if (tty->fg_rgb != 0xFFFFFFFF) {
		rgb = tty->fg_rgb;
	} else {
		rgb = console_palette[color & 0x0F];
	}
	rapi_console_glyph(con, (u32)(x * 8), (u32)(y * 16), c, rgb, 0x000000);
}

static void
terminal_set_hw_cursor(const terminal_state_t *tty)
{
	(void)tty;
}

static void
terminal_ansi_sgr(terminal_state_t *tty, int params[], int count)
{
	terminal_state_t *self;
	int		  i;
	int		  code;
	int		  r;
	int		  g;
	int		  b;
	u8		  color_idx;

	self = tty;
	i = 0;
	while (i < count) {
		code = params[i];

		if (code == 38 && i + 3 < count && params[i + 1] == 2) {
			r = params[i + 2];
			g = params[i + 3];
			b = params[i + 4];
			self->fg_rgb = ((u32)r << 16) | ((u32)g << 8) | (u32)b;
			i += 5;
			continue;
		}

		if (code == 39) {
			self->fg_rgb = 0xFFFFFFFF;
			self->color = 0x07;
			i++;
			continue;
		}

		color_idx = 0x07;
		switch (code) {
		case 0:
			color_idx = 0x07;
			break;
		case 30:
			color_idx = 0x00;
			break;
		case 31:
			color_idx = 0x04;
			break;
		case 32:
			color_idx = 0x02;
			break;
		case 33:
			color_idx = 0x0E;
			break;
		case 34:
			color_idx = 0x01;
			break;
		case 35:
			color_idx = 0x05;
			break;
		case 36:
			color_idx = 0x03;
			break;
		case 37:
			color_idx = 0x0F;
			break;
		case 90:
			color_idx = 0x08;
			break;
		case 91:
			color_idx = 0x0C;
			break;
		case 92:
			color_idx = 0x0A;
			break;
		case 93:
			color_idx = 0x0E;
			break;
		case 94:
			color_idx = 0x09;
			break;
		case 95:
			color_idx = 0x0D;
			break;
		case 96:
			color_idx = 0x0B;
			break;
		case 97:
			color_idx = 0x0F;
			break;
		default:
			i++;
			continue;
		}
		self->fg_rgb = 0xFFFFFFFF;
		self->color = color_idx;
		i++;
	}
}

static void
terminal_erase_line(terminal_state_t *tty, int mode, int active)
{
	terminal_state_t	*self;
	u16			 blank;
	int			 y;
	int			 from;
	int			 to;
	int			 x;

	self = tty;
	if (!self || !self->cells) {
		return;
	}

	y = self->cursor_y;
	if (y < 0 || y >= self->height) {
		return;
	}

	from = 0;
	to = 0;
	switch (mode) {
	case 0:
		from = self->cursor_x;
		to = self->width;
		break;
	case 1:
		from = 0;
		to = self->cursor_x + 1;
		break;
	case 2:
		from = 0;
		to = self->width;
		break;
	default:
		return;
	}

	blank = ((u16)self->color << 8) | ' ';
	for (x = from; x < to && x < self->width; x++) {
		self->cells[y * self->width + x] = blank;
		if (active) {
			terminal_draw_cell(x, y, ' ', self->color);
		}
	}
}

static void
terminal_erase_display(terminal_state_t *tty, int mode, int active)
{
	terminal_state_t	*self;
	u16			 blank;
	int			 y;
	int			 x;

	self = tty;
	if (!self || !self->cells) {
		return;
	}

	blank = ((u16)self->color << 8) | ' ';
	switch (mode) {
	case 0:
		terminal_erase_line(self, 0, active);
		for (y = self->cursor_y + 1; y < self->height; y++) {
			for (x = 0; x < self->width; x++) {
				self->cells[y * self->width + x] = blank;
				if (active) {
					terminal_draw_cell(x, y, ' ', self->color);
				}
			}
		}
		break;
	case 1:
		for (y = 0; y < self->cursor_y; y++) {
			for (x = 0; x < self->width; x++) {
				self->cells[y * self->width + x] = blank;
				if (active) {
					terminal_draw_cell(x, y, ' ', self->color);
				}
			}
		}
		terminal_erase_line(self, 1, active);
		break;
	case 2:
		for (y = 0; y < self->height; y++) {
			for (x = 0; x < self->width; x++) {
				self->cells[y * self->width + x] = blank;
				if (active) {
					terminal_draw_cell(x, y, ' ', self->color);
				}
			}
		}
		self->cursor_x = 0;
		self->cursor_y = 0;
		break;
	}
	if (active) {
		terminal_set_hw_cursor(self);
	}
}

static int
terminal_ansi_param(terminal_state_t *tty, int idx, int def)
{
	if (idx < tty->ansi_param_count) {
		return (tty->ansi_params[idx]);
	}
	return (def);
}

static void
terminal_ansi_execute(terminal_state_t *tty, char cmd, int active)
{
	terminal_state_t	*self;
	int			 n;
	int			 row;
	int			 col;
	int			 mode;

	self = tty;
	switch (cmd) {
	case 'A':
		n = terminal_ansi_param(self, 0, 1);
		if (n <= 0) {
			n = 1;
		}
		self->cursor_y -= n;
		if (self->cursor_y < 0) {
			self->cursor_y = 0;
		}
		break;
	case 'B':
		n = terminal_ansi_param(self, 0, 1);
		if (n <= 0) {
			n = 1;
		}
		self->cursor_y += n;
		if (self->cursor_y >= self->height) {
			self->cursor_y = self->height - 1;
		}
		break;
	case 'C':
		n = terminal_ansi_param(self, 0, 1);
		if (n <= 0) {
			n = 1;
		}
		self->cursor_x += n;
		if (self->cursor_x >= self->width) {
			self->cursor_x = self->width - 1;
		}
		break;
	case 'D':
		n = terminal_ansi_param(self, 0, 1);
		if (n <= 0) {
			n = 1;
		}
		self->cursor_x -= n;
		if (self->cursor_x < 0) {
			self->cursor_x = 0;
		}
		break;
	case 'H':
	case 'f':
		row = terminal_ansi_param(self, 0, 1);
		col = terminal_ansi_param(self, 1, 1);
		if (row <= 0) {
			row = 1;
		}
		if (col <= 0) {
			col = 1;
		}
		self->cursor_y = row - 1;
		self->cursor_x = col - 1;
		if (self->cursor_y >= self->height) {
			self->cursor_y = self->height - 1;
		}
		if (self->cursor_x >= self->width) {
			self->cursor_x = self->width - 1;
		}
		break;
	case 'J':
		mode = terminal_ansi_param(self, 0, 0);
		terminal_erase_display(self, mode, active);
		return;
	case 'K':
		mode = terminal_ansi_param(self, 0, 0);
		terminal_erase_line(self, mode, active);
		return;
	case 'm':
		terminal_ansi_sgr(self, self->ansi_params, self->ansi_param_count);
		break;
	}
	terminal_set_hw_cursor(self);
}

static void
terminal_redraw(const terminal_state_t *tty)
{
	const terminal_state_t	*self;
	u16			 cell;
	char			 c;
	u8			 color;
	int			 y;
	int			 x;

	self = tty;
	if (!self || !self->cells) {
		return;
	}

	for (y = 0; y < self->height; y++) {
		for (x = 0; x < self->width; x++) {
			cell = self->cells[y * self->width + x];
			c = (char)(cell & 0xFF);
			color = (u8)((cell >> 8) & 0xFF);
			terminal_draw_cell(x, y, c, color);
		}
	}
	if (g_con) {
		kms_console_flush(g_con);
	}

	terminal_set_hw_cursor(self);
}

static void
terminal_scroll(terminal_state_t *tty, int active)
{
	terminal_state_t	*self;
	kms_console_t		*con;
	u16			 blank;
	int			 y;
	int			 x;

	self = tty;
	if (!self || !self->cells) {
		return;
	}

	if (self->height <= 1) {
		return;
	}

	for (y = 1; y < self->height; y++) {
		memcpy(&self->cells[(y - 1) * self->width],
		    &self->cells[y * self->width],
		    self->width * sizeof(u16));
	}

	blank = ((u16)self->color << 8) | ' ';
	for (x = 0; x < self->width; x++) {
		self->cells[(self->height - 1) * self->width + x] = blank;
	}

	if (active) {
		con = terminal_con();
		if (con) {
			rapi_console_scroll_up(con, 16, 0x000000);
		}
	}
}

static void
terminal_putc_internal(terminal_state_t *tty, char c, int active)
{
	terminal_state_t	*self;
	int			 x;
	int			 y;

	self = tty;
	if (!self || !self->cells) {
		return;
	}

	if (self->ansi_state == 0) {
		if (c == 0x1B) {
			self->ansi_state = 1;
			return;
		}
	} else if (self->ansi_state == 1) {
		if (c == '[') {
			self->ansi_state = 2;
			self->ansi_param_count = 0;
			self->ansi_cur_param = 0;
			return;
		}
		self->ansi_state = 0;
	} else if (self->ansi_state == 2) {
		if (c >= '0' && c <= '9') {
			self->ansi_cur_param = self->ansi_cur_param * 10 + (c - '0');
			return;
		}
		if (c == ';') {
			if (self->ansi_param_count < 8) {
				self->ansi_params[self->ansi_param_count++] =
				    self->ansi_cur_param;
			}
			self->ansi_cur_param = 0;
			return;
		}
		if (self->ansi_param_count < 8) {
			self->ansi_params[self->ansi_param_count++] =
			    self->ansi_cur_param;
		}
		terminal_ansi_execute(self, c, active);
		self->ansi_state = 0;
		return;
	}

	if (c == '\n') {
		self->cursor_x = 0;
		self->cursor_y++;
	} else if (c == '\r') {
		self->cursor_x = 0;
	} else if (c == '\b') {
		if (self->cursor_x > 0) {
			self->cursor_x--;
		} else if (self->cursor_y > 0) {
			self->cursor_y--;
			self->cursor_x = self->width - 1;
		}
	} else {
		x = self->cursor_x;
		y = self->cursor_y;
		if (x >= 0 && y >= 0 && x < self->width && y < self->height) {
			self->cells[y * self->width + x] =
			    ((u16)self->color << 8) | (u8)c;
			if (active) {
				terminal_draw_cell(x, y, c, self->color);
			}
		}
		self->cursor_x++;
	}

	if (self->cursor_x >= self->width) {
		self->cursor_x = 0;
		self->cursor_y++;
	}

	if (self->cursor_y >= self->height) {
		terminal_scroll(self, active);
		self->cursor_y = self->height - 1;
	}

	if (active) {
		terminal_set_hw_cursor(self);
	}
}

static void
terminal_emit(char c)
{
	terminal_putc_internal(&terminals[terminal_active], c, 1);
	terminal_suppress_com1_mirror = 1;
	uart_write_byte((u8)c);
	terminal_suppress_com1_mirror = 0;
}

static void
terminal_emit_backspace(void)
{
	terminal_emit('\b');
	terminal_emit(' ');
	terminal_emit('\b');
}

static void
terminal_request_switch(int index)
{
	if (index < 0 || index >= TERM_COUNT) {
		return;
	}
	terminal_switch_pending = index;
}

static void
terminal_draw_indicator(int index)
{
	terminal_state_t	*term;
	char			 buf[16];
	int			 x;
	int			 y;
	int			 terminal_num;
	int			 i;

	term = &terminals[index];
	x = term->width - 15;
	y = 0;
	if (x < 0) {
		x = 0;
	}

	buf[0] = 'V';
	buf[1] = 'T';
	buf[2] = ' ';
	buf[3] = ' ';
	terminal_num = index + 1;
	if (terminal_num >= 10) {
		buf[4] = '1';
		buf[5] = '0';
		buf[6] = '\0';
	} else {
		buf[4] = '0' + terminal_num;
		buf[5] = '\0';
	}

	for (i = 0; buf[i]; i++) {
		terminal_draw_cell(x + i, y, buf[i], 0x0A);
	}
}

static void
terminal_switch_to(int index)
{
	if (index < 0 || index >= TERM_COUNT) {
		return;
	}
	if (index == terminal_active) {
		return;
	}
	if (terminals[index].state == TERM_STATE_DISABLED) {
		return;
	}

	terminal_active = index;

	if (terminals[terminal_active].state == TERM_STATE_SUSPENDED) {
		if (g_con) {
			kms_console_flush(g_con);
		}
	} else {
		terminal_redraw(&terminals[terminal_active]);
	}

	terminal_indicator_end_time = timer_get_ticks() + timer_get_frequency();
	terminal_indicator_active = 1;
	terminal_draw_indicator(index);
}

void
terminal_set_active(int index)
{
	terminal_lazy_init();
	terminal_switch_to(index);
}

void *
terminal_get_input_channel(void)
{
	return (terminal_input_channel);
}

void
terminal_restore_active_display(void)
{
	if (!terminal_initialized) {
		return;
	}

	terminal_redraw(&terminals[terminal_active]);
	if (terminal_indicator_active) {
		terminal_draw_indicator(terminal_active);
	}
}

void
terminal_update(void)
{
	int	target;

	if (terminal_indicator_active &&
	    timer_get_ticks() >= terminal_indicator_end_time) {
		terminal_indicator_active = 0;
		terminal_redraw(&terminals[terminal_active]);
	}

	target = terminal_switch_pending;
	if (target < 0) {
		return;
	}
	terminal_switch_pending = -1;
	terminal_switch_to(target);
}

static int
terminal_numpad_digit(u8 scancode, int extended)
{
	if (extended) {
		return (-1);
	}
	switch (scancode) {
	case 0x52:
		return (0);
	case 0x4F:
		return (1);
	case 0x50:
		return (2);
	case 0x51:
		return (3);
	case 0x4B:
		return (4);
	case 0x4C:
		return (5);
	case 0x4D:
		return (6);
	case 0x47:
		return (7);
	case 0x48:
		return (8);
	case 0x49:
		return (9);
	default:
		return (-1);
	}
}

static void
terminal_scancode_callback(u8 scancode, int released, int extended)
{
	int	digit;
	int	index;

	if (scancode == 0x1D) {
		terminal_ctrl_down = released ? 0 : 1;
		return;
	}

	if (!released && terminal_ctrl_down) {
		digit = terminal_numpad_digit(scancode, extended);
		if (digit < 0) {
			return;
		}

		index = (digit == 0) ? 9 : (digit - 1);
		if (terminals[index].state == TERM_STATE_DISABLED) {
			return;
		}
		terminal_request_switch(index);
	}
}

static void
terminal_lazy_init(void)
{
	if (!terminal_initialized) {
		terminal_init();
	}
}

void
terminal_init(void)
{
	terminal_state_t	*term;
	u16			 blank;
	int			 width;
	int			 height;
	int			 i;
	int			 j;

	if (terminal_initialized) {
		return;
	}

	width = 0;
	height = 0;
	if (terminal_con()) {
		width = (int)g_con->cols;
		height = (int)g_con->rows;
	}
	if (width <= 0) {
		width = 80;
	}
	if (height <= 0) {
		height = 25;
	}

	for (i = 0; i < TERM_COUNT; i++) {
		term = &terminals[i];
		term->width = width;
		term->height = height;
		term->cursor_x = 0;
		term->cursor_y = 0;
		term->color = 0x07;
		term->ansi_state = 0;
		term->fg_rgb = 0xFFFFFFFF;
		term->ansi_params[0] = 0;
		term->ansi_params[1] = 0;
		term->ansi_params[2] = 0;
		term->ansi_params[3] = 0;
		term->ansi_params[4] = 0;
		term->ansi_params[5] = 0;
		term->ansi_params[6] = 0;
		term->ansi_params[7] = 0;
		term->ansi_param_count = 0;
		term->ansi_cur_param = 0;
		term->state = TERM_STATE_ACTIVE;
		term->cells = (u16 *)kmem_calloc(
		    (unsigned long)(width * height), sizeof(u16));
		if (term->cells) {
			blank = ((u16)term->color << 8) | ' ';
			for (j = 0; j < width * height; j++) {
				term->cells[j] = blank;
			}
		}
	}

	keyboard_set_scancode_callback(terminal_scancode_callback);
	terminal_initialized = 1;
	terminal_redraw(&terminals[terminal_active]);
}

int
terminal_is_initialized(void)
{
	return (terminal_initialized);
}

void
terminal_reinit(void)
{
	terminal_state_t	*term;
	u16			*old_cells;
	u16			 blank;
	int			 new_w;
	int			 new_h;
	int			 old_w;
	int			 old_h;
	int			 copy_w;
	int			 copy_h;
	int			 i;
	int			 j;
	int			 y;
	if (!terminal_initialized) {
		terminal_init();
		return;
	}

	kms_kernel_console_reset();
	g_con = NULL;
	g_con = kms_kernel_console();
	if (!g_con) {
		return;
	}

	new_w = (int)g_con->cols;
	new_h = (int)g_con->rows;
	if (new_w <= 0) {
		new_w = 80;
	}
	if (new_h <= 0) {
		new_h = 25;
	}

	for (i = 0; i < TERM_COUNT; i++) {
		term = &terminals[i];
		old_w = term->width;
		old_h = term->height;
		old_cells = term->cells;

		term->width = new_w;
		term->height = new_h;
		term->cells = (u16 *)kmem_calloc(
		    (unsigned long)(new_w * new_h), sizeof(u16));
		if (term->cells) {
			blank = ((u16)term->color << 8) | ' ';
			for (j = 0; j < new_w * new_h; j++) {
				term->cells[j] = blank;
			}
			if (old_cells) {
				copy_w = old_w < new_w ? old_w : new_w;
				copy_h = old_h < new_h ? old_h : new_h;
				for (y = 0; y < copy_h; y++) {
					memcpy(&term->cells[y * new_w],
					    &old_cells[y * old_w],
					    (unsigned long)copy_w * sizeof(u16));
				}
				kmem_free(old_cells);
			}
		} else if (old_cells) {
			kmem_free(old_cells);
		}

		if (term->cursor_x >= new_w) {
			term->cursor_x = 0;
		}
		if (term->cursor_y >= new_h) {
			term->cursor_y = 0;
		}
	}

	for (i = 0; i < TERM_COUNT; i++) {
		terminal_redraw(&terminals[i]);
	}

	terminal_indicator_end_time = timer_get_ticks() + timer_get_frequency();
	terminal_indicator_active = 1;
	if (g_con) {
		kms_console_flush(g_con);
	}
	terminal_draw_indicator(terminal_active);
}

void
terminal_set_color(u8 color)
{
	if (!terminal_initialized) {
		return;
	}
	terminals[terminal_active].color = color;
}

void
terminal_flush_kernel(void)
{
	if (g_con) {
		kms_console_flush(g_con);
	}
}

void
terminal_putc_from_kernel(char c)
{
	if (!terminal_initialized) {
		return;
	}
	terminal_update();
	if (terminals[terminal_active].state != TERM_STATE_ACTIVE) {
		return;
	}
	terminal_putc_internal(&terminals[terminal_active], c, 1);
	if (c == '\n') {
		terminal_flush_kernel();
	}
}

void
terminal_log_mirror(char c)
{
	if (terminal_suppress_com1_mirror) {
		return;
	}
	if (!terminal_initialized) {
		return;
	}
	terminal_putc_internal(&terminals[0], c, terminal_active == 0);
}

void
terminal_clear_active(void)
{
	terminal_state_t	*term;
	u16		 blank;
	int		 i;

	if (!terminal_initialized) {
		return;
	}
	term = &terminals[terminal_active];
	if (!term->cells) {
		return;
	}
	blank = ((u16)term->color << 8) | ' ';
	for (i = 0; i < term->width * term->height; i++) {
		term->cells[i] = blank;
	}
	term->cursor_x = 0;
	term->cursor_y = 0;
	term->ansi_state = 0;
	term->fg_rgb = 0xFFFFFFFF;
	term->ansi_params[0] = 0;
	term->ansi_params[1] = 0;
	term->ansi_params[2] = 0;
	term->ansi_params[3] = 0;
	term->ansi_params[4] = 0;
	term->ansi_params[5] = 0;
	term->ansi_params[6] = 0;
	term->ansi_params[7] = 0;
	term->ansi_param_count = 0;
	term->ansi_cur_param = 0;
	terminal_redraw(term);
}

static void
terminal_pump_keyboard(void)
{
	terminal_input_queue_t	*q;
	char			 c;
	int			 next;
	int			 got_data;

	got_data = 0;
	if (terminals[terminal_active].state != TERM_STATE_ACTIVE) {
		return;
	}

	while (1) {
		c = keyboard_getchar();
		if (c == 0) {
			break;
		}
		q = &terminal_inputs[terminal_active];
		next = (q->head + 1) % 256;
		if (next != q->tail) {
			q->buf[q->head] = c;
			q->head = next;
		}
		got_data = 1;
	}

	if (got_data) {
		knote_notify_all(EVFILT_READ, 0, 0, 1);
	}
}

static char
terminal_getchar_blocking(int terminal_idx)
{
	terminal_input_queue_t	*q;
	char			 c;

	while (1) {
		terminal_update();
		terminal_pump_keyboard();
		q = &terminal_inputs[terminal_idx];
		if (q->head != q->tail) {
			c = q->buf[q->tail];
			q->tail = (q->tail + 1) % 256;
			terminal_update();
			return (c);
		}
		proc_sleep(terminal_input_channel);
	}
}

static void
terminal_emit_to(int terminal_idx, char c)
{
	if (terminals[terminal_idx].state != TERM_STATE_ACTIVE) {
		return;
	}
	terminal_putc_internal(&terminals[terminal_idx], c,
	    terminal_idx == terminal_active);
	if (terminal_idx == terminal_active) {
		terminal_suppress_com1_mirror = 1;
		uart_write_byte((u8)c);
		terminal_suppress_com1_mirror = 0;
	}
}

static void
terminal_fill_line_buffer(int terminal_idx)
{
	terminal_line_buf_t	*line;
	char			 c;

	line = &terminal_line_bufs[terminal_idx];
	if (!line) {
		return;
	}

	line->len = 0;
	line->read_pos = 0;

	while (1) {
		c = terminal_getchar_blocking(terminal_idx);
		if (c == 0) {
			continue;
		}

		if (c == '\b' || c == 0x7F) {
			if (line->len > 0) {
				line->len--;
				terminal_emit_to(terminal_idx, '\b');
				terminal_emit_to(terminal_idx, ' ');
				terminal_emit_to(terminal_idx, '\b');
				if (terminal_idx == terminal_active && g_con) {
					kms_console_flush(g_con);
				}
			}
			continue;
		}

		if (c == '\n' || c == '\r') {
			if (line->len < (TERM_LINE_BUF_SIZE - 1)) {
				line->data[line->len++] = '\n';
			}
			terminal_emit_to(terminal_idx, '\n');
			if (terminal_idx == terminal_active && g_con) {
				kms_console_flush(g_con);
			}
			break;
		}

		if (line->len < (TERM_LINE_BUF_SIZE - 1)) {
			line->data[line->len++] = c;
			terminal_emit_to(terminal_idx, c);
			if (terminal_idx == terminal_active && g_con) {
				kms_console_flush(g_con);
			}
		}
	}
}

int
terminal_read(void *buf, u32 count)
{
	terminal_line_buf_t	*line;
	u32			 available;
	u32			 to_copy;
	int			 terminal_idx;

	terminal_lazy_init();

	if (count == 0) {
		return (0);
	}

	if (terminals[terminal_active].state != TERM_STATE_ACTIVE) {
		return (-API_ERR_NOT_TERM);
	}

	__asm__ volatile("sti");

	terminal_idx = terminal_active;
	line = &terminal_line_bufs[terminal_idx];
	if (line->read_pos >= line->len) {
		line->len = 0;
		line->read_pos = 0;
		terminal_fill_line_buffer(terminal_idx);
	}

	available = line->len - line->read_pos;
	to_copy = count;
	if (to_copy > available) {
		to_copy = available;
	}

	if (to_copy > 0) {
		memcpy(buf, line->data + line->read_pos, to_copy);
		line->read_pos += to_copy;
	}

	if (line->read_pos >= line->len) {
		line->len = 0;
		line->read_pos = 0;
	}

	return ((int)to_copy);
}

int
terminal_write(const void *buf, u32 count)
{
	const char	*data;
	u32		 i;

	terminal_lazy_init();
	terminal_update();

	if (count == 0) {
		return (0);
	}

	if (terminals[terminal_active].state != TERM_STATE_ACTIVE) {
		return (-API_ERR_NOT_TERM);
	}

	data = (const char *)buf;
	for (i = 0; i < count; i++) {
		terminal_emit(data[i]);
	}

	if (g_con) {
		kms_console_flush(g_con);
	}

	return ((int)count);
}

static void
terminal_reset_one(int index)
{
	terminal_state_t	*term;
	terminal_line_buf_t	*line;
	terminal_input_queue_t	*input;
	u16			 blank;
	int			 i;

	if (index < 0 || index >= TERM_COUNT) {
		return;
	}

	term = &terminals[index];
	line = &terminal_line_bufs[index];
	input = &terminal_inputs[index];

	if (term->cells) {
		blank = ((u16)0x07 << 8) | ' ';
		for (i = 0; i < term->width * term->height; i++) {
			term->cells[i] = blank;
		}
	}

	term->cursor_x = 0;
	term->cursor_y = 0;
	term->color = 0x07;
	term->fg_rgb = 0xFFFFFFFF;
	term->ansi_state = 0;
	term->ansi_param_count = 0;
	term->ansi_cur_param = 0;
	for (i = 0; i < 8; i++) {
		term->ansi_params[i] = 0;
	}
	term->state = TERM_STATE_ACTIVE;

	line->len = 0;
	line->read_pos = 0;

	input->head = 0;
	input->tail = 0;
	for (i = 0; i < 1024; i++) {
		input->buf[i] = 0;
	}

	if (index == terminal_active && g_con) {
		rapi_console_clear(g_con, 0x000000);
		kms_console_flush(g_con);
	}
}

int
terminal_power_get(int index)
{
	if (index < 0 || index >= TERM_COUNT) {
		return (-API_ERR_INVAL);
	}
	if (!terminal_initialized && index != 0) {
		return (-API_ERR_INVAL);
	}
	return (terminals[index].state);
}

int
terminal_power_set(int index, int state)
{
	terminal_state_t	*term;

	if (index < 0 || index >= TERM_COUNT) {
		return (-API_ERR_INVAL);
	}
	if (!terminal_state_is_valid(state)) {
		return (-API_ERR_INVAL);
	}

	term = &terminals[index];

	if (term->state == TERM_STATE_DISABLED &&
	    state != TERM_STATE_DISABLED) {
		terminal_reset_one(index);
	}

	term->state = state;

	if (index == terminal_active) {
		if (state == TERM_STATE_ACTIVE) {
			terminal_redraw(term);
			terminal_indicator_end_time =
			    timer_get_ticks() + timer_get_frequency();
			terminal_indicator_active = 1;
			terminal_draw_indicator(index);
		} else if (state == TERM_STATE_SUSPENDED && g_con) {
			rapi_console_clear(g_con, 0x000000);
			kms_console_flush(g_con);
		}
	}

	return (0);
}

int
terminal_power_reset(int index)
{
	if (index < 0 || index >= TERM_COUNT) {
		return (-API_ERR_INVAL);
	}

	terminal_reset_one(index);

	if (index == terminal_active &&
	    terminals[index].state == TERM_STATE_ACTIVE) {
		terminal_redraw(&terminals[index]);
	}

	return (0);
}

int
terminal_power_suspend_all(void)
{
	int	i;
	int	ret;

	for (i = 0; i < TERM_COUNT; i++) {
		if (terminals[i].state == TERM_STATE_ACTIVE) {
			ret = terminal_power_set(i, TERM_STATE_SUSPENDED);
			if (ret != 0) {
				return (ret);
			}
		}
	}

	return (0);
}
