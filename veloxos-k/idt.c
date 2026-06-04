// idt.c - Interrupt Descriptor Table implementation
#include "idt.h"
#include "mouse_k.h"
#include "keyboard_k.h"
#include "timer.h"
#include "console.h"

static inline UINT8 inb(UINT16 port) {
    UINT8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// IDT table and pointer
static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idtr;

extern void timer_handler(); // From your timer.c

// Set an IDT gate
void idt_set_gate(UINT8 num, UINT64 handler, UINT16 selector, UINT8 flags) {
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].selector = selector;
    idt[num].ist = 0;
    idt[num].type_attr = flags;
    idt[num].offset_mid = (handler >> 16) & 0xFFFF;
    idt[num].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[num].zero = 0;
}

// Load IDT
void idt_load(void) {
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}

// Initialize IDT
void idt_init(void) {
    // Set up IDT pointer
    idtr.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idtr.base = (UINT64)&idt;
    
    // Clear IDT
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }
    
    // Get current code segment from CS register
    UINT16 code_seg;
    __asm__ volatile ("mov %%cs, %0" : "=r"(code_seg));
    
    // Set up exception handlers (0-31)
    idt_set_gate(0, (UINT64)isr0, code_seg, IDT_GATE_INTERRUPT);   // Divide by zero
    idt_set_gate(1, (UINT64)isr1, code_seg, IDT_GATE_INTERRUPT);   // Debug
    idt_set_gate(2, (UINT64)isr2, code_seg, IDT_GATE_INTERRUPT);   // NMI
    idt_set_gate(3, (UINT64)isr3, code_seg, IDT_GATE_INTERRUPT);   // Breakpoint
    idt_set_gate(6, (UINT64)isr6, code_seg, IDT_GATE_INTERRUPT);   // Invalid opcode
    idt_set_gate(8, (UINT64)isr8, code_seg, IDT_GATE_INTERRUPT);   // Double fault
    idt_set_gate(13, (UINT64)isr13, code_seg, IDT_GATE_INTERRUPT); // GPF
    idt_set_gate(14, (UINT64)isr14, code_seg, IDT_GATE_INTERRUPT); // Page fault
    
    // Set up IRQ handlers (32-47)
    idt_set_gate(32, (UINT64)isr32, code_seg, IDT_GATE_INTERRUPT); // Timer
    idt_set_gate(33, (UINT64)isr33, code_seg, IDT_GATE_INTERRUPT); // Keyboard
    idt_set_gate(44, (UINT64)isr44, code_seg, IDT_GATE_INTERRUPT); // Mouse
    
    // Load IDT
    idt_load();
}

// Exception handler (called from ASM stubs)
void exception_handler(interrupt_frame_t* frame, UINT64 error_code, UINT64 vector) {
    const char* exceptions[] = {
        "Divide by Zero", "Debug", "NMI", "Breakpoint",
        "Overflow", "Bound Range", "Invalid Opcode", "Device Not Available",
        "Double Fault", "Coprocessor Segment", "Invalid TSS", "Segment Not Present",
        "Stack Fault", "General Protection", "Page Fault", "Reserved",
    };
    
    // Use YOUR console, not UEFI Print()
    console_print("\n!!! EXCEPTION !!!\n");
    
    if (vector < 16) {
        console_print("Exception: ");
        console_print(exceptions[vector]);
        console_print("\n");
    } else {
        console_print("Unknown Exception\n");
    }
    
    console_print("System Halted.\n");
    
    // Halt
    __asm__ volatile ("cli; hlt");
    while(1) { __asm__ volatile ("hlt"); }
}

// IRQ handler - THIN DISPATCHER
void irq_handler(interrupt_frame_t* frame, UINT64 irq_num) {
    // Handle specific IRQs
    switch(irq_num) {
        case 0:  timer_handler();// Timer
    // Just increment a global tick counter if you need one.
    // REMOVE desktop_render() from here!
    break;
            
        case 1:  // Keyboard
            keyboard_irq_handler();  // Dispatch to keyboard driver
            break;

        case 12: // Mouse
            mouse_irq_handler();  // Dispatch to mouse driver
            break;
    }
    
    // Send End of Interrupt to PIC
    if (irq_num >= 8) {
        __asm__ volatile ("outb %0, $0xA0" : : "a"((UINT8)0x20)); // Slave PIC
    }
    __asm__ volatile ("outb %0, $0x20" : : "a"((UINT8)0x20)); // Master PIC
}
