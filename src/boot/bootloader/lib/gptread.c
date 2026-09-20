#include <boot/bootloader/lib/gptread.h>
#include <boot/bootloader/lib/string.h>

#define GPTR_SIGNATURE		0x5452415020494645ULL
#define GPTR_ENTRIES_PER_SECTOR	(GPTR_SECTOR_SIZE / GPTR_ENTRY_SIZE)

const u8 gptr_type_esp[16] = {
	0x28, 0x73, 0x2a, 0xc1, 0x1f, 0xf8, 0xd2, 0x11,
	0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b
};

const u8 gptr_type_bios[16] = {
	0x48, 0x61, 0x68, 0x21, 0x49, 0x64, 0x6f, 0x6e,
	0x74, 0x4e, 0x65, 0x65, 0x64, 0x45, 0x46, 0x49
};

const u8 gptr_type_otsos[16] = {
	0x4f, 0x54, 0x53, 0x4f, 0x53, 0x32, 0x00, 0x11,
	0x9c, 0xf5, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b
};

static int
gptr_guid_zero(const u8 *guid)
{
	u32	i;

	for (i = 0; i < 16; i++) {
		if (guid[i] != 0) {
			return (0);
		}
	}
	return (1);
}

int
gptr_find(gptr_read_fn read, void *ctx, const u8 *type_guid, gptr_part_t *out)
{
	u8			sector[GPTR_SECTOR_SIZE];
	const gptr_header_t	*hdr;
	const gptr_entry_t	*entry;
	u64			entry_lba;
	u32			count, per_sector, i, slot;

	if (read == NULL || type_guid == NULL || out == NULL) {
		return (-1);
	}
	if (read(ctx, GPTR_HEADER_LBA, 1, sector) != 0) {
		return (-1);
	}
	hdr = (const gptr_header_t *)sector;
	if (hdr->signature != GPTR_SIGNATURE) {
		return (-1);
	}
	if (hdr->entry_size != GPTR_ENTRY_SIZE || hdr->entry_count == 0) {
		return (-1);
	}
	count = hdr->entry_count;
	if (count > GPTR_MAX_ENTRIES) {
		count = GPTR_MAX_ENTRIES;
	}
	entry_lba = hdr->entry_lba;
	per_sector = GPTR_ENTRIES_PER_SECTOR;

	for (i = 0; i < count; i++) {
		if (i % per_sector == 0) {
			if (read(ctx, entry_lba + i / per_sector, 1,
			    sector) != 0) {
				return (-1);
			}
		}
		slot = i % per_sector;
		entry = (const gptr_entry_t *)(sector +
		    slot * GPTR_ENTRY_SIZE);
		if (gptr_guid_zero(entry->type_guid)) {
			continue;
		}
		if (bl_memcmp(entry->type_guid, type_guid, 16) != 0) {
			continue;
		}
		if (entry->first_lba == 0 ||
		    entry->last_lba < entry->first_lba) {
			continue;
		}
		out->first_lba = entry->first_lba;
		out->last_lba = entry->last_lba;
		out->index = i;
		return (0);
	}
	return (-1);
}
