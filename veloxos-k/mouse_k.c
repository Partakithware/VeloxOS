// mouse_k.c - PS/2 Mouse Driver
#include "mouse_k.h"
#include "event.h"

// Mouse state
static MouseState mouse_state = {0};
static MouseHandler current_handler = NULL;

// Mouse packet buffer
static UINT8 mouse_cycle = 0;
static UINT8 mouse_byte[3];

// External dependencies
extern UINT32 ScreenWidth;
extern UINT32 ScreenHeight;
extern UINT32 PixelsPerScanLine;

// Helper to read from port
static inline UINT8 inb(UINT16 port) {
    UINT8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Helper to write to port
static inline void outb(UINT16 port, UINT8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

// Wait for PS/2 controller
static void mouse_wait(UINT8 type) {
    UINT32 timeout = 100000;
    if (type == 0) {
        // Wait for output buffer to be full
        while (timeout--) {
            if (inb(0x64) & 1) return;
        }
    } else {
        // Wait for input buffer to be empty
        while (timeout--) {
            if (!(inb(0x64) & 2)) return;
        }
    }
}

// Write to mouse
static void mouse_write(UINT8 data) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, data);
}

// Read from mouse
static UINT8 mouse_read(void) {
    mouse_wait(0);
    return inb(0x60);
}

// Hardware initialization
static void mouse_init_hardware(void) {
    UINT8 status;
    
    // Enable auxiliary device (mouse)
    mouse_wait(1);
    outb(0x64, 0xA8);
    
    // Enable interrupts
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    status = (inb(0x60) | 2);
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);
    
    // Use default settings
    mouse_write(0xF6);
    mouse_read(); // Acknowledge
    
    // Enable data reporting
    mouse_write(0xF4);
    mouse_read(); // Acknowledge
}

// Initialize mouse state
void mouse_init_state(void) {
    mouse_state.x = ScreenWidth / 2;
    mouse_state.y = ScreenHeight / 2;
    mouse_state.buttons = 0;
    mouse_state.active = 0;
    mouse_state.ready = 0;
    mouse_cycle = 0;
    current_handler = NULL;
}

// Enable mouse
void mouse_enable(void) {
    mouse_init_hardware();
    mouse_state.active = 1;
    mouse_state.ready = 1;
}

// Disable mouse
void mouse_disable(void) {
    mouse_state.active = 0;
}

// Set mouse handler callback
void mouse_set_handler(MouseHandler handler) {
    current_handler = handler;
}

// Get current mouse state
MouseState* mouse_get_state(void) {
    return &mouse_state;
}

// Process mouse IRQ packet
void mouse_irq_handler(void) {
    if (!mouse_state.active) return;

    // 1. Hardware Communication (Must be fast!)
    UINT8 input = inb(0x60);
    mouse_byte[mouse_cycle++] = input;

    // Validate first byte (bit 3 check)
    if (mouse_cycle == 1) {
        if (!(mouse_byte[0] & 0x08)) {
            mouse_cycle = 0;
            return;
        }
    }

    // Wait for complete 3-byte packet
    if (mouse_cycle < 3) return;
    
    mouse_cycle = 0; // Reset for next packet

    // 2. Local State Update
    int dx = (int)((signed char)mouse_byte[1]);
    int dy = (int)((signed char)mouse_byte[2]);
    UINT8 buttons = mouse_byte[0] & 0x07;

    mouse_state.x += dx;
    mouse_state.y -= dy; // Y is inverted

    // Clamp to screen
    if (mouse_state.x < 0) mouse_state.x = 0;
    if (mouse_state.y < 0) mouse_state.y = 0;
    if (mouse_state.x >= (int)ScreenWidth) mouse_state.x = ScreenWidth - 1;
    if (mouse_state.y >= (int)ScreenHeight) mouse_state.y = ScreenHeight - 1;

    mouse_state.buttons = buttons;

    // 3. Push event to queue
    extern void push_event(OS_Event e);
    
    OS_Event e;
    e.type = EVENT_MOUSE;
    e.data1 = mouse_state.x;
    e.data2 = mouse_state.y;
    e.data3 = (int)buttons;
    
    push_event(e);
}