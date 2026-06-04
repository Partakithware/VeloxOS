// kernel_native.c - Lean Kernel (Direct Framebuffer, No UI Garbage)
#include <efi.h>
#include <efilib.h>
#include "idt.h"
#include "font.h"
#include "mouse_k.h"
#include "keyboard_k.h"
#include "event.h"
#include "console.h" // Include your new module
#include "commands.h" // Add this at the top
#include "kstdlib.h"
#include "font_ttf.h"
#include "pmm.h"
#include "double_buffer.h"
#include "vmm.h"

// Hardware Globals
UINT32 ScreenWidth = 0;
UINT32 ScreenHeight = 0;
UINT32 PixelsPerScanLine = 0;
UINT32* fb_base = NULL; 

EFI_MEMORY_DESCRIPTOR* global_mmap;
UINTN global_mmap_size;
UINTN global_desc_size;


    UINT32* saved_fb;
    UINT32 saved_width;
    UINT32 saved_height;
    UINT32 saved_pitch;

extern BOOLEAN uefi_exited;



// Text/Cursor State
//UINT32 CursorX = 20;
//UINT32 CursorY = 50;

// Initialize PIC (Hardware Essential)
// Initialize PIC (Programmable Interrupt Controller)
void pic_init(void) {
    // ICW1: Initialize
    __asm__ volatile ("outb %0, $0x20" : : "a"((UINT8)0x11));
    __asm__ volatile ("outb %0, $0xA0" : : "a"((UINT8)0x11));
    
    // ICW2: Remap IRQs to 32-47
    __asm__ volatile ("outb %0, $0x21" : : "a"((UINT8)0x20)); // Master PIC offset
    __asm__ volatile ("outb %0, $0xA1" : : "a"((UINT8)0x28)); // Slave PIC offset
    
    // ICW3: Tell master about slave
    __asm__ volatile ("outb %0, $0x21" : : "a"((UINT8)0x04));
    __asm__ volatile ("outb %0, $0xA1" : : "a"((UINT8)0x02));
    
    // ICW4: 8086 mode
    __asm__ volatile ("outb %0, $0x21" : : "a"((UINT8)0x01));
    __asm__ volatile ("outb %0, $0xA1" : : "a"((UINT8)0x01));
    
    // Unmask all IRQs
    __asm__ volatile ("outb %0, $0x21" : : "a"((UINT8)0x00));
    __asm__ volatile ("outb %0, $0xA1" : : "a"((UINT8)0x00));
}

// Initialize Programmable Interval Timer (PIT) to 100Hz
void pit_init(void) {
    UINT16 divisor = 1193180 / 100;   // 1.19MHz / 100Hz
    
    // Command port 0x43: Channel 0, Access lo/hi byte, Mode 2 (rate generator)
    __asm__ volatile ("outb %0, $0x43" : : "a"((UINT8)0x36));
    
    // Data port 0x40: Send low byte
    __asm__ volatile ("outb %0, $0x40" : : "a"((UINT8)(divisor & 0xFF)));
    
    // Data port 0x40: Send high byte
    __asm__ volatile ("outb %0, $0x40" : : "a"((UINT8)((divisor >> 8) & 0xFF)));
}

// DIRECT TO SCREEN PRINTING (Replaces backbuffer logic)
/*void put_char(int x, int y, char c, UINT32 foreground, UINT32 background) {
    if (fb_base == NULL) return; 
    for (int row = 0; row < 16; row++) {
        unsigned char row_data = font_bitmap[((unsigned char)c * 16) + row];
        for (int col = 0; col < 8; col++) {
            if (row_data & (0x80 >> col)) {
                fb_base[(y + row) * PixelsPerScanLine + (x + col)] = foreground;
            } else {
                fb_base[(y + row) * PixelsPerScanLine + (x + col)] = background;
            }
        }
    }
}

void gprint(char* str, UINT32 fg, UINT32 bg) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            CursorX = 20;
            CursorY += 16;
            continue;
        }
        put_char(CursorX, CursorY, str[i], fg, bg);
        CursorX += 8;
        if (CursorX > ScreenWidth - 20) {
            CursorX = 20;
            CursorY += 16;
        }
    }
}*/



void exit_uefi_services(EFI_HANDLE ImageHandle) {
    UINTN MemoryMapSize = 0;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
    UINTN MapKey;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;
    EFI_STATUS status;

    // Use the BS global from gnu-efi and the wrapper!
    uefi_call_wrapper(BS->GetMemoryMap, 5, &MemoryMapSize, NULL, &MapKey, &DescriptorSize, &DescriptorVersion);
    
    // Add extra space for the allocation itself
    MemoryMapSize += 4096; 
    
    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, MemoryMapSize, (VOID**)&MemoryMap);
    if (EFI_ERROR(status)) {
        // If we can't allocate here, we are in deep trouble
        while(1) __asm__ volatile("hlt");
    }

    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    
    __asm__ volatile ("cli");
    
    status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, MapKey);
    
    if (EFI_ERROR(status)) {
        // Key might have changed, try one last time
        __asm__ volatile ("sti");
        uefi_call_wrapper(BS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
        __asm__ volatile ("cli");
        status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, MapKey);
    }

    if (EFI_ERROR(status)) {
        while(1) __asm__ volatile("hlt");
    }

    // Success! Save the map for the PMM
    global_mmap = MemoryMap;
    global_mmap_size = MemoryMapSize;
    global_desc_size = DescriptorSize;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_STATUS status;

    // 1. SAFELY Locate Graphics
    status = uefi_call_wrapper(BS->LocateProtocol, 3, &gopGuid, NULL, (VOID**)&gop);
    
    if (EFI_ERROR(status) || gop == NULL) {
        Print(L"CRITICAL ERROR: GOP Graphics Protocol not found!\n");
        // Stall so you can read the error before reboot
        uefi_call_wrapper(BS->Stall, 1, 5000000); 
        return status;
    }

    // 2. Initialize Subsystems ONLY if we have graphics
   /* console_init(
        (UINT32*)(UINTN)gop->Mode->FrameBufferBase,
        gop->Mode->Info->HorizontalResolution,
        gop->Mode->Info->VerticalResolution,
        gop->Mode->Info->PixelsPerScanLine
    ); */

    UINT32* saved_fb = (UINT32*)(UINTN)gop->Mode->FrameBufferBase;
    UINT32 saved_width = gop->Mode->Info->HorizontalResolution;
    UINT32 saved_height = gop->Mode->Info->VerticalResolution;
    UINT32 saved_pitch = gop->Mode->Info->PixelsPerScanLine;

        // Initialize IDT BEFORE enabling interrupts

    
    //__asm__ volatile ("cli");
       
        
      init_ttf_font(16);      
    exit_uefi_services(ImageHandle);
    uefi_exited = TRUE;
     
           
      console_init(saved_fb, saved_width, saved_height, saved_pitch);   
      pmm_init(global_mmap, global_mmap_size, global_desc_size);
              // Inside kernel_native.c main or init function:
    uint64_t* kernel_pml4 = vmm_create_kernel_pml4();

    // Load the new page tables into CR3
    __asm__ volatile("mov %0, %%cr3" : : "r"(kernel_pml4));
      // Initialize double buffer
        if (db_init() != 0) {
            console_print("Failed to allocate back buffer!\n");
        }
      // Initialize hardware controllers
        idt_init();
        pic_init();
        pit_init();

    // Initialize input devices AFTER interrupts are set up
    keyboard_init_state();
    mouse_init_state();
    mouse_enable();  // This will initialize hardware and enable mouse


    
    

    
    // Initialize other systems

   
    
    
    //kprintf("LEAN KERNEL INITIALIZED\n-----------------------\n> ");
    // NOW initialize the PMM with the map we just grabbed
    

    //console_print("UEFI EXITED. RUNNING IN PURE KERNEL MODE.\n> ");

    __asm__ volatile ("sti");



    //console_print("VMM: Paging active. Memory Protected.\n");   

    // 3. The Main Loop
    while(1) {
        __asm__ volatile ("hlt"); 

        while (head != tail) {
            OS_Event e = pop_event();
            
            if (e.type == EVENT_KEYBOARD) {
                handle_input((char)e.data1, (uint8_t)e.data2);
            }
        }
    }
    return EFI_SUCCESS;
}
