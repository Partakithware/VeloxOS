#ifndef MEMORY_H
#define MEMORY_H

#include <efi.h>
#include <efilib.h>

// Add this global flag
extern BOOLEAN uefi_exited;

// Allocate 'size' bytes of memory
void* kmalloc(UINTN size);

// Free previously allocated memory
void kfree(void* ptr);

void* my_realloc(void* ptr, UINTN old_size, UINTN new_size);

#endif
