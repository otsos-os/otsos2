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

$define %type uint32_t as 32 bit unsigned

*/

/* !SPACE!

$space %internal none

*/

#ifndef PROGS_REGEDIT_GUI_KEYS_H
#define PROGS_REGEDIT_GUI_KEYS_H
#define RG_KEY_A		0x0004
#define RG_KEY_C		0x0006
#define RG_KEY_D		0x0007
#define RG_KEY_G		0x000a
#define RG_KEY_N		0x0011
#define RG_KEY_Q		0x0014
#define RG_KEY_R		0x0015
#define RG_KEY_S		0x0016
#define RG_KEY_U		0x0018
#define RG_KEY_V		0x0019
#define RG_KEY_Y		0x001c
#define RG_KEY_Z		0x001d
#define RG_KEY_1		0x001e
#define RG_KEY_2		0x001f
#define RG_KEY_3		0x0020
#define RG_KEY_4		0x0021
#define RG_KEY_5		0x0022
#define RG_KEY_6		0x0023
#define RG_KEY_7		0x0024
#define RG_KEY_8		0x0025
#define RG_KEY_9		0x0026
#define RG_KEY_0		0x0027
#define RG_KEY_ENTER		0x0028
#define RG_KEY_ESC		0x0029
#define RG_KEY_BACKSPACE	0x002a
#define RG_KEY_TAB		0x002b
#define RG_KEY_SPACE		0x002c
#define RG_KEY_MINUS		0x002d
#define RG_KEY_EQUAL		0x002e
#define RG_KEY_LEFTBRACE	0x002f
#define RG_KEY_RIGHTBRACE	0x0030
#define RG_KEY_BACKSLASH	0x0031
#define RG_KEY_SEMICOLON	0x0033
#define RG_KEY_APOSTROPHE	0x0034
#define RG_KEY_GRAVE		0x0035
#define RG_KEY_COMMA		0x0036
#define RG_KEY_DOT		0x0037
#define RG_KEY_SLASH		0x0038
#define RG_KEY_F1		0x003a
#define RG_KEY_F2		0x003b
#define RG_KEY_F3		0x003c
#define RG_KEY_F4		0x003d
#define RG_KEY_F5		0x003e
#define RG_KEY_F6		0x003f
#define RG_KEY_F11		0x0044
#define RG_KEY_INSERT		0x0049
#define RG_KEY_HOME		0x004a
#define RG_KEY_PAGEUP		0x004b
#define RG_KEY_DELETE		0x004c
#define RG_KEY_END		0x004d
#define RG_KEY_PAGEDOWN		0x004e
#define RG_KEY_RIGHT		0x004f
#define RG_KEY_LEFT		0x0050
#define RG_KEY_DOWN		0x0051
#define RG_KEY_UP		0x0052
#define RG_KEY_KP_SLASH		0x0054
#define RG_KEY_KP_ASTERISK	0x0055
#define RG_KEY_KP_MINUS		0x0056
#define RG_KEY_KP_PLUS		0x0057
#define RG_KEY_KP_ENTER		0x0058
#define RG_KEY_KP_1		0x0059
#define RG_KEY_KP_2		0x005a
#define RG_KEY_KP_3		0x005b
#define RG_KEY_KP_4		0x005c
#define RG_KEY_KP_5		0x005d
#define RG_KEY_KP_6		0x005e
#define RG_KEY_KP_7		0x005f
#define RG_KEY_KP_8		0x0060
#define RG_KEY_KP_9		0x0061
#define RG_KEY_KP_0		0x0062
#define RG_KEY_KP_DOT		0x0063
#define RG_MOD_LSHIFT		0x00000001
#define RG_MOD_RSHIFT		0x00000002
#define RG_MOD_LCTRL		0x00000004
#define RG_MOD_RCTRL		0x00000008
#define RG_MOD_CAPS		0x00000100
#define RG_MOD_SHIFT		(RG_MOD_LSHIFT | RG_MOD_RSHIFT)
#define RG_MOD_CTRL		(RG_MOD_LCTRL | RG_MOD_RCTRL)

#endif
