#include "kdesktop.h"

// 1. Trick VeloxFS so it doesn't try to pull in Linux <stdlib.h> / <string.h>
#define veloxfs_MALLOC(sz)         0
#define veloxfs_CALLOC(n, sz)      0
#define veloxfs_FREE(p)            0
#define veloxfs_MEMSET(d,c,n)      0
#define veloxfs_MEMCPY(d,s,n)      0
#define veloxfs_STRLEN(s)          0
#define veloxfs_STRCMP(a,b)        0
#define veloxfs_STRNCMP(a,b,n)     0
#define veloxfs_STRNCPY(d,s,n)     0
#define veloxfs_SNPRINTF(d,n,...)  0
#define veloxfs_LOG(fmt,...)       0
#define veloxfs_TIME()             0

#include "veloxfs.h"
#include "double_buffer.h" 
#include "font_ttf.h"      

// 2. Tell the compiler ScreenWidth exists in another file
extern int ScreenWidth;

// Clean, reusable text rendering to keep UI logic readable
static void draw_string(int x, int y, const char* str, uint32_t color) {
    int pen_x = 0;
    uint32_t* buf = db_get_buffer();
    
    if (!buf) return;

    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        ttf_put_char_buf(buf, ScreenWidth, x + pen_x, y, c, color);
        pen_x += get_ttf_char_width(c) + 1; // Native font spacing
    }
}

void kdesktop_render_file_explorer(Window* win, void* context_ptr) {
    FileExplorerContext* ctx = (FileExplorerContext*)context_ptr;
    if (!ctx) return;

    // 1. Draw Window Background
    db_fill_rect(win->x, win->y, win->width, win->height, ADWAITA_WINDOW_BG);
    
    // 2. Draw Header Bar (Matching your 35px titlebar from kdesktop.c)
    db_fill_rect(win->x, win->y, win->width, 35, ADWAITA_HEADER);

    // 3. Render Path Bar
    draw_string(win->x + 10, win->y + 10, ctx->current_path, ADWAITA_TEXT);
    
    // 4. Render File List
    int y_pos = win->y + 45; // Start just below the header

    // Guard for empty directories
    if (ctx->file_count == 0) {
        draw_string(win->x + 10, y_pos, "Directory is empty...", COLOR_TEXT_GRAY);
        return;
    }

    for (int i = 0; i < ctx->file_count; i++) {
        uint32_t bg = (i == ctx->selected_idx) ? ADWAITA_ACCENT : ADWAITA_WINDOW_BG;
        
        // Draw selection row if focused
        if (i == ctx->selected_idx) {
            db_fill_rect(win->x + 2, y_pos - 2, win->width - 4, 20, bg);
        }

        // Render file entry
        draw_string(win->x + 10, y_pos, "placeholder.txt", ADWAITA_TEXT);
        
        y_pos += 20; // Exact row height for utilitarian alignment
    }
}

// The Input Handler routed from kdesktop_handle_keyboard
void kdesktop_handle_explorer_input(Window* win, OS_Event* e) {
    FileExplorerContext* ctx = (FileExplorerContext*)win->extra_data;
    if (!ctx) return;
    
    if (e->type == EVENT_KEYBOARD) {
        char key = (char)e->data1;
        
        // Using 'w' and 's' for scrolling right now
        if (key == 'w' && ctx->selected_idx > 0) {
            ctx->selected_idx--;
        } else if (key == 's' && ctx->selected_idx < ctx->file_count - 1) {
            ctx->selected_idx++;
        }
    }
}