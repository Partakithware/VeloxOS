#include <stdint.h>
#include <stddef.h>
#include "console.h"

#include <efi.h>
#include "pmm.h"

volatile uint64_t total_pages;
volatile uint64_t free_pages;
static uint8_t* bitmap;
static uint64_t last_index = 0;

#define PAGE_SIZE 4096

void pmm_set_page(uint64_t page_addr) {
    uint64_t index = page_addr / PAGE_SIZE;
    bitmap[index / 8] |= (1 << (index % 8));
}

void pmm_free_page(uint64_t page_addr) {
    uint64_t index = page_addr / PAGE_SIZE;
    bitmap[index / 8] &= ~(1 << (index % 8));
}

// NEW: Check if a page is free
int pmm_is_page_free(uint64_t page_addr) {
    uint64_t index = page_addr / PAGE_SIZE;
    return !(bitmap[index / 8] & (1 << (index % 8)));
}

// NEW: Allocate a single physical page
uint64_t pmm_alloc_page(void) {
    // 1. Search from where we last found a free page to the end
    for (uint64_t i = last_index; i < total_pages; i++) {
        if (pmm_is_page_free(i * PAGE_SIZE)) {
            pmm_set_page(i * PAGE_SIZE);
            free_pages--;
            last_index = i; 
            return i * PAGE_SIZE;
        }
    }

    // 2. If we hit the end without finding anything, wrap back to the beginning
    // and search up to the original last_index
    for (uint64_t i = 0; i < last_index; i++) {
        if (pmm_is_page_free(i * PAGE_SIZE)) {
            pmm_set_page(i * PAGE_SIZE);
            free_pages--;
            last_index = i;
            return i * PAGE_SIZE;
        }
    }

    return 0; // Truly out of memory
}

// NEW: Allocate multiple contiguous pages with Next-Fit optimization
uint64_t pmm_alloc_pages(uint64_t count) {
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc_page();

    // 1. Search from last_index to the end
    for (uint64_t i = last_index; i <= total_pages - count; i++) {
        int found = 1;
        for (uint64_t j = 0; j < count; j++) {
            if (!pmm_is_page_free((i + j) * PAGE_SIZE)) {
                found = 0;
                i += j; // Optimization: Skip ahead to the page that was busy
                break;
            }
        }

        if (found) {
            for (uint64_t j = 0; j < count; j++) pmm_set_page((i + j) * PAGE_SIZE);
            free_pages -= count;
            last_index = i + count; // Update hint
            return i * PAGE_SIZE;
        }
    }

    // 2. Wrap around and search from the beginning to last_index
    for (uint64_t i = 0; i < last_index; i++) {
        int found = 1;
        for (uint64_t j = 0; j < count; j++) {
            if (!pmm_is_page_free((i + j) * PAGE_SIZE)) {
                found = 0;
                i += j;
                break;
            }
        }

        if (found) {
            for (uint64_t j = 0; j < count; j++) pmm_set_page((i + j) * PAGE_SIZE);
            free_pages -= count;
            last_index = i + count;
            return i * PAGE_SIZE;
        }
    }

    return 0; // Out of contiguous memory
}

// NEW: Free a single page and update the allocation hint
void pmm_dealloc_page(uint64_t page_addr) {
    if (pmm_is_page_free(page_addr)) return; // Don't free already free memory
    
    uint64_t index = page_addr / PAGE_SIZE;
    pmm_free_page(page_addr);
    free_pages++;

    // Move the hint back so the next allocation starts at the earliest hole
    if (index < last_index) {
        last_index = index;
    }
}

// NEW: Free multiple contiguous pages
void pmm_dealloc_pages(uint64_t page_addr, uint64_t count) {
    if (count == 0) return;

    // Calculate the start index once
    uint64_t start_index = page_addr / PAGE_SIZE;

    for (uint64_t i = 0; i < count; i++) {
        uint64_t addr = page_addr + (i * PAGE_SIZE);
        if (!pmm_is_page_free(addr)) {
            pmm_free_page(addr);
            free_pages++;
        }
    }

    // Move the hint back to the start of the entire freed block
    if (start_index < last_index) {
        last_index = start_index;
    }
}

void pmm_init(EFI_MEMORY_DESCRIPTOR* map, uint64_t map_size, uint64_t desc_size) {
    if (map == NULL || map_size == 0) return;

    uint64_t entries = map_size / desc_size;
    uint64_t max_addr = 0;

    // 1. Calculate max RAM 
    for (uint64_t i = 0; i < entries; i++) {
        EFI_MEMORY_DESCRIPTOR* d = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)map + (i * desc_size));
        uint64_t end = d->PhysicalStart + (d->NumberOfPages * 4096);
        if (end > max_addr) max_addr = end;
    }

    total_pages = 0;
    for (uint64_t i = 0; i < entries; i++) {
        EFI_MEMORY_DESCRIPTOR* d = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)map + (i * desc_size));
        
        if (d->Type == EfiConventionalMemory || 
            d->Type == EfiBootServicesData || 
            d->Type == EfiBootServicesCode ||
            d->Type == EfiLoaderCode ||
            d->Type == EfiLoaderData) {
            total_pages += d->NumberOfPages;
        }
    }
    
    uint64_t bitmap_size = (total_pages + 7) / 8;

    // 2. Find bitmap location
    bitmap = NULL;
    for (uint64_t i = 0; i < entries; i++) {
        EFI_MEMORY_DESCRIPTOR* d = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)map + (i * desc_size));
        if (d->Type == EfiConventionalMemory && d->PhysicalStart >= 0x1000000) {
            if ((d->NumberOfPages * 4096) >= bitmap_size) {
                bitmap = (uint8_t*)d->PhysicalStart;
                break;
            }
        }
    }

    if (!bitmap) return; 

    // 3. Mark everything as USED
    for (uint64_t i = 0; i < bitmap_size; i++) bitmap[i] = 0xFF;

    // 4. Free conventional memory
    free_pages = 0;
    for (uint64_t i = 0; i < entries; i++) {
        EFI_MEMORY_DESCRIPTOR* d = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)map + (i * desc_size));
        if (d->Type == EfiConventionalMemory) {
            for (uint64_t j = 0; j < d->NumberOfPages; j++) {
                pmm_free_page(d->PhysicalStart + (j * 4096));
                free_pages++;
            }
        }
    }

    // 5. Lock bitmap
    for (uint64_t i = 0; i < (bitmap_size + 4095) / 4096; i++) {
        pmm_set_page((uint64_t)bitmap + (i * 4096));
        free_pages--;
    }
}

uint64_t pmm_get_total_pages() {
    return total_pages;
}

uint64_t pmm_get_free_pages() {
    return free_pages;
}