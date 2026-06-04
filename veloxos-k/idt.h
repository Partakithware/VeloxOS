// idt.h - Interrupt Descriptor Table structures and functions
#ifndef IDT_H
#define IDT_H

#include <efi.h>
#include <efilib.h>

// IDT gate types
#define IDT_GATE_INTERRUPT  0x8E  // 32-bit interrupt gate, present
#define IDT_GATE_TRAP       0x8F  // 32-bit trap gate, present

// IDT entry structure (16 bytes)
typedef struct {
    UINT16 offset_low;    // Lower 16 bits of handler address
    UINT16 selector;      // Code segment selector
    UINT8  ist;           // Interrupt Stack Table (0 = don't use)
    UINT8  type_attr;     // Type and attributes
    UINT16 offset_mid;    // Middle 16 bits of handler address
    UINT32 offset_high;   // Upper 32 bits of handler address
    UINT32 zero;          // Reserved, must be zero
} __attribute__((packed)) idt_entry_t;

// IDT pointer structure
typedef struct {
    UINT16 limit;         // Size of IDT - 1
    UINT64 base;          // Base address of IDT
} __attribute__((packed)) idt_ptr_t;

// Interrupt frame pushed by CPU
typedef struct {
    UINT64 rip;
    UINT64 cs;
    UINT64 rflags;
    UINT64 rsp;
    UINT64 ss;
} __attribute__((packed)) interrupt_frame_t;

// Number of IDT entries
#define IDT_ENTRIES 256

// Function prototypes
void idt_init(void);
void idt_set_gate(UINT8 num, UINT64 handler, UINT16 selector, UINT8 flags);
void idt_load(void);


// Exception handlers (implemented in C)
void exception_handler(interrupt_frame_t* frame, UINT64 error_code, UINT64 vector);

// IRQ handlers
void irq_handler(interrupt_frame_t* frame, UINT64 irq_num);

// Assembly interrupt stubs (defined in interrupts.asm)
extern void isr0(void);   // Divide by zero
extern void isr1(void);   // Debug
extern void isr2(void);   // NMI
extern void isr3(void);   // Breakpoint
extern void isr6(void);   // Invalid opcode
extern void isr8(void);   // Double fault
extern void isr13(void);  // General protection fault
extern void isr14(void);  // Page fault
extern void isr32(void);  // Timer (IRQ0)
extern void isr33(void);  // Keyboard (IRQ1)
extern void isr44(void);  // Mouse

#endif // IDT_H