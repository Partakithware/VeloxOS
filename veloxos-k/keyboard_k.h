// keyboard_k.h - PS/2 Keyboard Driver Header
#ifndef KEYBOARD_K_H
#define KEYBOARD_K_H

#include <efi.h>
#include <efilib.h>

// Keyboard state structure
typedef struct {
    int shift_pressed;
    int ctrl_pressed;
    int alt_pressed;
    UINT8 last_scancode;
    unsigned char last_ascii;
} KeyboardState;

// Keyboard handler callback type
typedef void (*KeyboardHandler)(KeyboardState* state);

// Initialize keyboard state
void keyboard_init_state(void);

// Set callback handler for keyboard events
void keyboard_set_handler(KeyboardHandler handler);

// Get current keyboard state
KeyboardState* keyboard_get_state(void);

// Called by IRQ handler when keyboard data arrives
void keyboard_irq_handler(void);

// Utility: Convert scancode to ASCII
unsigned char keyboard_scancode_to_ascii(UINT8 scancode, int shift);

#endif // KEYBOARD_K_H