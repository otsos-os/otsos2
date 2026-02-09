#ifndef KERNEL_DRIVERS_DISK_DISK_H
#define KERNEL_DRIVERS_DISK_DISK_H

#include <mlibc/mlibc.h>

typedef enum {
  DISK_TYPE_PATA,
  DISK_TYPE_NVME,
  DISK_TYPE_RAM,
  DISK_TYPE_UNKNOWN
} disk_type_t;

typedef struct disk {
  char name[32];
  disk_type_t type;
  u32 sector_size;
  u32 total_sectors;
  void *private_data;

  void (*read_sector)(struct disk *self, u32 lba, u8 *buffer);
  void (*write_sector)(struct disk *self, u32 lba, u8 *buffer);
} disk_t;

void disk_manager_init(void);
int disk_manager_is_initialized(void);
int disk_register(disk_t *disk);
disk_t *disk_get(int index);
int disk_count(void);

void disk_read(disk_t *disk, u32 lba, u8 *buffer);
void disk_write(disk_t *disk, u32 lba, u8 *buffer);

#endif
