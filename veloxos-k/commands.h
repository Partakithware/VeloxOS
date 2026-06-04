#ifndef COMMANDS_H
#define COMMANDS_H

#include <efi.h>
#include <efilib.h>

// This is the main entry point from your kernel loop
void handle_input(char c, uint8_t scancode);

// Internal logic for processing a finished string
void process_command(char* cmd);

#endif