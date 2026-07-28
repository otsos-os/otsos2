/* !DEFINES!

$define %type api_term_mouse as struct with console mouse state args
$define %func api_term_mouse as function with args struct api_term_mouse *

*/

/* !SPACE!

$space %export api_term_mouse

*/

/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <kernel/api/api.h>
#include <kernel/console/terminal.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

int
api_term_mouse(struct api_term_mouse *uargs)
{
	struct api_term_mouse	args;
	int			tty;
	int			enabled;

	if (!uargs || !is_user_address(uargs, sizeof(*uargs))) {
		return (-API_ERR_BAD_ADDR);
	}

	memcpy(&args, uargs, sizeof(args));
	if (args.op != API_TERM_MOUSE_UPDATE ||
	    (args.flags & ~API_TERM_MOUSE_VISIBLE) != 0) {
		return (-API_ERR_INVAL);
	}

	tty = args.tty;
	if (tty == API_TERM_ACTIVE) {
		tty = terminal_get_active();
	}

	enabled = (args.flags & API_TERM_MOUSE_VISIBLE) != 0;
	return (terminal_mouse_update(tty, enabled, args.x, args.y,
	    args.buttons));
}
