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
