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

$define %type api_term_info as struct with active terminal info
$define %func api_term_info as function with args struct api_term_info *

*/

/* !SPACE!

$space %export api_term_info

*/

#include <kernel/api/api.h>
#include <kernel/console/terminal.h>
#include <kernel/console/pty.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

int
api_term_info(struct api_term_info *info)
{
	struct winsize	ws;
	process_t	*proc;
	int		pty_id, tty;

	if (!info || !is_user_address(info, sizeof(*info))) {
		return (-API_ERR_BAD_ADDR);
	}

	proc = process_current();
	if (proc && proc->controlling_tty < -1) {
		pty_id = -proc->controlling_tty - 2;
		pty_get_winsize(pty_id, &ws);
		info->tty = proc->controlling_tty;
		info->state = 0;
		info->rows = ws.ws_row;
		info->cols = ws.ws_col;
		info->xpixel = ws.ws_xpixel;
		info->ypixel = ws.ws_ypixel;
		return (0);
	}

	tty = terminal_get_active();
	terminal_get_winsize(tty, &ws);

	info->tty = tty;
	info->state = terminal_power_get(tty);
	info->rows = ws.ws_row;
	info->cols = ws.ws_col;
	info->xpixel = ws.ws_xpixel;
	info->ypixel = ws.ws_ypixel;
	return (0);
}
