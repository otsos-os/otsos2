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

#ifndef KERNEL_CONSOLE_PTY_H
#define KERNEL_CONSOLE_PTY_H

#include <kernel/console/terminal.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <mlibc/mlibc.h>

int	pty_init(void);
int	pty_open_master(vnode_t **out);
int	pty_name(int id, char *buf, int len);
int	pty_master_read(vnode_t *vn, void *buf, u32 count, int nonblock);
int	pty_master_write(vnode_t *vn, const void *buf, u32 count, int nonblock);
int	pty_master_ioctl(vnode_t *vn, u64 cmd, void *arg);
int	pty_slave_read(vnode_t *vn, void *buf, u32 count, int nonblock);
int	pty_slave_write(vnode_t *vn, const void *buf, u32 count, int nonblock);
int	pty_slave_ioctl(vnode_t *vn, u64 cmd, void *arg);
int	pty_read_available(vnode_t *vn);
int	pty_write_available(vnode_t *vn);
vnode_t	*pty_create_slave_vnode(int id);
void	pty_slave_release(vnode_t *vn);
void	pty_set_session_pgrp(int id, u32 sid, u32 pgid);
void	pty_hangup(int id);
void	pty_get_winsize(int id, struct winsize *ws);
void	pty_set_winsize(int id, const struct winsize *ws);
void	pty_get_termios(int id, struct termios *t);
void	pty_set_termios(int id, const struct termios *t);
int	pty_slave_read_idx(int id, void *buf, u32 count, int nonblock);
int	pty_slave_write_idx(int id, const void *buf, u32 count, int nonblock);

#endif
