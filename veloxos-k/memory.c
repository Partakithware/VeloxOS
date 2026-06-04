#include "memory.h"
#include "pmm.h"

BOOLEAN uefi_exited = FALSE;

typedef struct heap_node {
    uint64_t size;           // Size of the data area (excluding header)
    int free;                // 1 if available, 0 if in use
    struct heap_node* next;  // Pointer to the next block in the chain
} heap_node_t;

#define HEADER_SIZE sizeof(heap_node_t)

// The start of our managed heap
static heap_node_t* heap_head = NULL;

void* kmalloc(UINTN size) {
    if (!uefi_exited) {
        void* ptr = NULL;
        uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, size, &ptr);
        return ptr;
    }

    if (size == 0) return NULL;

    // 8-byte alignment
    size = (size + 7) & ~7;

    heap_node_t* current = heap_head;
    heap_node_t* last = NULL;

    // 1. Search the list for an existing free block
    while (current != NULL) {
        if (current->free && current->size >= size) {
            current->free = 0;
            // Success: Return the pointer just after the header
            return (void*)((uint8_t*)current + HEADER_SIZE);
        }
        last = current;
        current = current->next;
    }

    // 2. No block found? Expand the heap
    uint64_t total_needed = size + HEADER_SIZE;
    uint64_t pages_needed = (total_needed + 4095) / 4096;
    uint64_t phys_mem = pmm_alloc_pages(pages_needed);
    
    if (phys_mem == 0) return NULL; // Out of RAM

    heap_node_t* new_node = (heap_node_t*)phys_mem;
    new_node->size = (pages_needed * 4096) - HEADER_SIZE;
    new_node->free = 0;
    new_node->next = NULL;

    // Link it into the list
    if (last) {
        last->next = new_node;
    } else {
        heap_head = new_node;
    }

    return (void*)((uint8_t*)new_node + HEADER_SIZE);
}

void kfree(void* ptr) {
    if (ptr == NULL) return;

    if (!uefi_exited) {
        uefi_call_wrapper(BS->FreePool, 1, ptr);
        return;
    }

    // Move the pointer back to find the header
    heap_node_t* node = (heap_node_t*)((uint8_t*)ptr - HEADER_SIZE);
    
    // Mark as free so kmalloc can reuse it next time
    node->free = 1;
    
    // Note: We don't call pmm_dealloc_pages here. 
    // We keep the memory in our "Kernel Heap" pool for faster reuse.
}

void* my_realloc(void* ptr, UINTN old_size, UINTN new_size) {
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }
    if (ptr == NULL) return kmalloc(new_size);

    void* new_ptr = kmalloc(new_size);
    if (new_ptr) {
        uint8_t* src = (uint8_t*)ptr;
        uint8_t* dest = (uint8_t*)new_ptr;
        uint64_t copy_size = (old_size < new_size ? old_size : new_size);
        
        for (uint64_t i = 0; i < copy_size; i++) {
            dest[i] = src[i];
        }
        kfree(ptr);
    }
    return new_ptr;
}