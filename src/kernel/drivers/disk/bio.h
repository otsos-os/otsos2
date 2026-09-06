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

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type bio_done_fn as pointer to a completion callback taking one bio
$define %type bio_t as struct with one block request, its status and callback
$define %type bio_stat_t as struct with block layer accounting counters

$define %func bio_init as procedure with args bio_t *, disk_t *, u32
$define %func bio_submit as function with args bio_t *
$define %func bio_done as procedure with args bio_t *, int, u32
$define %func bio_read as function with args disk_t *, u64, u32, void *
$define %func bio_write as function with args disk_t *, u64, u32, const void *
$define %func bio_flush as function with args disk_t *
$define %func bio_wait as function with args bio_t *, u64
$define %func bio_status_name as function with args int
$define %func bio_stats as procedure with args bio_stat_t *
$define %func bio_dump as procedure with args void

$const BIO_READ as command reading sectors from the device into memory
$const BIO_WRITE as command writing sectors from memory to the device
$const BIO_FLUSH as command committing the device volatile write cache
$const BIO_STATUS_OK as status meaning the request completed in full
$const BIO_TIMEOUT_DEFAULT_NS as default ceiling on one synchronous request

*/

/* !SPACE!

$space %export bio_init, bio_submit, bio_done, bio_wait
$space %export bio_read, bio_write, bio_flush
$space %export bio_status_name, bio_stats, bio_dump

*/

#ifndef KERNEL_DRIVERS_DISK_BIO_H
#define KERNEL_DRIVERS_DISK_BIO_H

#include <kernel/drivers/disk/disk.h>
#include <mlibc/mlibc.h>


#define	BIO_READ	1
#define	BIO_WRITE	2
#define	BIO_FLUSH	3
#define	BIO_STATUS_OK		0
#define	BIO_STATUS_IOERR	(-1)
#define	BIO_STATUS_TIMEOUT	(-2)
#define	BIO_STATUS_INVAL	(-3)
#define	BIO_STATUS_NODEV	(-4)
#define	BIO_STATUS_NOMEM	(-5)
#define	BIO_STATUS_MEDIUM	(-6)
#define	BIO_STATUS_READONLY	(-7)
#define	BIO_STATUS_UNSUPP	(-8)
#define	BIO_F_INFLIGHT		0x0001
#define	BIO_F_DONE		0x0002
#define	BIO_TIMEOUT_DEFAULT_NS	30000000000ULL

typedef void	(*bio_done_fn)(struct bio *bio);

typedef struct bio {
	disk_t		*disk;
	void		*buf;
	void		*driver_priv;
	bio_done_fn	done;
	void		*done_arg;
	u64		lba;
	u64		submit_ns;
	u32		nsectors;
	u32		resid;
	u32		cmd;
	u32		flags;
	int		status;
	int		pad;
} bio_t;

typedef struct bio_stat {
	u64	submits;
	u64	completions;
	u64	reads;
	u64	writes;
	u64	flushes;
	u64	sectors_read;
	u64	sectors_written;
	u64	inflight;
	u64	inflight_peak;
	u64	fail_inval;
	u64	fail_nodev;
	u64	fail_io;
	u64	fail_timeout;
	u64	fail_readonly;
	u64	short_transfers;
} bio_stat_t;

void		bio_init(bio_t *bio, disk_t *disk, u32 cmd);
int		bio_submit(bio_t *bio);
void		bio_done(bio_t *bio, int status, u32 resid);
int		bio_wait(bio_t *bio, u64 timeout_ns);

int		bio_read(disk_t *disk, u64 lba, u32 nsectors, void *buf);
int		bio_write(disk_t *disk, u64 lba, u32 nsectors, const void *buf);
int		bio_flush(disk_t *disk);

const char	*bio_status_name(int status);
void		bio_stats(bio_stat_t *out);
void		bio_dump(void);

#endif
