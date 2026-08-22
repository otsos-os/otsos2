/* !DEFINES!

$define %type api_term_mode as struct with native terminal mode fields
$define %type termios as kernel terminal mode fields
$define %func api_term_mode_fill as procedure with args api_term_mode *, termios *
$define %func api_term_mode_apply as procedure with args termios *, api_term_mode *
$define %func api_term_mode as function with args struct api_term_mode *

*/

/* !SPACE!

$space %internal api_term_mode_fill, api_term_mode_apply
$space %export api_term_mode

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
#include <kernel/console/pty.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

static void
api_term_mode_fill(struct api_term_mode *args, const struct termios *term)
{
	args->iflag = term->c_iflag;
	args->oflag = term->c_oflag;
	args->cflag = term->c_cflag;
	args->lflag = term->c_lflag;
	memcpy(args->cc, term->c_cc, sizeof(args->cc));
	args->ispeed = term->c_ispeed;
	args->ospeed = term->c_ospeed;
}

static void
api_term_mode_apply(struct termios *term, const struct api_term_mode *args)
{
	term->c_iflag = args->iflag;
	term->c_oflag = args->oflag;
	term->c_cflag = args->cflag;
	term->c_lflag = args->lflag;
	memcpy(term->c_cc, args->cc, sizeof(term->c_cc));
	term->c_ispeed = args->ispeed;
	term->c_ospeed = args->ospeed;
}

int
api_term_mode(struct api_term_mode *uargs)
{
	struct api_term_mode	args;
	struct termios		term;
	process_t		*proc;
	int			ret, tty, pty_id;

	if (!uargs || !is_user_address(uargs, sizeof(*uargs))) {
		return (-API_ERR_BAD_ADDR);
	}

	memcpy(&args, uargs, sizeof(args));
	proc = process_current();
	if (args.tty == API_TERM_ACTIVE && proc && proc->controlling_tty < -1) {
		pty_id = -proc->controlling_tty - 2;
		if (args.op == API_TERM_MODE_GET) {
			pty_get_termios(pty_id, &term);
			api_term_mode_fill(&args, &term);
			args.tty = proc->controlling_tty;
			memcpy(uargs, &args, sizeof(args));
			return (0);
		} else if (args.op == API_TERM_MODE_SET) {
			pty_get_termios(pty_id, &term);
			api_term_mode_apply(&term, &args);
			pty_set_termios(pty_id, &term);
			return (0);
		}
		return (-API_ERR_INVAL);
	}

	tty = args.tty;
	if (tty == API_TERM_ACTIVE) {
		tty = terminal_get_active();
	}
	ret = terminal_power_get(tty);
	if (ret < 0) {
		return (ret);
	}

	switch (args.op) {
	case API_TERM_MODE_GET:
		terminal_get_termios(tty, &term);
		api_term_mode_fill(&args, &term);
		args.tty = tty;
		memcpy(uargs, &args, sizeof(args));
		return (0);
	case API_TERM_MODE_SET:
		terminal_get_termios(tty, &term);
		api_term_mode_apply(&term, &args);
		terminal_set_termios(tty, &term);
		return (0);
	default:
		return (-API_ERR_INVAL);
	}
}
