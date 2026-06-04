// keyboard_k.c - PS/2 Keyboard Driver
#include "keyboard_k.h"
#include "event.h"

// Keyboard state
static KeyboardState kbd_state = {0};
static KeyboardHandler current_handler = NULL;

// Standard US Keyboard Map (Set 1)
static unsigned char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0
};

// Shifted version for capital letters/symbols
static unsigned char kbd_us_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0
};

// Helper to read from port
static inline UINT8 inb(UINT16 port) {
    UINT8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Initialize keyboard state
void keyboard_init_state(void) {
    kbd_state.shift_pressed = 0;
    kbd_state.ctrl_pressed = 0;
    kbd_state.alt_pressed = 0;
    kbd_state.last_scancode = 0;
    kbd_state.last_ascii = 0;
    current_handler = NULL;
}

// Set keyboard handler callback
void keyboard_set_handler(KeyboardHandler handler) {
    current_handler = handler;
}

// Get current keyboard state
KeyboardState* keyboard_get_state(void) {
    return &kbd_state;
}

// Process keyboard IRQ
void keyboard_irq_handler(void) {
    UINT8 scancode = inb(0x60);
    kbd_state.last_scancode = scancode;

    // 1. Update Modifier States (Shift, Ctrl, Alt)
    // These remain in the IRQ because they change how future keys are interpreted
    if (scancode == 0x2A || scancode == 0x36) {
        kbd_state.shift_pressed = 1;
        return;
    } else if (scancode == 0xAA || scancode == 0xB6) {
        kbd_state.shift_pressed = 0;
        return;
    }

    if (scancode == 0x1D) {
        kbd_state.ctrl_pressed = 1;
        return;
    } else if (scancode == 0x9D) {
        kbd_state.ctrl_pressed = 0;
        return;
    }

    if (scancode == 0x38) {
        kbd_state.alt_pressed = 1;
        return;
    } else if (scancode == 0xB8) {
        kbd_state.alt_pressed = 0;
        return;
    }

    // 2. Process Key Down Events
    if (!(scancode & 0x80)) { // Key Press
    unsigned char ascii = kbd_state.shift_pressed ? kbd_us_shift[scancode] : kbd_us[scancode];
    kbd_state.last_ascii = ascii;

    // Push event if it's a printable ASCII OR if it's a special scancode we care about
    // 0x48 is UP, 0x50 is DOWN
    if (ascii != 0 || scancode == 0x48 || scancode == 0x50) {
        extern void push_event(OS_Event e);
        
        OS_Event e;
        e.type = EVENT_KEYBOARD;
        e.data1 = (int)ascii;     // Will be 0 for arrows
        e.data2 = (int)scancode;  // This is where we'll see 0x48
        e.data3 = (int)kbd_state.shift_pressed;
        
        push_event(e);
    }
}
}

// Get ASCII for a scancode (utility function)
unsigned char keyboard_scancode_to_ascii(UINT8 scancode, int shift) {
    if (scancode >= 128) return 0;
    return shift ? kbd_us_shift[scancode] : kbd_us[scancode];
}