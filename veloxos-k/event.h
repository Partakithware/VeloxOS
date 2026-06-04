#include <stdint.h>
// event.h
#ifndef EVENT_H
#define EVENT_H

typedef enum {
    EVENT_NONE,
    EVENT_KEYBOARD,
    EVENT_MOUSE,
    EVENT_TIMER,    // New
    EVENT_SYSTEM    // New
} EventType;

typedef struct {
    EventType type;
    int64_t data1; // Key ASCII / Mouse X / Timer Tick
    int64_t data2; // Scancode  / Mouse Y
    int64_t data3; // Modifiers / Mouse Buttons
    int64_t data4; // Reserved for 64-bit pointers/extra data
} OS_Event;

// Declarations of functions in event.c
void push_event(OS_Event e);
OS_Event pop_event(void);

// External declarations for the queue indices
extern volatile int head;
extern volatile int tail;

#endif
