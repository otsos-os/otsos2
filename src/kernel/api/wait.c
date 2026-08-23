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

#include <mm/vm/pmap.h>
#include <mm/vm/vm_map.h>
#include <kernel/event/event.h>
#include <kernel/console/terminal.h>
#include <kernel/console/pty.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mm/kmem.h>

extern void pmap_destroy_page_tables_only(u64 cr3);

int
api_proc_wait(int *status)
{
	process_t	*current;
	process_t	*child;
	thread_t	*child_td;
	u64		old_cr3;
	int		attempt, pid, i;

	current = process_current();
	if (!current) {
		return (-API_ERR_NO_CHILD);
	}

	for (attempt = 0; attempt < 2; attempt++) {
		for (i = 0; i < MAX_PROCESSES; i++) {
			child = &process_table[i];
			if (child->pid == 0) {
				continue;
			}
			child_td = child->main_thread;
			if (!child_td ||
			    child_td->state != PROC_STATE_ZOMBIE) {
				continue;
			}
			if (child->ppid != current->pid) {
				continue;
			}

			if (status && is_user_address(status, sizeof(int))) {
				*status = child->exit_code;
			}

			if (child_td->running_cpu >= 0) {
				pid = (int)child->pid;
				if (current->controlling_tty >= 0) {
					terminal_set_pgrp(current->controlling_tty,
					    current->pgid);
				} else if (current->controlling_tty < -1) {
					int pty_num = -current->controlling_tty - 2;
					pty_set_session_pgrp(pty_num, current->sid,
					    current->pgid);
				}
				child->ppid = 0;
				return (pid);
			}


			if (child->owns_address_space && child->cr3) {
				old_cr3 = pmap_get_cr3();
				pmap_load(child->cr3);
				vm_map_free_all(child);
				pmap_load(old_cr3);
				pmap_destroy(child->cr3);
				child->cr3 = 0;
				child->owns_address_space = 0;
			}

			thread_destroy(child_td);
			pid = (int)child->pid;
			memset(child, 0, sizeof(process_t));
			if (current->controlling_tty >= 0) {
				terminal_set_pgrp(current->controlling_tty,
				    current->pgid);
			} else if (current->controlling_tty < -1) {
				int pty_num = -current->controlling_tty - 2;
				pty_set_session_pgrp(pty_num, current->sid,
				    current->pgid);
			}
			return (pid);
		}

		if (attempt == 0) {
			proc_sleep((void *)current);
		}
	}

	return (-API_ERR_NO_CHILD);
}
