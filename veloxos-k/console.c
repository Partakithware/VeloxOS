#include "console.h"
#include "font.h"
#include "font_ttf.h"
#include "kstring.h"

// Explicitly define the globals declared in console.h
extern UINT32* fb_base;
extern UINT32 ScreenWidth;
extern UINT32 ScreenHeight;
extern UINT32 PixelsPerScanLine;

UINT32 CursorX = 20;
UINT32 CursorY = 50;

#define COLOR_FG 0x00FFFFFF
#define COLOR_BG 0x00000000

void console_init(UINT32* fb, UINT32 width, UINT32 height, UINT32 pitch) {
    fb_base = fb;
    ScreenWidth = width;
    ScreenHeight = height;
    PixelsPerScanLine = pitch;
    
    if (fb_base != NULL) {
    // This is the "Powerhouse" way: Fill the entire buffer at once
    // ScreenWidth * ScreenHeight * 4 bytes (assuming 32-bit pixels)
    kmemset(fb_base, 0, ScreenHeight * PixelsPerScanLine * 4);
    
    // If you want your specific COLOR_BG (0x00111111), 
    // keep your loop, but it's now officially "The Initializer"
}
}



void draw_filled_rect(int x, int y, int w, int h, uint32_t color) {
    for (int curr_y = y; curr_y < y + h; curr_y++) {
        for (int curr_x = x; curr_x < x + w; curr_x++) {
            // Safety check to stay within screen bounds
            if (curr_x >= 0 && curr_x < ScreenWidth && curr_y >= 0 && curr_y < ScreenHeight) {
                uint32_t* pixel = (uint32_t*)fb_base + curr_y * PixelsPerScanLine + curr_x;
                *pixel = color;
            }
        }
    }
}

void console_backspace_variable(int width) {
    if (CursorX > 50) {
        CursorX -= width;
        
        // Calculate vertical area based on font metrics
        int font_height = (int)((font_ascent - font_descent) * font_scale);
        int draw_y = CursorY + (int)(font_ascent * font_scale) - font_height;

        // Erase the EXACT width of the character
        draw_filled_rect(CursorX, draw_y, width, font_height + 5, COLOR_BG);
    }
}

void console_scroll() {
    int line_height = (int)((font_ascent - font_descent + font_lineGap) * font_scale) + 4;

    // 1. Move the entire screen UP by one line height
    // We start copying from the second line (fb_base + pitch * line_height) 
    // to the very top (fb_base)
    void* src = (void*)((uint8_t*)fb_base + (PixelsPerScanLine * line_height * 4));
        // Use UINTN (the UEFI standard) or uint64_t
        UINTN bytes_to_copy = (UINTN)(ScreenHeight - line_height) * PixelsPerScanLine * 4;
    
    kmemcpy(fb_base, src, bytes_to_copy);

    // 2. Wipe the newly created empty space at the bottom
    void* bottom_line = (void*)((uint8_t*)fb_base + (ScreenHeight - line_height) * PixelsPerScanLine * 4);
    kmemset(bottom_line, 0, line_height * PixelsPerScanLine * 4);

    // 3. Adjust the cursor so it stays on the bottom line
    CursorY -= line_height;
}

void console_write_char(char c) {
    if (c == '\n') {
        CursorX = 50;
        // Add 1.5x scaling to the line jump for "breathing room"
        int line_height = (int)((font_ascent - font_descent + font_lineGap) * font_scale);
        CursorY += (line_height + 4); // Extra 4 pixels of padding
    }
    else if (c == '\r') {
        CursorX = 50;
    }
    else if (c == '\b') {
    if (CursorX > 50) {
            // 1. Move cursor back (you'll need to store previous character widths 
            // for perfect backspace, but for now, let's just jump back 12 pixels)
            CursorX -= 12; 
            
            // 2. Draw a black box over the old character to "erase" it
            // (You can use a simple loop or a clear_rect function)
            draw_filled_rect(CursorX, CursorY, 12, 24, COLOR_BG); 
        }
    }
    else if (c >= 32) { 
        // Printable characters
        ttf_put_char(CursorX, CursorY, c, 0xFFFFFFFF);
        CursorX += get_ttf_char_width(c) + 1;
    }

    // Handle screen wrapping
    if (CursorY > ScreenHeight - 100) { // Give it some padding
        console_scroll();
    }
}

void console_print(char* str) {
    while (*str) console_write_char(*str++);
}

void console_clear() {
    if (fb_base == NULL) return;

    // Fast wipe using your new kstring library
    // 0 is black. If you want COLOR_BG, use your fill loop here.
    kmemset(fb_base, 0, ScreenHeight * PixelsPerScanLine * 4);

    CursorX = 50;
    CursorY = 50;
}

