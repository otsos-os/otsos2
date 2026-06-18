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

#ifndef SYSCALL_H
#define SYSCALL_H

#include <kernel/interrupts/idt.h>
#include <mlibc/mlibc.h>

#define CALL_TERM_READ 0x100
#define CALL_TERM_WRITE 0x101
#define CALL_DATA_OPEN 0x200
#define CALL_DATA_CLOSE 0x201
#define CALL_DATA_READ 0x202
#define CALL_DATA_WRITE 0x203
#define CALL_DATA_SEEK 0x204
#define CALL_DATA_PIPE 0x205
#define CALL_FS_CHDIR 0x206
#define CALL_FS_GETCWD 0x207
#define CALL_FS_LISTDIR 0x208
#define CALL_MEM_MAP   0x300
#define CALL_MEM_UNMAP 0x301
#define CALL_PROC_CLONE 0x400
#define CALL_PROC_COPY 0x401
#define CALL_PROC_SPAWN 0x402
#define CALL_PROC_EXIT 0x403
#define CALL_PROC_WAIT 0x404
#define CALL_PROC_KILL 0x405
#define CALL_PROC_LIST 0x406
#define CALL_KUSR_AUTH 0x407
#define CALL_SYS_INFO 0x500
#define CALL_DRM_CALL 0x600

void syscall_init(void);
void syscall_handler(registers_t *regs);
int syscall_is_initialized(void);

#endif
