#include "double_buffer.h"
#include "console.h" /* For ScreenWidth, ScreenHeight, fb_base, PixelsPerScanLine */

/* Bring in our zero-dependency graphics layer */
#include "sysfb.h"
#define BARE_GFX_IMPLEMENTATION
#include "baregfx.h"

/* The global context handling the smart-rendering */
static bgfx_context g_ctx;
static int db_initialized = 0;

int db_init(void) {
	if (db_initialized) return 0;

	/* Grab the BSS-allocated back buffer from our static pool. Zero mallocs! */
	unsigned int* bb = sys_allocate_back_buffer(ScreenWidth, ScreenHeight, PixelsPerScanLine);
	if (!bb) return -1; /* Failed to allocate from static pool */

	/* Initialize the compositor */
	bgfx_init(&g_ctx, fb_base, bb, ScreenWidth, ScreenHeight, PixelsPerScanLine);
	
	db_initialized = 1;
	
	/* Clear to black initially */
	db_clear(0x00000000);
	
	return 0; /* Success */
}

unsigned int* db_get_buffer(void) {
	if (!db_initialized) return 0;
	return g_ctx.back_buffer;
}

void db_swap(void) {
	if (!db_initialized) return;
	
	/* Pushes ONLY the pixels inside the damage tracking box */
	bgfx_swap_buffers(&g_ctx);
}

void db_mark_dirty(int x, int y, int w, int h) {
	if (!db_initialized) return;
	
	int max_x = x + w;
	int max_y = y + h;
	
	/* Physical screen bounds checking to prevent overflows */
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (max_x > (int)g_ctx.width) max_x = (int)g_ctx.width;
	if (max_y > (int)g_ctx.height) max_y = (int)g_ctx.height;

	if (!g_ctx.is_dirty) {
		g_ctx.dirty_min_x = x;
		g_ctx.dirty_min_y = y;
		g_ctx.dirty_max_x = max_x;
		g_ctx.dirty_max_y = max_y;
		g_ctx.is_dirty = 1;
	} else {
		if (x < (int)g_ctx.dirty_min_x) g_ctx.dirty_min_x = x;
		if (y < (int)g_ctx.dirty_min_y) g_ctx.dirty_min_y = y;
		if (max_x > (int)g_ctx.dirty_max_x) g_ctx.dirty_max_x = max_x;
		if (max_y > (int)g_ctx.dirty_max_y) g_ctx.dirty_max_y = max_y;
	}
}

void db_clear(unsigned int color) {
	if (!db_initialized) return;
	
	/* Use baregfx's virtual math to paint 100.00% of the screen */
	bgfx_draw_rect_v(&g_ctx, 0, 0, BGFX_VMAX, BGFX_VMAX, color);
}

void db_put_pixel(int x, int y, unsigned int color) {
	if (!db_initialized) return;
	if (x < 0 || x >= (int)g_ctx.width || y < 0 || y >= (int)g_ctx.height) return;
	
	g_ctx.back_buffer[y * g_ctx.pitch + x] = color;
	db_mark_dirty(x, y, 1, 1);
}

void db_draw_rect(int x, int y, int width, int height, unsigned int color) {
	for (int i = 0; i < width; i++) db_put_pixel(x + i, y, color);
	for (int i = 0; i < width; i++) db_put_pixel(x + i, y + height - 1, color);
	for (int i = 0; i < height; i++) db_put_pixel(x, y + i, color);
	for (int i = 0; i < height; i++) db_put_pixel(x + width - 1, y + i, color);
}

void db_fill_rect(int x, int y, int width, int height, unsigned int color) {
	if (!db_initialized) return;
	
	int end_x = x + width;
	int end_y = y + height;
	
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (end_x > (int)g_ctx.width) end_x = (int)g_ctx.width;
	if (end_y > (int)g_ctx.height) end_y = (int)g_ctx.height;
	
	/* Write directly to memory for speed */
	for (int dy = y; dy < end_y; dy++) {
		unsigned int* row = g_ctx.back_buffer + (dy * g_ctx.pitch);
		for (int dx = x; dx < end_x; dx++) {
			row[dx] = color;
		}
	}
	
	/* Update the damage tracker just once for the whole block */
	db_mark_dirty(x, y, width, height);
}

void db_draw_line(int x0, int y0, int x1, int y1, unsigned int color) {
	int dx = x1 - x0;
	int dy = y1 - y0;
	
	if (dx < 0) dx = -dx;
	if (dy < 0) dy = -dy;
	
	int sx = x0 < x1 ? 1 : -1;
	int sy = y0 < y1 ? 1 : -1;
	int err = dx - dy;
	
	/* Track the bounds of the line for the dirty rect */
	int min_x = x0 < x1 ? x0 : x1;
	int min_y = y0 < y1 ? y0 : y1;
	
	while (1) {
		db_put_pixel(x0, y0, color);
		if (x0 == x1 && y0 == y1) break;
		int e2 = 2 * err;
		if (e2 > -dy) { err -= dy; x0 += sx; }
		if (e2 < dx) { err += dx; y0 += sy; }
	}
	
	db_mark_dirty(min_x, min_y, dx + 1, dy + 1);
}

void db_cleanup(void) {
	/* Since we use static BSS memory, there is no heap to free! */
	/* Literally zero chance of a double-free or memory leak. */
	db_initialized = 0;
}

//OLD SYSTEM BELOW -- GROSS

/*#include "double_buffer.h"
#include "memory.h"
#include "console.h" // For ScreenWidth, ScreenHeight, fb_base, etc.
#include <stddef.h>

static uint32_t* back_buffer = NULL;
static uint32_t buffer_width = 0;
static uint32_t buffer_height = 0;

int db_init(void) {
    buffer_width = ScreenWidth;
    buffer_height = ScreenHeight;
    
    // Calculate buffer size in bytes
    uint64_t buffer_size = buffer_width * buffer_height * sizeof(uint32_t);
    
    // Allocate the back buffer using your kmalloc
    back_buffer = (uint32_t*)kmalloc(buffer_size);
    
    if (back_buffer == NULL) {
        return -1; // Failed to allocate
    }
    
    // Clear to black initially
    db_clear(0x00000000);
    
    return 0; // Success
}

uint32_t* db_get_buffer(void) {
    return back_buffer;
}

void db_swap(void) {
    if (back_buffer == NULL) return;
    
    // Copy entire back buffer to framebuffer
    uint32_t* fb = (uint32_t*)fb_base;
    
    for (uint32_t y = 0; y < buffer_height; y++) {
        for (uint32_t x = 0; x < buffer_width; x++) {
            uint32_t offset = y * PixelsPerScanLine + x;
            uint32_t buf_offset = y * buffer_width + x;
            fb[offset] = back_buffer[buf_offset];
        }
    }
}

void db_clear(uint32_t color) {
    if (back_buffer == NULL) return;
    
    for (uint32_t i = 0; i < buffer_width * buffer_height; i++) {
        back_buffer[i] = color;
    }
}

void db_put_pixel(int x, int y, uint32_t color) {
    if (back_buffer == NULL) return;
    if (x < 0 || x >= buffer_width || y < 0 || y >= buffer_height) return;
    
    back_buffer[y * buffer_width + x] = color;
}

void db_draw_rect(int x, int y, int width, int height, uint32_t color) {
    // Draw outline only (4 lines)
    
    // Top line
    for (int i = 0; i < width; i++) {
        db_put_pixel(x + i, y, color);
    }
    
    // Bottom line
    for (int i = 0; i < width; i++) {
        db_put_pixel(x + i, y + height - 1, color);
    }
    
    // Left line
    for (int i = 0; i < height; i++) {
        db_put_pixel(x, y + i, color);
    }
    
    // Right line
    for (int i = 0; i < height; i++) {
        db_put_pixel(x + width - 1, y + i, color);
    }
}

void db_fill_rect(int x, int y, int width, int height, uint32_t color) {
    for (int dy = 0; dy < height; dy++) {
        for (int dx = 0; dx < width; dx++) {
            db_put_pixel(x + dx, y + dy, color);
        }
    }
}

void db_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    // Bresenham's line algorithm
    int dx = x1 - x0;
    int dy = y1 - y0;
    
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        db_put_pixel(x0, y0, color);
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void db_cleanup(void) {
    if (back_buffer != NULL) {
        kfree(back_buffer);
        back_buffer = NULL;
    }
}*/