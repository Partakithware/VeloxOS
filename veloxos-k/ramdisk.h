#ifndef RAMDISK_H
#define RAMDISK_H

#include <stdint.h>

void ram_disk_init(void);
void ram_disk_read(uint64_t lba, uint32_t count, void* buffer);
void ram_disk_write(uint64_t lba, uint32_t count, const void* buffer);

#endif