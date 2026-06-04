#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <efi.h>

// Initialize the PMM using the UEFI Memory Map
void pmm_init(EFI_MEMORY_DESCRIPTOR* map, uint64_t map_size, uint64_t desc_size);

// Status functions
uint64_t pmm_get_total_pages();
uint64_t pmm_get_free_pages();

// Page allocation functions
uint64_t pmm_alloc_page(void);           // Allocate 1 page (4KB)
uint64_t pmm_alloc_pages(uint64_t count); // Allocate N contiguous pages
void pmm_dealloc_page(uint64_t page_addr);
void pmm_dealloc_pages(uint64_t page_addr, uint64_t count);

// Internal helper functions
void pmm_set_page(uint64_t page_addr);
void pmm_free_page(uint64_t page_addr);

#endif