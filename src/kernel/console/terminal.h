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
 *    in the documentation and/or other materials provided with the distribution.
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

#ifndef KERNEL_CONSOLE_TERMINAL_H
#define KERNEL_CONSOLE_TERMINAL_H

#include <kernel/drivers/fs/vfs/vfs.h>
#include <mlibc/mlibc.h>

#define	TERM_STATE_ACTIVE	0
#define	TERM_STATE_SUSPENDED	1
#define	TERM_STATE_DISABLED	2
#define	NCCS		32
#define	VINTR		0
#define	VQUIT		1
#define	VERASE		2
#define	VKILL		3
#define	VEOF		4
#define	VTIME		5
#define	VMIN		6
#define	VSWTC		7
#define	VSTART		8
#define	VSTOP		9
#define	VSUSP		10
#define	VEOL		11
#define	VREPRINT	12
#define	VDISCARD	13
#define	VWERASE		14
#define	VLNEXT		15
#define	VEOL2		16

/* c_iflag */
#define	IGNBRK		0000001
#define	BRKINT		0000002
#define	IGNPAR		0000004
#define	PARMRK		0000010
#define	INPCK		0000020
#define	ISTRIP		0000040
#define	INLCR		0000100
#define	IGNCR		0000200
#define	ICRNL		0000400
#define	IXON		0001000
#define	IXOFF		0002000
#define	IXANY		0004000
#define	IMAXBEL		0010000
#define	IUTF8		0040000
#define	OPOST		0000001
#define	OLCUC		0000002
#define	ONLCR		0000004
#define	OCRNL		0000010
#define	ONOCR		0000020
#define	ONLRET		0000040
#define	OFILL		0000100
#define	OFDEL		0000200
#define	CBAUD		0010017
#define	CSIZE		0000060
#define	CS5		0000000
#define	CS6		0000020
#define	CS7		0000040
#define	CS8		0000060
#define	CSTOPB		0000100
#define	CREAD		0000200
#define	PARENB		0000400
#define	PARODD		0001000
#define	HUPCL		0002000
#define	CLOCAL		0004000
#define	ISIG		0000001
#define	ICANON		0000002
#define	XCASE		0000004
#define	ECHO		0000010
#define	ECHOE		0000020
#define	ECHOK		0000040
#define	ECHONL		0000100
#define	NOFLSH		0000200
#define	TOSTOP		0000400
#define	ECHOCTL		0001000
#define	ECHOPRT		0002000
#define	ECHOKE		0004000
#define	FLUSHO		0010000
#define	PENDIN		0040000
#define	IEXTEN		0100000
#define	CINTR		0x03
#define	CQUIT		0x1c
#define	CERASE		0x08
#define	CKILL		0x15
#define	CEOF		0x04
#define	CSTART		0x11
#define	CSTOP		0x13
#define	CSUSP		0x1a
#define	CREPRINT	0x12
#define	CDISCARD	0x0f
#define	CWERASE		0x17
#define	CLNEXT		0x16
#define	CEOL		0
#define	CEOL2		0
#define	B38400		0000015

typedef unsigned int	tcflag_t;
typedef unsigned char	cc_t;
typedef unsigned int	speed_t;

struct winsize {
	u16	ws_row;
	u16	ws_col;
	u16	ws_xpixel;
	u16	ws_ypixel;
};

struct termios {
	tcflag_t	c_iflag;
	tcflag_t	c_oflag;
	tcflag_t	c_cflag;
	tcflag_t	c_lflag;
	cc_t		c_line;
	cc_t		c_cc[NCCS];
	speed_t		c_ispeed;
	speed_t		c_ospeed;
};

int	terminal_read(void *buf, u32 count);
int	terminal_write(const void *buf, u32 count);
void	terminal_init(void);
int	terminal_is_initialized(void);
void	terminal_reinit(void);
void	terminal_putc_from_kernel(char c);
void	terminal_flush_kernel(void);
void	terminal_set_color(u8 color);
void	terminal_clear_active(void);
void	terminal_log_mirror(char c);
void	terminal_putc_to(int index, char c);
void	terminal_puts_to(int index, const char *s);
void	terminal_set_active(int index);
int	terminal_get_active(void);
void	terminal_restore_active_display(void);
void	terminal_update(void);
void	*terminal_get_input_channel(void);
int	terminal_power_get(int index);
int	terminal_power_set(int index, int state);
int	terminal_power_reset(int index);
int	terminal_power_suspend_all(void);
int	terminal_read_idx(int idx, void *buf, u32 count, int nonblock);
int	terminal_write_idx(int idx, const void *buf, u32 count);
int	terminal_ioctl_idx(int idx, u64 cmd, void *arg);
int	terminal_read_vnode(vnode_t *vn, void *buf, u32 count,
    int nonblock);
int	terminal_read_available(int idx);
int	terminal_write_vnode(vnode_t *vn, const void *buf, u32 count);
int	terminal_ioctl_vnode(vnode_t *vn, u64 cmd, void *arg);
void	terminal_set_winsize(int idx, const struct winsize *ws);
void	terminal_get_winsize(int idx, struct winsize *ws);
void	terminal_set_termios(int idx, const struct termios *t);
void	terminal_get_termios(int idx, struct termios *t);
void	terminal_set_session(int idx, u32 sid);
u32	terminal_get_session(int idx);
void	terminal_set_pgrp(int idx, u32 pgid);
u32	terminal_get_pgrp(int idx);
void	terminal_hangup(int idx);

#endif
