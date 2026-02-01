/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PATA_H
#define PATA_H

#include <mlibc/mlibc.h>

#define IDE_DATA 0x1F0
#define IDE_ERROR 0x1F1
#define IDE_FEATURES 0x1F1
#define IDE_SEC_COUNT 0x1F2
#define IDE_LBA_LOW 0x1F3
#define IDE_LBA_MID 0x1F4
#define IDE_LBA_HIGH 0x1F5
#define IDE_DRIVE_SEL 0x1F6
#define IDE_COMMAND 0x1F7
#define IDE_STATUS 0x1F7

#define IDE_CMD_READ 0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_IDENTIFY 0xEC

#define IDE_STATUS_BSY 0x80 // Busy
#define IDE_STATUS_DRQ 0x08 // Data Request
#define IDE_STATUS_ERR 0x01 // Error

void pata_read_sector(unsigned int lba, unsigned char *buffer);
void pata_write_sector(unsigned int lba, unsigned char *buffer);
void pata_identify(unsigned short *target_buf);

#endif
