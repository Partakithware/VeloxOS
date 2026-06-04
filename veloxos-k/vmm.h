#ifndef VMM_H
#define VMM_H

#include <stdint.h>

// Page Table Entry Flags
#define PAGE_PRESENT    (1ULL << 0)   // Page is in RAM
#define PAGE_WRITE      (1ULL << 1)   // If set, page is read/write. If clear, read-only
#define PAGE_USER       (1ULL << 2)   // If set, User-mode can access. If clear, Kernel only
#define PAGE_PWT        (1ULL << 3)   // Page-level write-through
#define PAGE_PCD        (1ULL << 4)   // Page-level cache disable
#define PAGE_ACCESSED   (1ULL << 5)   // Set by CPU when page is read
#define PAGE_DIRTY      (1ULL << 6)   // Set by CPU when page is written to
#define PAGE_HUGE       (1ULL << 7)   // Used in PDE/PDPTE for 2MB/1GB pages
#define PAGE_GLOBAL     (1ULL << 8)   // Prevents TLB flush on CR3 switch
#define PAGE_NX         (1ULL << 63)  // No-Execute bit (requires EFER.NXE)

// Function Prototypes
void vmm_init(void);
void vmm_load_pml4(uint64_t* pml4);
void vmm_map_page(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags);
uint64_t* vmm_create_kernel_pml4(void);

#endif
