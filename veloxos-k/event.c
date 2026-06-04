#include "event.h"

// 8192 is ~320KB of RAM. Fine for 4GB, but keep an eye on it!
#define QUEUE_SIZE 8192 
OS_Event event_queue[QUEUE_SIZE];
volatile int head = 0;
volatile int tail = 0;

// IMPORTANT: Use these to prevent race conditions during pointer updates
static inline void disable_interrupts() { __asm__ volatile("cli"); }
static inline void enable_interrupts()  { __asm__ volatile("sti"); }
// Helper to check if queue is empty (for your loop)
int has_events(void) {
    return head != tail;
}


void push_event(OS_Event e) {
    disable_interrupts(); // LOCK
    
    int next = (head + 1) % QUEUE_SIZE;
    if (next != tail) {
        event_queue[head] = e;
        head = next;
    }
    
    enable_interrupts(); // UNLOCK
}

OS_Event pop_event(void) {
    OS_Event e = {EVENT_NONE, 0, 0, 0, 0};
    
    disable_interrupts(); // LOCK
    
    if (head != tail) {
        e = event_queue[tail];
        tail = (tail + 1) % QUEUE_SIZE;
    }
    
    enable_interrupts(); // UNLOCK
    return e;
}

