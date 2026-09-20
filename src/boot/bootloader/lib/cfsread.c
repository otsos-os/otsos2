#include <boot/bootloader/lib/cfsread.h>
#include <boot/bootloader/lib/string.h>

static int
cfsr_sectors(cfsr_volume_t *vol, u32 block, u32 count, void *dst)
{
	u32	chunk, limit;
	u8	*out;

	out = (u8 *)dst;
	limit = vol->max_run;
	if (limit == 0) {
		limit = 1;
	}
	while (count > 0) {
		chunk = count > limit ? limit : count;
		if (vol->read(vol->ctx, vol->base_lba + block, chunk,
		    out) != 0) {
			return (-1);
		}
		out += chunk * CFSR_BLOCK_SIZE;
		block += chunk;
		count -= chunk;
	}
	return (0);
}

static int
cfsr_sector(cfsr_volume_t *vol, u32 block, void *dst)
{
	return (vol->read(vol->ctx, vol->base_lba + block, 1, dst));
}

static int
cfsr_map_load(cfsr_volume_t *vol, u32 map_block)
{
	u32	sector;

	if (map_block >= vol->block_map_blocks) {
		return (-1);
	}
	if (vol->map_cached == map_block) {
		return (0);
	}
	sector = 1 + vol->file_table_blocks + map_block;
	if (cfsr_sector(vol, sector, vol->map) != 0) {
		vol->map_cached = CFSR_MAP_NONE;
		return (-1);
	}
	vol->map_cached = map_block;
	return (0);
}

static int
cfsr_next_block(cfsr_volume_t *vol, u32 block, u32 *next)
{
	const u32	*entries;
	u32		per_block, map_block, map_offset;

	per_block = CFSR_BLOCK_SIZE / sizeof(u32);
	map_block = block / per_block;
	map_offset = block % per_block;
	if (cfsr_map_load(vol, map_block) != 0) {
		return (-1);
	}
	entries = (const u32 *)vol->map;
	*next = entries[map_offset];
	return (0);
}

int
cfsr_probe(const void *sector)
{
	const cfsr_superblock_t	*sb;

	if (sector == NULL) {
		return (-1);
	}
	sb = (const cfsr_superblock_t *)sector;
	if (sb->magic != CFSR_MAGIC) {
		return (-1);
	}
	if (sb->block_count < 8 || sb->file_table_block_count == 0 ||
	    sb->block_map_block_count == 0) {
		return (-1);
	}
	if (1U + sb->file_table_block_count + sb->block_map_block_count >=
	    sb->block_count) {
		return (-1);
	}
	return (0);
}

int
cfsr_mount(cfsr_volume_t *vol, cfsr_read_fn read, void *ctx, u64 base_lba,
    u32 max_run)
{
	const cfsr_superblock_t	*sb;

	if (vol == NULL || read == NULL) {
		return (-1);
	}
	bl_memset(vol, 0, sizeof(*vol));
	vol->read = read;
	vol->ctx = ctx;
	vol->base_lba = base_lba;
	vol->max_run = max_run != 0 ? max_run : 1;
	vol->map_cached = CFSR_MAP_NONE;

	if (cfsr_sector(vol, 0, vol->sector) != 0) {
		return (-1);
	}
	if (cfsr_probe(vol->sector) != 0) {
		return (-1);
	}
	sb = (const cfsr_superblock_t *)vol->sector;
	vol->block_count = sb->block_count;
	vol->file_table_blocks = sb->file_table_block_count;
	vol->block_map_blocks = sb->block_map_block_count;
	vol->root_dir_block = sb->root_dir_block;
	vol->data_area_start = 1 + vol->file_table_blocks +
	    vol->block_map_blocks;
	return (0);
}

static int
cfsr_name_eq(const char *name, const char *comp, u32 len)
{
	u32	i;

	if (len == 0 || len > CFSR_NAME_MAX) {
		return (0);
	}
	for (i = 0; i < len; i++) {
		if (name[i] != comp[i] || name[i] == '\0') {
			return (0);
		}
	}
	return (i == CFSR_NAME_MAX || name[i] == '\0');
}

static int
cfsr_find_in_dir(cfsr_volume_t *vol, u32 dir_block, const char *comp,
    u32 len, cfsr_entry_t *out, u32 *out_index)
{
	const cfsr_entry_t	*entries;
	u32			per_block, block, i;

	per_block = CFSR_BLOCK_SIZE / sizeof(cfsr_entry_t);
	for (block = 1; block < 1 + vol->file_table_blocks; block++) {
		if (cfsr_sector(vol, block, vol->sector) != 0) {
			return (-1);
		}
		entries = (const cfsr_entry_t *)vol->sector;
		for (i = 0; i < per_block; i++) {
			if (entries[i].status != 1) {
				continue;
			}
			if (entries[i].parent_block != dir_block) {
				continue;
			}
			if (!cfsr_name_eq(entries[i].name, comp, len)) {
				continue;
			}
			bl_memcpy(out, &entries[i], sizeof(*out));
			*out_index = (block - 1) * per_block + i;
			return (0);
		}
	}
	return (-1);
}

static int
cfsr_entry_by_index(cfsr_volume_t *vol, u32 index, cfsr_entry_t *out)
{
	const cfsr_entry_t	*entries;
	u32			per_block, block, offset;

	per_block = CFSR_BLOCK_SIZE / sizeof(cfsr_entry_t);
	block = 1 + index / per_block;
	offset = index % per_block;
	if (block >= 1 + vol->file_table_blocks) {
		return (-1);
	}
	if (cfsr_sector(vol, block, vol->sector) != 0) {
		return (-1);
	}
	entries = (const cfsr_entry_t *)vol->sector;
	bl_memcpy(out, &entries[offset], sizeof(*out));
	return (0);
}

static int
cfsr_walk(cfsr_volume_t *vol, const char *path, cfsr_entry_t *out,
    u32 *out_index)
{
	cfsr_entry_t	entry;
	const char	*p;
	u32		dir_block, len, index, depth;

	if (path == NULL || out == NULL) {
		return (-1);
	}
	p = path;
	while (*p == '/') {
		p++;
	}
	dir_block = vol->root_dir_block;
	index = 0;
	if (*p == '\0') {
		if (cfsr_entry_by_index(vol, dir_block, &entry) != 0) {
			return (-1);
		}
		bl_memcpy(out, &entry, sizeof(*out));
		if (out_index != NULL) {
			*out_index = dir_block;
		}
		return (0);
	}

	depth = 0;
	while (*p != '\0') {
		if (++depth > CFSR_MAX_DEPTH) {
			return (-1);
		}
		len = 0;
		while (p[len] != '\0' && p[len] != '/') {
			len++;
		}
		if (cfsr_find_in_dir(vol, dir_block, p, len, &entry,
		    &index) != 0) {
			return (-1);
		}
		p += len;
		while (*p == '/') {
			p++;
		}
		if (*p == '\0') {
			bl_memcpy(out, &entry, sizeof(*out));
			if (out_index != NULL) {
				*out_index = index;
			}
			return (0);
		}
		if (entry.type != CFSR_TYPE_DIR) {
			return (-1);
		}
		dir_block = index;
	}
	return (-1);
}

int
cfsr_lookup(cfsr_volume_t *vol, const char *path, cfsr_entry_t *out)
{
	if (vol == NULL) {
		return (-1);
	}
	return (cfsr_walk(vol, path, out, NULL));
}

int
cfsr_read(cfsr_volume_t *vol, const cfsr_entry_t *entry, void *dst, u32 limit,
    u32 *out_size)
{
	u8	*out;
	u32	remaining, copied, block, run, last, next, full, needed;

	if (vol == NULL || entry == NULL || dst == NULL) {
		return (-1);
	}
	if (entry->type == CFSR_TYPE_DIR) {
		return (-1);
	}
	remaining = entry->size;
	if (remaining > limit) {
		return (-1);
	}
	out = (u8 *)dst;
	copied = 0;
	block = entry->start_block;

	while (remaining > 0 && block != CFSR_EOF_MARKER) {
		needed = (remaining + CFSR_BLOCK_SIZE - 1) /
		    CFSR_BLOCK_SIZE;
		run = 1;
		last = block;
		while (run < needed) {
			if (cfsr_next_block(vol, last, &next) != 0) {
				return (-1);
			}
			if (next == CFSR_EOF_MARKER || next != last + 1) {
				break;
			}
			last = next;
			run++;
		}

		full = remaining / CFSR_BLOCK_SIZE;
		if (full > run) {
			full = run;
		}
		if (full > 0) {
			if (cfsr_sectors(vol, vol->data_area_start + block,
			    full, out + copied) != 0) {
				return (-1);
			}
			copied += full * CFSR_BLOCK_SIZE;
			remaining -= full * CFSR_BLOCK_SIZE;
		}
		if (full < run && remaining > 0) {
			if (cfsr_sector(vol, vol->data_area_start + block +
			    full, vol->sector) != 0) {
				return (-1);
			}
			bl_memcpy(out + copied, vol->sector, remaining);
			copied += remaining;
			remaining = 0;
			full++;
		}
		if (remaining == 0) {
			break;
		}
		if (cfsr_next_block(vol, block + full - 1, &next) != 0) {
			return (-1);
		}
		block = next;
	}

	if (out_size != NULL) {
		*out_size = copied;
	}
	return (copied == entry->size ? 0 : -1);
}

int
cfsr_dir_entry(cfsr_volume_t *vol, const char *path, u32 index,
    cfsr_entry_t *out)
{
	const cfsr_entry_t	*entries;
	cfsr_entry_t		dir;
	u32			dir_block, per_block, block, i, seen;

	if (vol == NULL || out == NULL) {
		return (-1);
	}
	if (cfsr_walk(vol, path, &dir, &dir_block) != 0) {
		return (-1);
	}
	if (dir.type != CFSR_TYPE_DIR) {
		return (-1);
	}

	per_block = CFSR_BLOCK_SIZE / sizeof(cfsr_entry_t);
	seen = 0;
	for (block = 1; block < 1 + vol->file_table_blocks; block++) {
		if (cfsr_sector(vol, block, vol->sector) != 0) {
			return (-1);
		}
		entries = (const cfsr_entry_t *)vol->sector;
		for (i = 0; i < per_block; i++) {
			if (entries[i].status != 1) {
				continue;
			}
			if (entries[i].parent_block != dir_block) {
				continue;
			}
			if (seen == index) {
				bl_memcpy(out, &entries[i], sizeof(*out));
				return (0);
			}
			seen++;
		}
	}
	return (-1);
}
