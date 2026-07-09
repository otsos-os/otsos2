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

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned

$define %func keymap_ps2_set1 as function with args u8, int
$define %func keymap_ascii as function with args u16, u32

*/

/* !SPACE!

$space %export keymap_ps2_set1, keymap_ascii

*/

#include <kernel/drivers/keyboard/keycodes.h>
#include <kernel/drivers/keyboard/keymap.h>

static const u16 ps2_set1[128] = {
	[0x01] = KEY_ESC,
	[0x02] = KEY_1,
	[0x03] = KEY_2,
	[0x04] = KEY_3,
	[0x05] = KEY_4,
	[0x06] = KEY_5,
	[0x07] = KEY_6,
	[0x08] = KEY_7,
	[0x09] = KEY_8,
	[0x0a] = KEY_9,
	[0x0b] = KEY_0,
	[0x0c] = KEY_MINUS,
	[0x0d] = KEY_EQUAL,
	[0x0e] = KEY_BACKSPACE,
	[0x0f] = KEY_TAB,
	[0x10] = KEY_Q,
	[0x11] = KEY_W,
	[0x12] = KEY_E,
	[0x13] = KEY_R,
	[0x14] = KEY_T,
	[0x15] = KEY_Y,
	[0x16] = KEY_U,
	[0x17] = KEY_I,
	[0x18] = KEY_O,
	[0x19] = KEY_P,
	[0x1a] = KEY_LEFTBRACE,
	[0x1b] = KEY_RIGHTBRACE,
	[0x1c] = KEY_ENTER,
	[0x1d] = KEY_LCTRL,
	[0x1e] = KEY_A,
	[0x1f] = KEY_S,
	[0x20] = KEY_D,
	[0x21] = KEY_F,
	[0x22] = KEY_G,
	[0x23] = KEY_H,
	[0x24] = KEY_J,
	[0x25] = KEY_K,
	[0x26] = KEY_L,
	[0x27] = KEY_SEMICOLON,
	[0x28] = KEY_APOSTROPHE,
	[0x29] = KEY_GRAVE,
	[0x2a] = KEY_LSHIFT,
	[0x2b] = KEY_BACKSLASH,
	[0x2c] = KEY_Z,
	[0x2d] = KEY_X,
	[0x2e] = KEY_C,
	[0x2f] = KEY_V,
	[0x30] = KEY_B,
	[0x31] = KEY_N,
	[0x32] = KEY_M,
	[0x33] = KEY_COMMA,
	[0x34] = KEY_DOT,
	[0x35] = KEY_SLASH,
	[0x36] = KEY_RSHIFT,
	[0x37] = KEY_KP_ASTERISK,
	[0x38] = KEY_LALT,
	[0x39] = KEY_SPACE,
	[0x3a] = KEY_CAPSLOCK,
	[0x3b] = KEY_F1,
	[0x3c] = KEY_F2,
	[0x3d] = KEY_F3,
	[0x3e] = KEY_F4,
	[0x3f] = KEY_F5,
	[0x40] = KEY_F6,
	[0x41] = KEY_F7,
	[0x42] = KEY_F8,
	[0x43] = KEY_F9,
	[0x44] = KEY_F10,
	[0x45] = KEY_NUMLOCK,
	[0x46] = KEY_SCROLLLOCK,
	[0x47] = KEY_KP_7,
	[0x48] = KEY_KP_8,
	[0x49] = KEY_KP_9,
	[0x4a] = KEY_KP_MINUS,
	[0x4b] = KEY_KP_4,
	[0x4c] = KEY_KP_5,
	[0x4d] = KEY_KP_6,
	[0x4e] = KEY_KP_PLUS,
	[0x4f] = KEY_KP_1,
	[0x50] = KEY_KP_2,
	[0x51] = KEY_KP_3,
	[0x52] = KEY_KP_0,
	[0x53] = KEY_KP_DOT,
	[0x57] = KEY_F11,
	[0x58] = KEY_F12,
};

static const u16 ps2_set1_e0[128] = {
	[0x1c] = KEY_KP_ENTER,
	[0x1d] = KEY_RCTRL,
	[0x35] = KEY_KP_SLASH,
	[0x38] = KEY_RALT,
	[0x47] = KEY_HOME,
	[0x48] = KEY_UP,
	[0x49] = KEY_PAGEUP,
	[0x4b] = KEY_LEFT,
	[0x4d] = KEY_RIGHT,
	[0x4f] = KEY_END,
	[0x50] = KEY_DOWN,
	[0x51] = KEY_PAGEDOWN,
	[0x52] = KEY_INSERT,
	[0x53] = KEY_DELETE,
};

u16
keymap_ps2_set1(u8 scancode, int extended)
{
	u8	code;

	code = scancode & 0x7f;
	if (code >= 128) {
		return (KEY_NONE);
	}
	if (extended) {
		return (ps2_set1_e0[code]);
	}
	return (ps2_set1[code]);
}

u32
keymap_ascii(u16 key, u32 mods)
{
	int	shift;
	int	caps;
	int	ctrl;
	u32	ch;

	shift = (mods & MOD_SHIFT) != 0;
	caps = (mods & MOD_CAPS) != 0;
	ctrl = (mods & MOD_CTRL) != 0;

	if (key >= KEY_A && key <= KEY_Z) {
		ch = (u32)('a' + (key - KEY_A));
		if (ctrl) {
			return (ch - 'a' + 1);
		}
		if (shift ^ caps) {
			ch = ch - 32;
		}
		return (ch);
	}

	switch (key) {
	case KEY_1: return (shift ? '!' : '1');
	case KEY_2: return (shift ? '@' : '2');
	case KEY_3: return (shift ? '#' : '3');
	case KEY_4: return (shift ? '$' : '4');
	case KEY_5: return (shift ? '%' : '5');
	case KEY_6: return (shift ? '^' : '6');
	case KEY_7: return (shift ? '&' : '7');
	case KEY_8: return (shift ? '*' : '8');
	case KEY_9: return (shift ? '(' : '9');
	case KEY_0: return (shift ? ')' : '0');
	case KEY_ENTER:
	case KEY_KP_ENTER:
		return ('\n');
	case KEY_ESC: return (27);
	case KEY_BACKSPACE: return ('\b');
	case KEY_TAB: return ('\t');
	case KEY_SPACE: return (' ');
	case KEY_MINUS: return (shift ? '_' : '-');
	case KEY_EQUAL: return (shift ? '+' : '=');
	case KEY_LEFTBRACE: return (shift ? '{' : '[');
	case KEY_RIGHTBRACE: return (shift ? '}' : ']');
	case KEY_BACKSLASH: return (shift ? '|' : '\\');
	case KEY_SEMICOLON: return (shift ? ':' : ';');
	case KEY_APOSTROPHE: return (shift ? '"' : '\'');
	case KEY_GRAVE: return (shift ? '~' : '`');
	case KEY_COMMA: return (shift ? '<' : ',');
	case KEY_DOT: return (shift ? '>' : '.');
	case KEY_SLASH: return (shift ? '?' : '/');
	case KEY_KP_0: return ('0');
	case KEY_KP_1: return ('1');
	case KEY_KP_2: return ('2');
	case KEY_KP_3: return ('3');
	case KEY_KP_4: return ('4');
	case KEY_KP_5: return ('5');
	case KEY_KP_6: return ('6');
	case KEY_KP_7: return ('7');
	case KEY_KP_8: return ('8');
	case KEY_KP_9: return ('9');
	case KEY_KP_DOT: return ('.');
	case KEY_KP_SLASH: return ('/');
	case KEY_KP_ASTERISK: return ('*');
	case KEY_KP_MINUS: return ('-');
	case KEY_KP_PLUS: return ('+');
	default:
		return (0);
	}
}
