#include "ramdisk.h"
#include "kstring.h"
#include "memory.h"
#include "console.h"

#define SECTOR_SIZE    512
#define RAM_DISK_SECTORS 16384          // 8MB: plenty to test VeloxFS
#define RAM_DISK_SIZE  (RAM_DISK_SECTORS * SECTOR_SIZE)

static uint8_t* ram_disk_buffer = NULL;

void ram_disk_init(void) {
    ram_disk_buffer = (uint8_t*)kmalloc(RAM_DISK_SIZE);
    if (!ram_disk_buffer) {
        console_print("CRITICAL: Failed to allocate RAM disk!\n");
        return;
    }
    kmemset(ram_disk_buffer, 0, RAM_DISK_SIZE);  // use YOUR memset, not libc
    console_print("RAM Disk initialized (8MB)\n");
}

void ram_disk_read(uint64_t lba, uint32_t count, void* buffer) {
    if (!ram_disk_buffer) return;
    // Bounds check: clamp reads to actual disk size
    if (lba >= RAM_DISK_SECTORS) return;
    if (lba + count > RAM_DISK_SECTORS) count = RAM_DISK_SECTORS - lba;

    uint8_t* src = ram_disk_buffer + (lba * SECTOR_SIZE);
    kmemcpy(buffer, src, count * SECTOR_SIZE);
}

void ram_disk_write(uint64_t lba, uint32_t count, const void* buffer) {
    if (!ram_disk_buffer) return;
    // Bounds check
    if (lba >= RAM_DISK_SECTORS) return;
    if (lba + count > RAM_DISK_SECTORS) count = RAM_DISK_SECTORS - lba;

    uint8_t* dest = ram_disk_buffer + (lba * SECTOR_SIZE);
    kmemcpy(dest, buffer, count * SECTOR_SIZE);
}