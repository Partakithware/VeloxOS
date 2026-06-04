#ifndef DOUBLE_BUFFER_H
#define DOUBLE_BUFFER_H

/* Zero-bloat double buffer system wrapper */

int db_init(void);
unsigned int* db_get_buffer(void);
void db_swap(void);
void db_clear(unsigned int color);

/* Drawing primitives on the back buffer */
void db_put_pixel(int x, int y, unsigned int color);
void db_draw_rect(int x, int y, int width, int height, unsigned int color);
void db_fill_rect(int x, int y, int width, int height, unsigned int color);
void db_draw_line(int x0, int y0, int x1, int y1, unsigned int color);

/* NEW: Tells the compositor a specific area was altered */
void db_mark_dirty(int x, int y, int w, int h);

/* Cleanup (No-op in the static BSS model) */
void db_cleanup(void);

#endif

//OLD SYSTEM BELOW - GROSS

/*#ifndef DOUBLE_BUFFER_H
#define DOUBLE_BUFFER_H

#include <stdint.h>

// Initialize the double buffer system
// This allocates a back buffer the same size as your framebuffer
int db_init(void);

// Get pointer to back buffer for drawing
uint32_t* db_get_buffer(void);

// Swap back buffer to screen (makes your drawing visible)
void db_swap(void);

// Clear the back buffer to a color
void db_clear(uint32_t color);

// Drawing primitives on the back buffer
void db_put_pixel(int x, int y, uint32_t color);
void db_draw_rect(int x, int y, int width, int height, uint32_t color);
void db_fill_rect(int x, int y, int width, int height, uint32_t color);
void db_draw_line(int x0, int y0, int x1, int y1, uint32_t color);

// Cleanup (free the back buffer)
void db_cleanup(void);

#endif*/