/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
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

/* !DEFINES!

$define %type int as 32 bit signed
$define %type api_term_power as struct with term power syscall args
$define %func api_term_power as function with args struct api_term_power *

*/

/* !SPACE!

$space %export api_term_power

*/

#include <kernel/api/api.h>
#include <kernel/drivers/tty.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

int
api_term_power(struct api_term_power *uargs)
{
	struct api_term_power	args;
	process_t		*proc;

	proc = process_current();
	if (!proc) {
		return (-API_ERR_BAD_VALUE);
	}

	if (!uargs || !is_user_address(uargs, sizeof(*uargs))) {
		return (-API_ERR_BAD_ADDR);
	}

	memcpy(&args, uargs, sizeof(args));
	switch (args.op) {
	case API_TERM_POWER_GET:
		return (tty_power_get(args.tty));
	case API_TERM_POWER_CHANGE:
		if (args.state == TTY_STATE_DISABLED && !proc->kusr_auth) {
			return (-API_ERR_PERM);
		}
		return (tty_power_set(args.tty, args.state));
	case API_TERM_POWER_RESET:
		if (!proc->kusr_auth) {
			return (-API_ERR_PERM);
		}
		return (tty_power_reset(args.tty));
	default:
		return (-API_ERR_INVAL);
	}
}
