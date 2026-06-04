#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// Returns total milliseconds since boot
uint64_t get_uptime_ms();

// The hardware interrupt handler
void timer_handler(void);

#endif