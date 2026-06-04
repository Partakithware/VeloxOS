// mouse_k.h - PS/2 Mouse Driver Header
#ifndef MOUSE_K_H
#define MOUSE_K_H

#include <efi.h>
#include <efilib.h>

// Mouse state structure
typedef struct {
    int x;              // Current X position
    int y;              // Current Y position
    UINT8 buttons;      // Button state (bit 0=left, 1=right, 2=middle)
    int active;         // Is mouse enabled?
    int ready;          // Is mouse initialized?
} MouseState;

// Mouse handler callback type
typedef void (*MouseHandler)(MouseState* state);

// Initialize mouse state
void mouse_init_state(void);

// Enable/disable mouse
void mouse_enable(void);
void mouse_disable(void);

// Set callback handler for mouse events
void mouse_set_handler(MouseHandler handler);

// Get current mouse state
MouseState* mouse_get_state(void);

// Called by IRQ handler when mouse data arrives
void mouse_irq_handler(void);


#endif // MOUSE_K_H