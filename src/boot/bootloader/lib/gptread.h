#ifndef BOOTLOADER_GPTREAD_H
#define BOOTLOADER_GPTREAD_H

#include <boot/bootloader/lib/types.h>

#define GPTR_SECTOR_SIZE	512U
#define GPTR_HEADER_LBA		1U
#define GPTR_MAX_ENTRIES	128U
#define GPTR_ENTRY_SIZE		128U

typedef int	(*gptr_read_fn)(void *ctx, u64 lba, u32 count, void *dst);

typedef struct {
	u64	signature;
	u32	revision;
	u32	header_size;
	u32	header_crc;
	u32	reserved;
	u64	current_lba;
	u64	backup_lba;
	u64	first_usable_lba;
	u64	last_usable_lba;
	u8	disk_guid[16];
	u64	entry_lba;
	u32	entry_count;
	u32	entry_size;
	u32	entry_crc;
} __attribute__((packed)) gptr_header_t;

typedef struct {
	u8	type_guid[16];
	u8	part_guid[16];
	u64	first_lba;
	u64	last_lba;
	u64	attributes;
	u16	name[36];
} __attribute__((packed)) gptr_entry_t;

typedef struct {
	u64	first_lba;
	u64	last_lba;
	u32	index;
} gptr_part_t;

extern const u8	gptr_type_otsos[16];
extern const u8	gptr_type_esp[16];
extern const u8	gptr_type_bios[16];

int	gptr_find(gptr_read_fn read, void *ctx, const u8 *type_guid,
	    gptr_part_t *out);

#endif
