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
$define %type bio_t as struct with one block request, its status and callback
$define %type bio_stat_t as struct with block layer accounting counters
$define %type disk_t as struct with one registered block device and its geometry

$define %func bio_now_ns as function with args void
$define %func bio_fail as function with args bio_t *, int
$define %func bio_account_done as procedure with args bio_t *
$define %func bio_rw_sync as function with args disk_t *, u64, u32, void *, u32
$define %func bio_init as procedure with args bio_t *, disk_t *, u32
$define %func bio_submit as function with args bio_t *
$define %func bio_done as procedure with args bio_t *, int, u32
$define %func bio_wait as function with args bio_t *, u64
$define %func bio_read as function with args disk_t *, u64, u32, void *
$define %func bio_write as function with args disk_t *, u64, u32, const void *
$define %func bio_flush as function with args disk_t *
$define %func bio_status_name as function with args int
$define %func bio_stats as procedure with args bio_stat_t *
$define %func bio_dump as procedure with args void

$const BIO_ABORT_GRACE_NS as wait granted to an accepted abort before griping

*/

/* !SPACE!

$space %internal bio_now_ns, bio_fail, bio_account_done, bio_rw_sync
$space %export bio_init, bio_submit, bio_done, bio_wait
$space %export bio_read, bio_write, bio_flush
$space %export bio_status_name, bio_stats, bio_dump

*/

#include <kernel/drivers/disk/bio.h>
#include <kernel/drivers/disk/disk.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/process.h>
#include <kernel/time.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

#define	BIO_ABORT_GRACE_NS	2000000000ULL

static bio_stat_t	bio_g;

static u64
bio_now_ns(void)
{
	struct timespec	ts;

	getnanouptime(&ts);
	return ((u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec);
}

static int
bio_fail(bio_t *bio, int status)
{
	if (bio != NULL) {
		bio->status = status;
		bio->resid = bio->nsectors;
	}
	switch (status) {
	case BIO_STATUS_NODEV:
		__atomic_fetch_add(&bio_g.fail_nodev, 1, __ATOMIC_RELAXED);
		break;
	case BIO_STATUS_READONLY:
		__atomic_fetch_add(&bio_g.fail_readonly, 1, __ATOMIC_RELAXED);
		break;
	default:
		__atomic_fetch_add(&bio_g.fail_inval, 1, __ATOMIC_RELAXED);
		break;
	}
	return (status);
}

void
bio_init(bio_t *bio, disk_t *disk, u32 cmd)
{
	if (bio == NULL) {
		return;
	}
	memset(bio, 0, sizeof(*bio));
	bio->disk = disk;
	bio->cmd = cmd;
	bio->status = BIO_STATUS_OK;
}

int
bio_submit(bio_t *bio)
{
	disk_t	*disk;
	u64	inflight, last;
	int	error, nodev_flush;

	if (bio == NULL) {
		return (BIO_STATUS_INVAL);
	}
	nodev_flush = 0;
	disk = bio->disk;
	if (disk == NULL || disk->ops == NULL ||
	    disk->ops->submit == NULL) {
		return (bio_fail(bio, BIO_STATUS_NODEV));
	}
	
	if ((bio->flags & (BIO_F_INFLIGHT | BIO_F_DONE)) != 0) {
		return (bio_fail(bio, BIO_STATUS_INVAL));
	}
	if (disk->sector_size == 0 || disk->max_io_sectors == 0) {
		return (bio_fail(bio, BIO_STATUS_NODEV));
	}

	switch (bio->cmd) {
	case BIO_READ:
	case BIO_WRITE:
		if (bio->buf == NULL || bio->nsectors == 0) {
			return (bio_fail(bio, BIO_STATUS_INVAL));
		}
		if (bio->cmd == BIO_WRITE &&
		    (disk->flags & DISK_F_READONLY) != 0) {
			return (bio_fail(bio, BIO_STATUS_READONLY));
		}
		last = bio->lba + (u64)bio->nsectors;
		if (last < bio->lba || last > disk->total_sectors) {
			return (bio_fail(bio, BIO_STATUS_INVAL));
		}
		if (bio->nsectors > disk->max_io_sectors) {
			return (bio_fail(bio, BIO_STATUS_INVAL));
		}
		break;
	case BIO_FLUSH:
	
		if ((disk->flags & DISK_F_NO_FLUSH) != 0) {
			nodev_flush = 1;
			break;
		}
		break;
	default:
		return (bio_fail(bio, BIO_STATUS_INVAL));
	}

	bio->status = BIO_STATUS_OK;
	bio->resid = (bio->cmd == BIO_FLUSH) ? 0 : bio->nsectors;
	bio->submit_ns = bio_now_ns();

	__atomic_or_fetch(&bio->flags, BIO_F_INFLIGHT, __ATOMIC_RELEASE);

	inflight = __atomic_add_fetch(&bio_g.inflight, 1, __ATOMIC_RELAXED);
	if (inflight > __atomic_load_n(&bio_g.inflight_peak,
	    __ATOMIC_RELAXED)) {
		__atomic_store_n(&bio_g.inflight_peak, inflight,
		    __ATOMIC_RELAXED);
	}
	__atomic_fetch_add(&bio_g.submits, 1, __ATOMIC_RELAXED);

	
	if (nodev_flush != 0) {
		bio_done(bio, BIO_STATUS_OK, 0);
		return (0);
	}

	error = disk->ops->submit(disk, bio);
	if (error != 0) {
		
		__atomic_and_fetch(&bio->flags, (u32)~BIO_F_INFLIGHT,
		    __ATOMIC_RELEASE);
		__atomic_fetch_sub(&bio_g.inflight, 1, __ATOMIC_RELAXED);
		__atomic_fetch_add(&bio_g.fail_io, 1, __ATOMIC_RELAXED);
		bio->status = BIO_STATUS_IOERR;
		bio->resid = bio->nsectors;
		return (BIO_STATUS_IOERR);
	}
	return (0);
}

static void
bio_account_done(bio_t *bio)
{
	u64	moved;

	moved = (u64)bio->nsectors - (u64)bio->resid;

	__atomic_fetch_add(&bio_g.completions, 1, __ATOMIC_RELAXED);
	if (__atomic_load_n(&bio_g.inflight, __ATOMIC_RELAXED) != 0) {
		__atomic_fetch_sub(&bio_g.inflight, 1, __ATOMIC_RELAXED);
	}

	switch (bio->cmd) {
	case BIO_READ:
		__atomic_fetch_add(&bio_g.reads, 1, __ATOMIC_RELAXED);
		__atomic_fetch_add(&bio_g.sectors_read, moved,
		    __ATOMIC_RELAXED);
		break;
	case BIO_WRITE:
		__atomic_fetch_add(&bio_g.writes, 1, __ATOMIC_RELAXED);
		__atomic_fetch_add(&bio_g.sectors_written, moved,
		    __ATOMIC_RELAXED);
		break;
	case BIO_FLUSH:
		__atomic_fetch_add(&bio_g.flushes, 1, __ATOMIC_RELAXED);
		break;
	default:
		break;
	}

	if (bio->resid != 0 && bio->cmd != BIO_FLUSH) {
		__atomic_fetch_add(&bio_g.short_transfers, 1,
		    __ATOMIC_RELAXED);
	}
	switch (bio->status) {
	case BIO_STATUS_OK:
		break;
	case BIO_STATUS_TIMEOUT:
		__atomic_fetch_add(&bio_g.fail_timeout, 1, __ATOMIC_RELAXED);
		break;
	default:
		__atomic_fetch_add(&bio_g.fail_io, 1, __ATOMIC_RELAXED);
		break;
	}
}

void
bio_done(bio_t *bio, int status, u32 resid)
{
	u32	flags;

	if (bio == NULL) {
		return;
	}

	flags = __atomic_load_n(&bio->flags, __ATOMIC_ACQUIRE);
	if ((flags & BIO_F_DONE) != 0) {
		printk("bio: double completion, cmd %u lba %u (driver bug)\n",
		    bio->cmd, (u32)bio->lba);
		return;
	}
	if ((flags & BIO_F_INFLIGHT) == 0) {
		printk("bio: completion of an idle request, cmd %u lba %u "
		    "(driver bug)\n", bio->cmd, (u32)bio->lba);
		return;
	}

	if (resid > bio->nsectors) {
		resid = bio->nsectors;
	}
	bio->status = status;
	bio->resid = resid;
	bio_account_done(bio);

	flags &= ~BIO_F_INFLIGHT;
	flags |= BIO_F_DONE;
	__atomic_store_n(&bio->flags, flags, __ATOMIC_RELEASE);

	if (bio->done != NULL) {
		bio->done(bio);
	}
}

int
bio_wait(bio_t *bio, u64 timeout_ns)
{
	disk_t	*disk;
	u64	now, deadline, grumbled;
	u32	flags;
	int	aborting, error;

	if (bio == NULL) {
		return (BIO_STATUS_INVAL);
	}
	flags = __atomic_load_n(&bio->flags, __ATOMIC_ACQUIRE);
	if ((flags & BIO_F_DONE) != 0) {
		return (bio->status);
	}
	if ((flags & BIO_F_INFLIGHT) == 0) {
		return (BIO_STATUS_INVAL);
	}

	disk = bio->disk;
	if (timeout_ns == 0) {
		timeout_ns = BIO_TIMEOUT_DEFAULT_NS;
	}
	deadline = bio_now_ns() + timeout_ns;
	grumbled = 0;
	aborting = 0;

	for (;;) {
		if ((__atomic_load_n(&bio->flags, __ATOMIC_ACQUIRE) &
		    BIO_F_DONE) != 0) {
			return (bio->status);
		}

		now = bio_now_ns();
		if (now >= deadline) {
			if (aborting == 0) {
				aborting = 1;
				error = -1;
				if (disk != NULL && disk->ops != NULL &&
				    disk->ops->timeout != NULL) {
					error = disk->ops->timeout(disk, bio);
				}
				if (error != 0) {
					printk("bio: %s cmd %u lba %u timed "
					    "out and cannot be aborted; "
					    "waiting to avoid a stale write\n",
					    disk != NULL ? disk->name : "?",
					    bio->cmd, (u32)bio->lba);
				}
				grumbled = now + BIO_ABORT_GRACE_NS;
			} else if (now >= grumbled) {
				printk("bio: %s abort did not complete cmd %u "
				    "lba %u within grace\n",
				    disk != NULL ? disk->name : "?",
				    bio->cmd, (u32)bio->lba);
				grumbled = now + BIO_ABORT_GRACE_NS;
			}
		}

		process_yield();
	}
}


static int
bio_rw_sync(disk_t *disk, u64 lba, u32 nsectors, void *buf, u32 cmd)
{
	bio_t	bio;
	u8	*p;
	u32	chunk, done;
	int	error;

	if (disk == NULL || buf == NULL || nsectors == 0) {
		return (BIO_STATUS_INVAL);
	}
	if (disk->sector_size == 0 || disk->max_io_sectors == 0) {
		return (BIO_STATUS_NODEV);
	}

	p = (u8 *)buf;
	done = 0;

	while (done < nsectors) {
		chunk = nsectors - done;
		if (chunk > disk->max_io_sectors) {
			chunk = disk->max_io_sectors;
		}

		bio_init(&bio, disk, cmd);
		bio.lba = lba + (u64)done;
		bio.nsectors = chunk;
		bio.buf = p;

		error = bio_submit(&bio);
		if (error != 0) {
			return (error);
		}
		error = bio_wait(&bio, 0);
		if (error != BIO_STATUS_OK) {
			return (error);
		}
		if (bio.resid != 0) {
			return (BIO_STATUS_IOERR);
		}

		done += chunk;
		p += (u64)chunk * disk->sector_size;
	}
	return (BIO_STATUS_OK);
}

int
bio_read(disk_t *disk, u64 lba, u32 nsectors, void *buf)
{
	return (bio_rw_sync(disk, lba, nsectors, buf, BIO_READ));
}

int
bio_write(disk_t *disk, u64 lba, u32 nsectors, const void *buf)
{
	
	return (bio_rw_sync(disk, lba, nsectors, (void *)(u64)buf, BIO_WRITE));
}

int
bio_flush(disk_t *disk)
{
	bio_t	bio;
	int	error;

	if (disk == NULL) {
		return (BIO_STATUS_NODEV);
	}

	bio_init(&bio, disk, BIO_FLUSH);
	error = bio_submit(&bio);
	if (error != 0) {
		return (error);
	}
	
	return (bio_wait(&bio, 0));
}

const char *
bio_status_name(int status)
{
	switch (status) {
	case BIO_STATUS_OK:
		return ("ok");
	case BIO_STATUS_IOERR:
		return ("io error");
	case BIO_STATUS_TIMEOUT:
		return ("timeout");
	case BIO_STATUS_INVAL:
		return ("invalid request");
	case BIO_STATUS_NODEV:
		return ("no device");
	case BIO_STATUS_NOMEM:
		return ("out of memory");
	case BIO_STATUS_MEDIUM:
		return ("medium error");
	case BIO_STATUS_READONLY:
		return ("read-only device");
	case BIO_STATUS_UNSUPP:
		return ("unsupported command");
	default:
		return ("unknown");
	}
}

void
bio_stats(bio_stat_t *out)
{
	if (out == NULL) {
		return;
	}
	out->submits = __atomic_load_n(&bio_g.submits, __ATOMIC_RELAXED);
	out->completions = __atomic_load_n(&bio_g.completions,
	    __ATOMIC_RELAXED);
	out->reads = __atomic_load_n(&bio_g.reads, __ATOMIC_RELAXED);
	out->writes = __atomic_load_n(&bio_g.writes, __ATOMIC_RELAXED);
	out->flushes = __atomic_load_n(&bio_g.flushes, __ATOMIC_RELAXED);
	out->sectors_read = __atomic_load_n(&bio_g.sectors_read,
	    __ATOMIC_RELAXED);
	out->sectors_written = __atomic_load_n(&bio_g.sectors_written,
	    __ATOMIC_RELAXED);
	out->inflight = __atomic_load_n(&bio_g.inflight, __ATOMIC_RELAXED);
	out->inflight_peak = __atomic_load_n(&bio_g.inflight_peak,
	    __ATOMIC_RELAXED);
	out->fail_inval = __atomic_load_n(&bio_g.fail_inval, __ATOMIC_RELAXED);
	out->fail_nodev = __atomic_load_n(&bio_g.fail_nodev, __ATOMIC_RELAXED);
	out->fail_io = __atomic_load_n(&bio_g.fail_io, __ATOMIC_RELAXED);
	out->fail_timeout = __atomic_load_n(&bio_g.fail_timeout,
	    __ATOMIC_RELAXED);
	out->fail_readonly = __atomic_load_n(&bio_g.fail_readonly,
	    __ATOMIC_RELAXED);
	out->short_transfers = __atomic_load_n(&bio_g.short_transfers,
	    __ATOMIC_RELAXED);
}

void
bio_dump(void)
{
	bio_stat_t	st;

	bio_stats(&st);

	printk("bio: submits %u completions %u inflight %u (peak %u)\n",
	    (u32)st.submits, (u32)st.completions, (u32)st.inflight,
	    (u32)st.inflight_peak);
	printk("bio: reads %u (%u sectors) writes %u (%u sectors) "
	    "flushes %u\n", (u32)st.reads, (u32)st.sectors_read,
	    (u32)st.writes, (u32)st.sectors_written, (u32)st.flushes);
	
	printk("bio: fail inval %u nodev %u io %u timeout %u readonly %u "
	    "short %u\n", (u32)st.fail_inval, (u32)st.fail_nodev,
	    (u32)st.fail_io, (u32)st.fail_timeout, (u32)st.fail_readonly,
	    (u32)st.short_transfers);
}
