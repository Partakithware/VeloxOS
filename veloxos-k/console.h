#ifndef CONSOLE_H
#define CONSOLE_H

#include <efi.h>
#include <efilib.h>

extern UINT32 ScreenWidth;
extern UINT32 ScreenHeight;
extern UINT32 PixelsPerScanLine;
extern UINT32* fb_base;

// Initialize the console with framebuffer details
void console_init(UINT32* fb, UINT32 width, UINT32 height, UINT32 pitch);

// The main entry point for processing characters
void console_write_char(char c);

// Helper for strings
void console_print(char* str);

void console_clear();

void console_backspace_variable(int width);

#endif