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
#include "gpt.h"

#include <kernel/drivers/disk/bio.h>
#include <kernel/drivers/disk/blockdev.h>
#include <kernel/crypto/rng/rng.h>
#include <kernel/drivers/newbus/newbus.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>
#include <mm/kmem.h>

static const u8 gpt_type_esp[16] = {
	0x28, 0x73, 0x2a, 0xc1, 0x1f, 0xf8, 0xd2, 0x11,
	0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b
};

static const u8 gpt_type_bios[16] = {
	0x48, 0x61, 0x68, 0x21, 0x49, 0x64, 0x6f, 0x6e,
	0x74, 0x4e, 0x65, 0x65, 0x64, 0x45, 0x46, 0x49
};

static const u8 gpt_type_otsos[16] = {
	0x4f, 0x54, 0x53, 0x4f, 0x53, 0x32, 0x00, 0x11,
	0x9c, 0xf5, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b
};

typedef struct gpt_slice {
	disk_t		disk;
	disk_t		*parent;
	device_t	dev;
	u32		entry_index;
	int		used;
} gpt_slice_t;

typedef struct gpt_layout {
	disk_t		*disk;
	gpt_entry_t	entries[GPT_ENTRY_COUNT];
	u64		next_lba;
	u64		first_usable;
	u64		last_usable;
	u32		count;
	int		active;
} gpt_layout_t;

static gpt_slice_t	gpt_slices[GPT_MAX_SLICES];
static gpt_layout_t	gpt_pending;
static u32		gpt_crc_table[256];
static int		gpt_crc_ready;

static void
gpt_crc32_init(void)
{
	u32	c;
	int	i, k;

	if (gpt_crc_ready) {
		return;
	}
	for (i = 0; i < 256; i++) {
		c = (u32)i;
		for (k = 0; k < 8; k++) {
			if ((c & 1) != 0) {
				c = 0xEDB88320U ^ (c >> 1);
			} else {
				c = c >> 1;
			}
		}
		gpt_crc_table[i] = c;
	}
	gpt_crc_ready = 1;
}

u32
gpt_crc32(const void *data, u32 len)
{
	const u8	*p;
	u32		crc;
	u32		i;

	gpt_crc32_init();
	p = (const u8 *)data;
	crc = 0xFFFFFFFFU;
	for (i = 0; i < len; i++) {
		crc = gpt_crc_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
	}
	return (crc ^ 0xFFFFFFFFU);
}

static int
gpt_slice_submit(disk_t *self, bio_t *bio)
{
	gpt_slice_t	*slice;
	u64		end;

	slice = (gpt_slice_t *)self->private_data;
	if (slice == NULL || slice->parent == NULL || bio == NULL) {
		return (-1);
	}
	if (bio->cmd == BIO_FLUSH) {
		return (slice->parent->ops->submit(slice->parent, bio));
	}
	end = bio->lba + (u64)bio->nsectors;
	if (end > self->total_sectors || end < bio->lba) {
		bio_done(bio, BIO_STATUS_INVAL, bio->nsectors);
		return (0);
	}
	bio->lba += self->base_lba;
	bio->disk = slice->parent;
	return (slice->parent->ops->submit(slice->parent, bio));
}

static const disk_ops_t gpt_slice_ops = {
	.submit		= gpt_slice_submit,
	.timeout	= NULL,
};

static gpt_slice_t *
gpt_slice_alloc(void)
{
	int	i;

	for (i = 0; i < GPT_MAX_SLICES; i++) {
		if (!gpt_slices[i].used) {
			memset(&gpt_slices[i], 0, sizeof(gpt_slices[i]));
			gpt_slices[i].used = 1;
			return (&gpt_slices[i]);
		}
	}
	return (NULL);
}

static void
gpt_slice_name(char *out, u32 size, const char *parent, u32 index)
{
	u32	len;

	len = (u32)strlen(parent);
	if (len > 0 && parent[len - 1] >= '0' && parent[len - 1] <= '9') {
		snprintf(out, size, "%sp%u", parent, index);
	} else {
		snprintf(out, size, "%s%u", parent, index);
	}
}

static int
gpt_slice_register(disk_t *parent, const gpt_entry_t *entry, u32 index)
{
	gpt_slice_t	*slice;
	device_t	child;
	char		unitname[DISK_NAME_MAX];
	u64		sectors;

	if (entry->first_lba == 0 || entry->last_lba < entry->first_lba) {
		return (-1);
	}
	sectors = entry->last_lba - entry->first_lba + 1;
	if (entry->last_lba >= parent->total_sectors) {
		drivers_log("[GPT] %s entry %u past end of device\n",
		    parent->name, index);
		return (-1);
	}

	slice = gpt_slice_alloc();
	if (slice == NULL) {
		drivers_log("[GPT] slice table full\n");
		return (-1);
	}

	gpt_slice_name(unitname, sizeof(unitname), parent->name, index);

	slice->parent = parent;
	slice->entry_index = index;
	strncpy(slice->disk.name, unitname, DISK_NAME_MAX - 1);
	slice->disk.type = DISK_TYPE_SLICE;
	slice->disk.sector_size = parent->sector_size;
	slice->disk.total_sectors = sectors;
	slice->disk.base_lba = entry->first_lba;
	slice->disk.max_io_sectors = parent->max_io_sectors;
	slice->disk.flags = (parent->flags & DISK_F_NO_FLUSH) | DISK_F_SLICE;
	slice->disk.ops = &gpt_slice_ops;
	slice->disk.private_data = slice;
	slice->disk.parent = parent;

	if (parent->dev != NULL) {
		child = device_add_child(parent->dev, unitname, (int)index);
		if (child != NULL) {
			slice->dev = child;
			slice->disk.dev = child;
		}
	}

	if (disk_register(&slice->disk) < 0) {
		if (slice->dev != NULL && parent->dev != NULL) {
			(void)device_delete_child(parent->dev, slice->dev);
		}
		slice->used = 0;
		return (-1);
	}
	drivers_log("[GPT] %s: lba %llu..%llu (%llu MiB)\n",
	    slice->disk.name, (unsigned long long)entry->first_lba,
	    (unsigned long long)entry->last_lba,
	    (unsigned long long)((sectors * parent->sector_size) >> 20));
	return (0);
}

void
gpt_detach(disk_t *disk)
{
	int	i;

	for (i = 0; i < GPT_MAX_SLICES; i++) {
		if (!gpt_slices[i].used || gpt_slices[i].parent != disk) {
			continue;
		}
		(void)disk_unregister(&gpt_slices[i].disk);
		if (gpt_slices[i].dev != NULL && disk->dev != NULL) {
			(void)device_delete_child(disk->dev,
			    gpt_slices[i].dev);
		}
		gpt_slices[i].used = 0;
	}
}

static int
gpt_guid_zero(const u8 *g)
{
	int	i;

	for (i = 0; i < 16; i++) {
		if (g[i] != 0) {
			return (0);
		}
	}
	return (1);
}

static int
gpt_guid_fill(u8 *g)
{
	if (crypto_rng_bytes(g, 16) != 0) {
		return (-1);
	}
	g[6] = (u8)((g[6] & 0x0F) | 0x40);
	g[8] = (u8)((g[8] & 0x3F) | 0x80);
	return (0);
}

static void
gpt_name_utf16(u16 *dst, const char *src)
{
	int	i;

	memset(dst, 0, 36 * sizeof(u16));
	if (src == NULL) {
		return;
	}
	for (i = 0; i < 35 && src[i] != '\0'; i++) {
		dst[i] = (u16)(unsigned char)src[i];
	}
}

static int
gpt_read_header(disk_t *disk, gpt_header_t *hdr)
{
	u8	*sector;
	u32	saved_crc;
	u32	calc;

	if (disk->sector_size < sizeof(gpt_header_t) ||
	    disk->total_sectors < 34) {
		return (-1);
	}
	sector = kmem_alloc(disk->sector_size);
	if (sector == NULL) {
		return (-1);
	}
	if (bio_read(disk, GPT_HEADER_LBA, 1, sector) != BIO_STATUS_OK) {
		kmem_free(sector);
		return (-1);
	}
	memcpy(hdr, sector, sizeof(*hdr));
	kmem_free(sector);
	if (hdr->signature != GPT_SIGNATURE) {
		return (-1);
	}
	if (hdr->header_size < 92 || hdr->header_size > disk->sector_size) {
		return (-1);
	}
	if (hdr->entry_count == 0 || hdr->entry_count > GPT_ENTRY_COUNT) {
		return (-1);
	}
	if (hdr->entry_size != GPT_ENTRY_SIZE) {
		return (-1);
	}
	saved_crc = hdr->header_crc32;
	hdr->header_crc32 = 0;
	calc = gpt_crc32(hdr, hdr->header_size);
	hdr->header_crc32 = saved_crc;
	if (calc != saved_crc) {
		drivers_log("[GPT] %s header CRC mismatch\n", disk->name);
		return (-1);
	}
	return (0);
}

static int
gpt_read_entries(disk_t *disk, const gpt_header_t *hdr, gpt_entry_t *entries)
{
	u8	*buf;
	u32	bytes;
	u32	sectors;
	u32	crc;
	int	error;

	bytes = hdr->entry_count * hdr->entry_size;
	sectors = (bytes + disk->sector_size - 1) / disk->sector_size;
	buf = kmem_alloc(sectors * disk->sector_size);
	if (buf == NULL) {
		return (-1);
	}
	error = bio_read(disk, hdr->entry_lba, sectors, buf);
	if (error != BIO_STATUS_OK) {
		kmem_free(buf);
		return (-1);
	}
	crc = gpt_crc32(buf, bytes);
	if (crc != hdr->entry_crc32) {
		drivers_log("[GPT] %s entry CRC mismatch\n", disk->name);
		kmem_free(buf);
		return (-1);
	}
	memcpy(entries, buf, bytes);
	kmem_free(buf);
	return (0);
}

int
gpt_probe(disk_t *disk)
{
	gpt_header_t	hdr;

	if (disk == NULL) {
		return (-1);
	}
	return (gpt_read_header(disk, &hdr));
}

int
gpt_rescan(disk_t *disk)
{
	gpt_header_t	hdr;
	gpt_entry_t	entries[GPT_ENTRY_COUNT];
	u32		i, published;

	if (disk == NULL || (disk->flags & DISK_F_SLICE) != 0) {
		return (-1);
	}
	gpt_detach(disk);
	if (gpt_read_header(disk, &hdr) != 0) {
		return (-1);
	}
	memset(entries, 0, sizeof(entries));
	if (gpt_read_entries(disk, &hdr, entries) != 0) {
		return (-1);
	}
	published = 0;
	for (i = 0; i < hdr.entry_count && i < GPT_ENTRY_COUNT; i++) {
		if (gpt_guid_zero(entries[i].type_guid)) {
			continue;
		}
		if (gpt_slice_register(disk, &entries[i], published + 1) == 0) {
			published++;
		}
	}
	drivers_log("[GPT] %s: %u partition(s)\n", disk->name, published);
	return (0);
}

int
gpt_layout_init(disk_t *disk)
{
	u64	entry_sectors;

	if (disk == NULL || (disk->flags & DISK_F_SLICE) != 0) {
		return (-1);
	}
	if (disk->total_sectors < 2048) {
		drivers_log("[GPT] %s too small for GPT\n", disk->name);
		return (-1);
	}
	memset(&gpt_pending, 0, sizeof(gpt_pending));
	entry_sectors = (GPT_ENTRY_COUNT * GPT_ENTRY_SIZE +
	    disk->sector_size - 1) / disk->sector_size;
	gpt_pending.disk = disk;
	gpt_pending.first_usable = GPT_ENTRY_LBA + entry_sectors;
	if (gpt_pending.first_usable < GPT_ALIGN_SECTORS) {
		gpt_pending.first_usable = GPT_ALIGN_SECTORS;
	}
	gpt_pending.last_usable = disk->total_sectors - 1 - 1 - entry_sectors;
	if (gpt_pending.last_usable <= gpt_pending.first_usable) {
		return (-1);
	}
	gpt_pending.next_lba = gpt_pending.first_usable;
	gpt_pending.active = 1;
	return (0);
}

int
gpt_layout_add(disk_t *disk, struct dioc_gpt_add *add)
{
	const u8	*type;
	gpt_entry_t	*e;
	u64		size;
	u64		first;
	u64		last;

	if (!gpt_pending.active || gpt_pending.disk != disk || add == NULL) {
		return (-1);
	}
	if (gpt_pending.count >= GPT_ENTRY_COUNT) {
		return (-1);
	}
	switch (add->kind) {
	case DIOC_GPT_KIND_ESP:
		type = gpt_type_esp;
		break;
	case DIOC_GPT_KIND_BIOS:
		type = gpt_type_bios;
		break;
	case DIOC_GPT_KIND_ROOT:
		type = gpt_type_otsos;
		break;
	default:
		return (-1);
	}
	if (add->size_sectors == 0) {
		return (-1);
	}
	first = gpt_pending.next_lba;
	if ((first % GPT_ALIGN_SECTORS) != 0) {
		first = (first + GPT_ALIGN_SECTORS - 1) &
		    ~(u64)(GPT_ALIGN_SECTORS - 1);
	}
	size = add->size_sectors;
	if (add->kind == DIOC_GPT_KIND_ROOT && size == ~0ULL) {
		last = gpt_pending.last_usable;
	} else {
		last = first + size - 1;
	}
	if (last > gpt_pending.last_usable || last < first) {
		return (-1);
	}
	e = &gpt_pending.entries[gpt_pending.count];
	memcpy(e->type_guid, type, 16);
	if (gpt_guid_fill(e->unique_guid) != 0) {
		return (-1);
	}
	e->first_lba = first;
	e->last_lba = last;
	e->attributes = 0;
	gpt_name_utf16(e->name, add->name[0] != '\0' ? add->name : "OTSOS");
	add->index = gpt_pending.count + 1;
	add->first_lba = first;
	add->last_lba = last;
	gpt_pending.count++;
	gpt_pending.next_lba = last + 1;
	return (0);
}

static int
gpt_write_protective_mbr(disk_t *disk)
{
	u8	*mbr;
	u32	lba32;
	int	error;

	mbr = kmem_alloc(disk->sector_size);
	if (mbr == NULL) {
		return (-1);
	}
	memset(mbr, 0, disk->sector_size);
	mbr[446] = 0x00;
	mbr[447] = 0x00;
	mbr[448] = 0x02;
	mbr[449] = 0x00;
	mbr[450] = 0xEE;
	mbr[451] = 0xFF;
	mbr[452] = 0xFF;
	mbr[453] = 0xFF;
	mbr[454] = 0x01;
	mbr[455] = 0x00;
	mbr[456] = 0x00;
	mbr[457] = 0x00;
	lba32 = (u32)disk->total_sectors - 1;
	if (disk->total_sectors > 0xFFFFFFFFULL) {
		lba32 = 0xFFFFFFFFU;
	}
	mbr[458] = (u8)(lba32);
	mbr[459] = (u8)(lba32 >> 8);
	mbr[460] = (u8)(lba32 >> 16);
	mbr[461] = (u8)(lba32 >> 24);
	mbr[510] = 0x55;
	mbr[511] = 0xAA;
	error = bio_write(disk, 0, 1, mbr);
	kmem_free(mbr);
	return (error == BIO_STATUS_OK ? 0 : -1);
}

static int
gpt_write_header(disk_t *disk, gpt_header_t *hdr, u64 lba)
{
	u8	*sector;
	int	error;

	sector = kmem_alloc(disk->sector_size);
	if (sector == NULL) {
		return (-1);
	}
	memset(sector, 0, disk->sector_size);
	hdr->header_crc32 = 0;
	hdr->header_crc32 = gpt_crc32(hdr, hdr->header_size);
	memcpy(sector, hdr, sizeof(*hdr));
	error = bio_write(disk, lba, 1, sector);
	kmem_free(sector);
	return (error == BIO_STATUS_OK ? 0 : -1);
}

int
gpt_layout_commit(disk_t *disk)
{
	gpt_header_t	hdr;
	gpt_header_t	alt;
	u8		*buf;
	u32		bytes;
	u32		sectors;
	u64		entry_lba;
	u64		alt_entry_lba;
	u64		alt_lba;

	if (!gpt_pending.active || gpt_pending.disk != disk) {
		return (-1);
	}
	if (gpt_pending.count == 0) {
		return (-1);
	}
	bytes = GPT_ENTRY_COUNT * GPT_ENTRY_SIZE;
	sectors = (bytes + disk->sector_size - 1) / disk->sector_size;
	entry_lba = GPT_ENTRY_LBA;
	alt_lba = disk->total_sectors - 1;
	alt_entry_lba = alt_lba - sectors;

	buf = kmem_alloc(sectors * disk->sector_size);
	if (buf == NULL) {
		return (-1);
	}
	memset(buf, 0, sectors * disk->sector_size);
	memcpy(buf, gpt_pending.entries, sizeof(gpt_pending.entries));

	memset(&hdr, 0, sizeof(hdr));
	hdr.signature = GPT_SIGNATURE;
	hdr.revision = GPT_REVISION;
	hdr.header_size = 92;
	hdr.my_lba = GPT_HEADER_LBA;
	hdr.alternate_lba = alt_lba;
	hdr.first_usable_lba = gpt_pending.first_usable;
	hdr.last_usable_lba = gpt_pending.last_usable;
	if (gpt_guid_fill(hdr.disk_guid) != 0) {
		kmem_free(buf);
		return (-1);
	}
	hdr.entry_lba = entry_lba;
	hdr.entry_count = GPT_ENTRY_COUNT;
	hdr.entry_size = GPT_ENTRY_SIZE;
	hdr.entry_crc32 = gpt_crc32(buf, bytes);

	if (gpt_write_protective_mbr(disk) != 0) {
		kmem_free(buf);
		return (-1);
	}
	if (bio_write(disk, entry_lba, sectors, buf) != BIO_STATUS_OK) {
		kmem_free(buf);
		return (-1);
	}
	if (gpt_write_header(disk, &hdr, GPT_HEADER_LBA) != 0) {
		kmem_free(buf);
		return (-1);
	}

	alt = hdr;
	alt.my_lba = alt_lba;
	alt.alternate_lba = GPT_HEADER_LBA;
	alt.entry_lba = alt_entry_lba;
	if (bio_write(disk, alt_entry_lba, sectors, buf) != BIO_STATUS_OK) {
		kmem_free(buf);
		return (-1);
	}
	if (gpt_write_header(disk, &alt, alt_lba) != 0) {
		kmem_free(buf);
		return (-1);
	}
	kmem_free(buf);
	if (bio_flush(disk) != BIO_STATUS_OK &&
	    (disk->flags & DISK_F_NO_FLUSH) == 0) {
		return (-1);
	}
	gpt_pending.active = 0;
	return (gpt_rescan(disk));
}
