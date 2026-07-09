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
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
*/
/* !SPACE!
$space %export KEY_*, KEY_EVENT_*, MOD_*
*/
#ifndef KERNEL_DRIVERS_KEYBOARD_KEYCODES_H
#define KERNEL_DRIVERS_KEYBOARD_KEYCODES_H
#define	KEY_NONE		0x0000
#define	KEY_A		0x0004
#define	KEY_B		0x0005
#define	KEY_C		0x0006
#define	KEY_D		0x0007
#define	KEY_E		0x0008
#define	KEY_F		0x0009
#define	KEY_G		0x000a
#define	KEY_H		0x000b
#define	KEY_I		0x000c
#define	KEY_J		0x000d
#define	KEY_K		0x000e
#define	KEY_L		0x000f
#define	KEY_M		0x0010
#define	KEY_N		0x0011
#define	KEY_O		0x0012
#define	KEY_P		0x0013
#define	KEY_Q		0x0014
#define	KEY_R		0x0015
#define	KEY_S		0x0016
#define	KEY_T		0x0017
#define	KEY_U		0x0018
#define	KEY_V		0x0019
#define	KEY_W		0x001a
#define	KEY_X		0x001b
#define	KEY_Y		0x001c
#define	KEY_Z		0x001d
#define	KEY_1		0x001e
#define	KEY_2		0x001f
#define	KEY_3		0x0020
#define	KEY_4		0x0021
#define	KEY_5		0x0022
#define	KEY_6		0x0023
#define	KEY_7		0x0024
#define	KEY_8		0x0025
#define	KEY_9		0x0026
#define	KEY_0		0x0027
#define	KEY_ENTER		0x0028
#define	KEY_ESC		0x0029
#define	KEY_BACKSPACE	0x002a
#define	KEY_TAB		0x002b
#define	KEY_SPACE		0x002c
#define	KEY_MINUS		0x002d
#define	KEY_EQUAL		0x002e
#define	KEY_LEFTBRACE	0x002f
#define	KEY_RIGHTBRACE	0x0030
#define	KEY_BACKSLASH	0x0031
#define	KEY_SEMICOLON	0x0033
#define	KEY_APOSTROPHE	0x0034
#define	KEY_GRAVE		0x0035
#define	KEY_COMMA		0x0036
#define	KEY_DOT		0x0037
#define	KEY_SLASH		0x0038
#define	KEY_CAPSLOCK	0x0039
#define	KEY_F1		0x003a
#define	KEY_F2		0x003b
#define	KEY_F3		0x003c
#define	KEY_F4		0x003d
#define	KEY_F5		0x003e
#define	KEY_F6		0x003f
#define	KEY_F7		0x0040
#define	KEY_F8		0x0041
#define	KEY_F9		0x0042
#define	KEY_F10		0x0043
#define	KEY_F11		0x0044
#define	KEY_F12		0x0045
#define	KEY_PRINTSCREEN	0x0046
#define	KEY_SCROLLLOCK	0x0047
#define	KEY_PAUSE		0x0048
#define	KEY_INSERT		0x0049
#define	KEY_HOME		0x004a
#define	KEY_PAGEUP		0x004b
#define	KEY_DELETE		0x004c
#define	KEY_END		0x004d
#define	KEY_PAGEDOWN	0x004e
#define	KEY_RIGHT		0x004f
#define	KEY_LEFT		0x0050
#define	KEY_DOWN		0x0051
#define	KEY_UP		0x0052
#define	KEY_NUMLOCK		0x0053
#define	KEY_KP_SLASH	0x0054
#define	KEY_KP_ASTERISK	0x0055
#define	KEY_KP_MINUS	0x0056
#define	KEY_KP_PLUS		0x0057
#define	KEY_KP_ENTER	0x0058
#define	KEY_KP_1		0x0059
#define	KEY_KP_2		0x005a
#define	KEY_KP_3		0x005b
#define	KEY_KP_4		0x005c
#define	KEY_KP_5		0x005d
#define	KEY_KP_6		0x005e
#define	KEY_KP_7		0x005f
#define	KEY_KP_8		0x0060
#define	KEY_KP_9		0x0061
#define	KEY_KP_0		0x0062
#define	KEY_KP_DOT		0x0063
#define	KEY_LCTRL		0x00e0
#define	KEY_LSHIFT		0x00e1
#define	KEY_LALT		0x00e2
#define	KEY_LSUPER		0x00e3
#define	KEY_RCTRL		0x00e4
#define	KEY_RSHIFT		0x00e5
#define	KEY_RALT		0x00e6
#define	KEY_RSUPER		0x00e7
#define	KEY_EVENT_PRESS	0x00000001
#define	KEY_EVENT_RELEASE	0x00000002
#define	KEY_EVENT_REPEAT	0x00000004
#define	KEY_EVENT_EXTENDED	0x00000008
#define	MOD_LSHIFT		0x00000001
#define	MOD_RSHIFT		0x00000002
#define	MOD_LCTRL		0x00000004
#define	MOD_RCTRL		0x00000008
#define	MOD_LALT		0x00000010
#define	MOD_RALT		0x00000020
#define	MOD_LSUPER		0x00000040
#define	MOD_RSUPER		0x00000080
#define	MOD_CAPS		0x00000100
#define	MOD_NUM		0x00000200
#define	MOD_SCROLL		0x00000400
#define	MOD_SHIFT		(MOD_LSHIFT | MOD_RSHIFT)
#define	MOD_CTRL		(MOD_LCTRL | MOD_RCTRL)
#define	MOD_ALT		(MOD_LALT | MOD_RALT)
#define	MOD_SUPER		(MOD_LSUPER | MOD_RSUPER)
#endif
