#include "vmm.h"
#include "pmm.h"
#include "console.h" // Include your new module
#include "kstring.h"

// Indices for the 4 levels of paging
#define PML4_IDX(addr) (((addr) >> 39) & 0x1FF)
#define PDPT_IDX(addr) (((addr) >> 30) & 0x1FF)
#define PD_IDX(addr)   (((addr) >> 21) & 0x1FF)
#define PT_IDX(addr)   (((addr) >> 12) & 0x1FF)

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITE   (1ULL << 1)

// A generic page table entry
typedef uint64_t pt_entry_t;

// Our main Kernel Directory
static pt_entry_t* kernel_pml4 = NULL;

void vmm_load_pml4(pt_entry_t* pml4) {
    __asm__ volatile("mov %0, %%cr3" : : "r"((uint64_t)pml4));
}

void vmm_init() {
    // 1. Allocate a page for the new Top-Level Directory (PML4)
    kernel_pml4 = (pt_entry_t*)pmm_alloc_page();
    kmemset(kernel_pml4, 0, 4096);

    // 2. Identity Map the first 4GB of physical memory
    // (So your kernel code, video memory, and stack keep working)
    // This is a simplified "Huge Page" (2MB) identity map strategy
    
    // Use PMM to allocate PDP, PD, etc. (This logic requires the next step: map_page)
    
    // CRITICAL: For now, just printing that we are ready.
    // Switching CR3 without a perfect map will Triple Fault instantly.
    console_print("VMM: PML4 Allocated at 0x");
    // (print address)
}

// Map a virtual address to a physical address
void vmm_map_page(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    // Level 4 -> Level 3 (PDPT)
    if (!(pml4[PML4_IDX(virt)] & PAGE_PRESENT)) {
        uint64_t new_table = pmm_alloc_page();
        kmemset((void*)new_table, 0, 4096);
        pml4[PML4_IDX(virt)] = new_table | PAGE_PRESENT | flags;
    }
    uint64_t* pdpt = (uint64_t*)(pml4[PML4_IDX(virt)] & ~0xFFFULL);

    // Level 3 -> Level 2 (Page Directory)
    if (!(pdpt[PDPT_IDX(virt)] & PAGE_PRESENT)) {
        uint64_t new_table = pmm_alloc_page();
        kmemset((void*)new_table, 0, 4096);
        pdpt[PDPT_IDX(virt)] = new_table | PAGE_PRESENT | flags;
    }
    uint64_t* pd = (uint64_t*)(pdpt[PDPT_IDX(virt)] & ~0xFFFULL);

    // Level 2 -> Level 1 (Page Table)
    if (!(pd[PD_IDX(virt)] & PAGE_PRESENT)) {
        uint64_t new_table = pmm_alloc_page();
        kmemset((void*)new_table, 0, 4096);
        pd[PD_IDX(virt)] = new_table | PAGE_PRESENT | flags;
    }
    uint64_t* pt = (uint64_t*)(pd[PD_IDX(virt)] & ~0xFFFULL);

    // Level 1: Map the actual physical page
    pt[PT_IDX(virt)] = phys | PAGE_PRESENT | flags;

    // Flush the TLB for this address
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

uint64_t* vmm_create_kernel_pml4() {
    uint64_t* pml4 = (uint64_t*)pmm_alloc_page();
    kmemset(pml4, 0, 4096);

    // 1. Identity Map the first 4GB (instead of 512MB)
    // This is safer for UEFI environments where memory is scattered.
    // Use 2MB pages or just a loop of 4KB pages for now.
    for (uint64_t addr = 0; addr < 0x100000000ULL; addr += 4096) {
        // We pass PAGE_WRITE (1ULL << 1) | PAGE_PRESENT (1ULL << 0)
        vmm_map_page(pml4, addr, addr, 0x3); 
    }

    // 2. Explicitly map the Framebuffer
    extern uint32_t* fb_base;
    extern uint32_t ScreenHeight, PixelsPerScanLine;
    uint64_t fb_phys = (uint64_t)fb_base;
    uint64_t fb_size = (uint64_t)ScreenHeight * PixelsPerScanLine * 4;
    
    // Ensure the FB is mapped even if it's above 4GB
    for (uint64_t addr = 0; addr < fb_size; addr += 4096) {
        vmm_map_page(pml4, fb_phys + addr, fb_phys + addr, 0x3);
    }

    return pml4;
}
