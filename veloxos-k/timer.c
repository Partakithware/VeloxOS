#include "timer.h"
// volatile tells the compiler: "This changes outside your normal flow (in an interrupt)!"
static volatile uint64_t system_ticks = 0;

uint64_t get_uptime_ms() {
    // Since our PIT is 100Hz, each tick is 10ms
    return system_ticks * 10;
}

// This will be called from your assembly stub
void timer_handler(void) {
    system_ticks++;

    // Send EOI (End of Interrupt) to the Master PIC (Port 0x20)
    // If we don't do this, we only ever get ONE timer tick!
    __asm__ volatile ("outb %0, $0x20" : : "a"((uint8_t)0x20));
}
