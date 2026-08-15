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

/* !DEFINES!

$define %type registers_t as struct with CPU register snapshot
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed

$define %func syscall_init as procedure with args void
$define %func syscall_handler as procedure with args registers_t *
$define %func syscall_is_initialized as function with args void

*/

/* !SPACE!

$space %export syscall_init, syscall_handler, syscall_is_initialized

*/

#ifndef SYSCALL_H
#define SYSCALL_H

#include <kernel/interrupts/idt.h>
#include <mlibc/mlibc.h>

#define	CALL_TERM_READ		0x100
#define	CALL_TERM_WRITE		0x101
#define	CALL_TERM_INFO		0x102
#define	CALL_TERM_MODE		0x103
#define	CALL_TERM_POWER		0x111
#define	CALL_TERM_MOUSE		0x112
#define	CALL_INPUT_READ		0x120
#define	CALL_INPUT_POLL		0x121
#define	CALL_INPUT_FLUSH	0x122
#define	CALL_DATA_OPEN		0x200
#define	CALL_DATA_CLOSE		0x201
#define	CALL_DATA_READ		0x202
#define	CALL_DATA_WRITE		0x203
#define	CALL_DATA_SEEK		0x204
#define	CALL_DATA_PIPE		0x205
#define	CALL_DATA_DIR		0x210
#define	CALL_FS_CHDIR		0x206
#define	CALL_FS_GETCWD		0x207
#define	CALL_FS_LISTDIR		0x208
#define	CALL_FS_STAT		0x209
#define	CALL_FS_RENAME		0x20A
#define	CALL_FS_UNLINK		0x20B
#define	CALL_FS_LINKNEW		0x20C
#define	CALL_FS_LINKGO		0x20D
#define	CALL_FS_MNT		0x20E
#define	CALL_FS_UMNT		0x20F
#define	CALL_MEM_MAP		0x300	
#define	CALL_MEM_UNMAP		0x301
#define	CALL_SHM_GET		0x302
#define	CALL_SHM_MAP		0x303
#define	CALL_SHM_CTL		0x304
#define	CALL_PROC_CLONE		0x400
#define	CALL_PROC_COPY		0x401
#define	CALL_PROC_SPAWN		0x402
#define	CALL_PROC_EXIT		0x403
#define	CALL_PROC_WAIT		0x404
#define	CALL_PROC_KILL		0x405
#define	CALL_PROC_LIST		0x406
#define	CALL_KUSR_AUTH		0x407
#define	CALL_SYS_INFO		0x500
#define	CALL_SYS_MEMINFO	0x501
#define	CALL_SYS_KMEMINFO	0x502
#define	CALL_DRM_CALL		0x600

#define	CALL_EVENT_KQUEUE	0x700
#define	CALL_EVENT_KEVENT	0x701
#define	CALL_EVENT_CLOSE	0x702

#define	CALL_NET_OPEN		0x800
#define	CALL_NET_BIND		0x801
#define	CALL_NET_CONNECT	0x802
#define	CALL_NET_SEND		0x803
#define	CALL_NET_RECV		0x804
#define	CALL_NET_CTL		0x805
#define	CALL_NET_LISTEN		0x806
#define	CALL_NET_ACCEPT		0x807

#define	CALL_TRACE_OPEN		0x900
#define	CALL_TRACE_CLOSE	0x901
#define	CALL_TRACE_READ		0x902
#define	CALL_TRACE_CTL		0x903
#define	CALL_TRACE_INFO		0x904
#define	CALL_TRACE_MARK		0x905

#define	CALL_REG_OPEN		0xA00
#define	CALL_REG_CLOSE		0xA01
#define	CALL_REG_GET		0xA02
#define	CALL_REG_SET		0xA03
#define	CALL_REG_CREATE_KEY	0xA04
#define	CALL_REG_DELETE_KEY	0xA05
#define	CALL_REG_DELETE_VALUE	0xA06
#define	CALL_REG_ENUM		0xA07
#define	CALL_REG_UPD		0xA08

#define	CALL_IPC_CREATE	0xB00
#define	CALL_IPC_CONNECT	0xB01
#define	CALL_IPC_SEND		0xB02
#define	CALL_IPC_RECV		0xB03
#define	CALL_IPC_CALL		0xB04
#define	CALL_IPC_CTL		0xB05

#define	CALL_KOFO_LOAD		0xC00
#define	CALL_KOFO_INFO		0xC01
#define	CALL_KOFO_UNLOAD	0xC02

#define	CALL_ENTITY_CREATE	0xD00
#define	CALL_ENTITY_OPEN	0xD01
#define	CALL_ENTITY_CLOSE	0xD02
#define	CALL_ENTITY_DUP		0xD03
#define	CALL_ENTITY_STAT	0xD04
#define	CALL_ENTITY_LIST	0xD05
#define	CALL_ENTITY_CTL		0xD06
#define	CALL_ENTITY_QUERY	0xD07
#define	CALL_ENTITY_READ	0xD08
#define	CALL_ENTITY_WRITE	0xD09
#define	CALL_ENTITY_SEEK	0xD0A

#define	CALL_PROC_GETPID	0x408
#define	CALL_PROC_GETPPID	0x409
#define	CALL_PROC_THREAD_EXIT	0x40A
#define	CALL_PROC_THREAD_JOIN	0x40B
#define	CALL_PROC_GETTID	0x40C
#define	CALL_PROC_EXIT_GROUP	0x40D
#define	CALL_PROC_SET_TID_ADDR	0x40E
#define	CALL_PROC_SETSID		0x411
#define	CALL_PROC_GETSID		0x412
#define	CALL_PROC_PERM		0x413
#define	CALL_FUTEX_WAIT		0x40F
#define	CALL_FUTEX_WAKE		0x410
#define	CALL_SYS_RANDOM		0x503
#define	CALL_SYS_TIMEINFO	0x504
#define	CALL_SYS_TIME		0x505
#define	CALL_SYS_CPUINFO	0x506
#define	CALL_POWER_STATE	0x507

#define	CALL_PERSONALITY	0xFFFF

void	syscall_init(void);
void	syscall_handler(registers_t *regs);
int	syscall_is_initialized(void);

#endif
