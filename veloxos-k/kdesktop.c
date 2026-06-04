#include "kdesktop.h"
#include "double_buffer.h"
#include "event.h"
#include "mouse_k.h"
#include "console.h"
#include "memory.h"
#include "kstring.h"
#include "kstdlib.h"
#include "stdint.h"     // <--- fix UINT64_MAX
#include "font_ttf.h"
#include "commands.h"
#include "pmm.h"      // <--- ADD THIS for pmm_get_free_pages

#include "ramdisk.h"


// --- Update your Glue Layer ---
#define veloxfs_MALLOC(sz)     kmalloc(sz)
#define veloxfs_FREE(p)        kfree(p)
#define veloxfs_CALLOC(n, sz)  ({ void* ptr = kmalloc((n)*(sz)); if(ptr) kmemset(ptr, 0, (n)*(sz)); ptr; })
#define veloxfs_MEMSET(d,c,n)  kmemset(d,c,n)
#define veloxfs_MEMCPY(d,s,n)  kmemcpy(d,s,n)
#define veloxfs_STRLEN(s)      strlen(s)
#define veloxfs_STRCMP(a,b)    strcmp(a,b)
#define veloxfs_STRNCMP(a,b,n) strncmp(a,b,n) // Now this exists!
#define veloxfs_STRNCPY(d,s,n) strncpy(d,s,n) // Now this exists!
#define veloxfs_SNPRINTF(d,n,...) snprintf(d,n,__VA_ARGS__) // Now this exists!


#define veloxfs_IMPLEMENTATION
#include "veloxfs.h"


// ADD THESE PROTOTYPES (so kdesktop knows these functions exist):
void ram_disk_init(void);
void ram_disk_read(uint64_t lba, uint32_t count, void* buffer);
void ram_disk_write(uint64_t lba, uint32_t count, const void* buffer);


// This bridges the gap: VeloxFS asks for bytes, RAM disk gives sectors.
static int veloxfs_read_adapter(void *user, uint64_t offset, void *buf, uint32_t size) {
    // 1. IS THIS NULL?
    if (buf == NULL) {
            kprintf("\n!!! CRITICAL ERROR !!!\n");
            kprintf("VeloxFS tried to read into a NULL buffer.\n");
            kprintf("Requested Offset: %llu\n", offset);
            kprintf("Requested Size:   %u\n", size);
            
            // This stops the system so you can read the screen
            while(1); 
    }
    
    if (!buf) { kprintf("VeloxFS: NULL read buffer at offset %llu\n", offset); while(1); }
    // kprintf("IO Read: Off: %d, Size: %d\n", offset, size); // REMOVE THIS
    // 2. IS OFFSET/SIZE VALID?
    // If offset + size goes beyond your RAM disk, you're writing to/reading from 
    // memory that doesn't exist, which will instantly kill the kernel.
    //kprintf("IO Read: Off: %d, Size: %d\n", offset, size);
    
    uint64_t lba = offset / 512;
    uint32_t count = (size + 511) / 512;
    ram_disk_read(lba, count, buf);
    return 0;
}

static int veloxfs_write_adapter(void *user, uint64_t offset, const void *buf, uint32_t size) {
    uint64_t lba = offset / 512;
    uint32_t count = (size + 511) / 512;
    ram_disk_write(lba, count, buf);
    return 0; // Success
}


// Desktop state
static Window* window_list = NULL;
static Window* focused_window = NULL;
static DesktopIcon* icon_list = NULL;
static int desktop_running = 0;
static int mouse_x = 0;
static int mouse_y = 0;
extern int kstrcmp(char* s1, char* s2);

// Helper: String copy
static void str_copy(char* dest, const char* src, int max_len) {
    int i = 0;
    while (src[i] != '\0' && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

uint32_t blend_pixel(uint32_t src_color, uint32_t dest_color, uint8_t alpha) {
    if (alpha == 255) return src_color;
    if (alpha == 0)   return dest_color;

    uint32_t rb = src_color & 0xFF00FF;
    uint32_t g  = src_color & 0x00FF00;

    uint32_t rb_dest = dest_color & 0xFF00FF;
    uint32_t g_dest  = dest_color & 0x00FF00;

    // The formula: (src * alpha + dest * (255 - alpha)) / 255
    uint32_t out_rb = (rb * alpha + rb_dest * (255 - alpha)) >> 8;
    uint32_t out_g  = (g * alpha + g_dest * (255 - alpha)) >> 8;

    return (out_rb & 0xFF00FF) | (out_g & 0x00FF00);
}

void db_draw_rect_transparent(int x, int y, int w, int h, uint32_t color, uint8_t alpha) {
    uint32_t* buffer = db_get_buffer();
    
    for (int curr_y = y; curr_y < y + h; curr_y++) {
        if (curr_y < 0 || curr_y >= ScreenHeight) continue;
        
        for (int curr_x = x; curr_x < x + w; curr_x++) {
            if (curr_x < 0 || curr_x >= ScreenWidth) continue;

            int offset = curr_y * PixelsPerScanLine + curr_x;
            uint32_t bg_color = buffer[offset];
            buffer[offset] = blend_pixel(color, bg_color, alpha);
        }
    }
    db_mark_dirty(x, y, w, h);
}

// Callback for terminal icon
void on_terminal_icon_click(void) {
    kdesktop_create_terminal(150, 100, 600, 400);
}

// Initialize desktop
void kdesktop_init(void) {
    window_list = NULL;
    focused_window = NULL;
    icon_list = NULL;
    desktop_running = 0;
    
    // Get initial mouse position
    MouseState* ms = mouse_get_state();
    mouse_x = ms->x;
    mouse_y = ms->y;
    
    // Create desktop icons
    kdesktop_create_icon(30, 30, "Terminal", on_terminal_icon_click);
    
    // Create a welcome window
    kdesktop_create_window(100, 100, 400, 200, "Welcome!", WINDOW_NORMAL);
}

// Create desktop icon
DesktopIcon* kdesktop_create_icon(int x, int y, const char* label, void (*callback)(void)) {
    DesktopIcon* icon = (DesktopIcon*)kmalloc(sizeof(DesktopIcon));
    if (!icon) return NULL;
    
    icon->x = x;
    icon->y = y;
    icon->width = ICON_SIZE;
    icon->height = ICON_SIZE + 20; // Extra for label
    str_copy(icon->label, label, 32);
    icon->color = COLOR_WINDOW_TITLE;
    icon->on_click = callback;
    
    // Add to list
    icon->next = icon_list;
    icon_list = icon;
    
    return icon;
}

// Create a terminal window
Window* kdesktop_create_terminal(int x, int y, int width, int height) {
    Window* win = kdesktop_create_window(x, y, width, height, "Terminal", WINDOW_TERMINAL);
    if (!win) return NULL;
    
    // Allocate terminal buffer (80x25 is classic)
    win->terminal_buffer_size = 80 * 25;
    win->terminal_buffer = (char*)kmalloc(win->terminal_buffer_size);
    if (win->terminal_buffer) {
        kmemset(win->terminal_buffer, 0, win->terminal_buffer_size);
    }
    
    win->terminal_cursor_x = 0;
    win->terminal_cursor_y = 0;
    win->bg_color = COLOR_TERMINAL_BG;
    
    // Welcome message
    kdesktop_terminal_write_string(win, "Terminal Ready!\n> ");
    
    return win;
}

// Create a new window
Window* kdesktop_create_window(int x, int y, int width, int height, const char* title, WindowType type) {
    Window* win = (Window*)kmalloc(sizeof(Window));
    if (!win) return NULL;
    
    win->x = x;
    win->y = y;
    win->width = width;
    win->height = height;
    str_copy(win->title, title, 64);
    win->title_color = COLOR_WINDOW_TITLE;
    win->border_color = COLOR_WINDOW_BORDER;
    win->bg_color = COLOR_WINDOW_BG;
    win->visible = 1;
    win->focused = 0;
    win->dragging = 0;
    win->drag_offset_x = 0;
    win->drag_offset_y = 0;
    win->type = type;
    win->terminal_buffer = NULL;
    win->terminal_cursor_x = 0;
    win->terminal_cursor_y = 0;
    win->terminal_buffer_size = 0;
    win->input_pos = 0;
    kmemset(win->input_buffer, 0, 128);
    
    // Add to linked list
    win->next = window_list;
    window_list = win;
    
    // Focus new window
    kdesktop_focus_window(win);
    
    return win;
}

// Focus a window
void kdesktop_focus_window(Window* win) {
    // Unfocus all windows
    Window* w = window_list;
    while (w) {
        w->focused = 0;
        w = w->next;
    }
    
    // Focus target
    if (win) {
        win->focused = 1;
        focused_window = win;
    }
}

void kdesktop_terminal_write_char(Window* win, char c) {
    if (!win || !win->terminal_buffer || win->type != WINDOW_TERMINAL) return;
    
    // Safety check
    if (win->terminal_buffer_size < (80 * 25)) return;
    
    // 1. Handle Newlines and Carriages
    if (c == '\n') {
        win->terminal_cursor_x = 0;
        win->terminal_cursor_y++;
    } else if (c == '\r') {
        win->terminal_cursor_x = 0;
    } 
    // 2. Handle Backspace
    else if (c == '\b') {
        if (win->terminal_cursor_x > 0) {
            win->terminal_cursor_x--;
            win->terminal_buffer[win->terminal_cursor_y * 80 + win->terminal_cursor_x] = ' ';
        }
    } 
    // 3. Handle Printable Characters
    else if (c >= 32 && c < 127) {
        // Calculate wrap threshold based on window width
        // We use a safe estimate: if current column * avg_width > window width
        int avg_char_width = 10; 
        if ((win->terminal_cursor_x * avg_char_width) > (win->width - 40)) {
            win->terminal_cursor_x = 0;
            win->terminal_cursor_y++;
        }

        if (win->terminal_cursor_y < 25) {
            int pos = win->terminal_cursor_y * 80 + win->terminal_cursor_x;
            win->terminal_buffer[pos] = c;
            win->terminal_cursor_x++;
            
            // Limit to grid width to prevent buffer overflow
            if (win->terminal_cursor_x >= 80) {
                win->terminal_cursor_x = 0;
                win->terminal_cursor_y++;
            }
        }
    }

    // 4. Scroll Logic (Unified for all cases)
    if (win->terminal_cursor_y >= 25) {
        for (int i = 0; i < 24; i++) {
            for (int j = 0; j < 80; j++) {
                win->terminal_buffer[i * 80 + j] = win->terminal_buffer[(i + 1) * 80 + j];
            }
        }
        for (int j = 0; j < 80; j++) {
            win->terminal_buffer[24 * 80 + j] = ' ';
        }
        win->terminal_cursor_y = 24;
    }
}

// Terminal: Write string
void kdesktop_terminal_write_string(Window* win, const char* str) {
    while (*str) {
        kdesktop_terminal_write_char(win, *str);
        str++;
    }
}

// Terminal: Render content
void kdesktop_terminal_render(Window* win) {
    if (!win || !win->terminal_buffer) return;
    
    uint32_t* back_buf = db_get_buffer();
    if (!back_buf) return;

    const int char_h = 16;
    const int off_x = 10;
    const int off_y = 45; // Start below the 35px header

    for (int row = 0; row < 25; row++) {
        int pen_x = off_x; // Initialize pen X at the start of each row

        for (int col = 0; col < 80; col++) {
            int idx = row * 80 + col;
            if (idx < 0 || idx >= win->terminal_buffer_size) break;

            char c = win->terminal_buffer[idx];
            if (c < 32 || c > 126) continue;

            // Get the REAL width of this specific glyph
            int char_w = (c == ' ') ? 5 : get_ttf_char_width(c); // Use 5px for space, or whatever suits your font

            // 3. Only draw if it's NOT a space
            if (c != ' ') {
                int dx = win->x + pen_x;
                int dy = win->y + off_y + (row * char_h);

                if (dx > 0 && (dx + char_w) < (int)ScreenWidth &&
                    dy > 0 && (dy + char_h) < (int)ScreenHeight &&
                    dx < (win->x + win->width - 5) &&
                    dy < (win->y + win->height - 5)) 
                {
                    ttf_put_char_buf(back_buf, ScreenWidth, dx, dy, c, ADWAITA_TEXT);
                }
            }

            // Calculate screen coordinates using dynamic pen_x
            //int dx = win->x + pen_x;
            //int dy = win->y + off_y + (row * char_h);

            // CRITICAL GUARD: Only draw if within screen AND window bounds
            /*if (dx > 0 && (dx + char_w) < (int)ScreenWidth &&
                dy > 0 && (dy + char_h) < (int)ScreenHeight &&
                dx < (win->x + win->width - 5) &&
                dy < (win->y + win->height - 5)) 
            {
                ttf_put_char_buf(back_buf, ScreenWidth, dx, dy, c, ADWAITA_TEXT);
            }*/

            // Move the pen forward by the width of the character + 1px spacing
            pen_x += char_w + 1;
        }
    }
}

// Draw a single window
void draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            // Logic to skip corners:
            // If we are in a corner area, check the distance from the corner center
            int dx = (i < r) ? r - i : (i > w - r) ? i - (w - r) : 0;
            int dy = (j < r) ? r - j : (j > h - r) ? j - (h - r) : 0;
            if (dx * dx + dy * dy <= r * r || (dx == 0 || dy == 0)) {
                db_put_pixel(x + i, y + j, color);
            }
        }
    }
}

void draw_rounded_rect_outline(int x, int y, int w, int h, int r, uint32_t color) {
    // A simple way is to use your existing draw_rounded_rect logic 
    // but only plot pixels if they are on the boundary. 
    // Or, for now, just use db_draw_rect for a quick test:
    db_draw_rect(x, y, w, h, color); 
}

void kdesktop_draw_window(Window* win) {
    if (!win) return; // Add this immediately
    if (!win->visible) return;
    
    // Check if the back buffer is actually ready
    if (db_get_buffer() == NULL) return;

    // Draw Shadow (Maybe skip this while dragging for speed?)
    if (!win->dragging) {
        for(int s = 3; s > 0; s--) { // Reduced shadow depth from 5 to 3
            draw_rounded_rect(win->x + s, win->y + s, win->width, win->height, 12, 0x101010);
        }
    }

    // Draw Window Body
    db_draw_rect_transparent(win->x, win->y, win->width, win->height, ADWAITA_WINDOW_BG, 240);

    // Draw the Title Bar
    db_draw_rect_transparent(win->x, win->y, win->width, 35, ADWAITA_HEADER, 255);

    // 3. The Highlight (The "Modern" touch)
    // Draw a 1-pixel thin white line at the very top of the window
    // with low alpha (e.g., 50) to simulate light hitting the edge.
    db_draw_rect_transparent(win->x, win->y, win->width, 1, 0xFFFFFF, 50);

    // In kdesktop_draw_window:
    uint32_t border = win->focused ? ADWAITA_ACCENT : ADWAITA_BORDER;

    // Draw a simple 1px border around the rect
    draw_rounded_rect_outline(win->x, win->y, win->width, win->height, 10, border);

        // Always draw title so we know what window it is
        int title_pen_x = 15; 
        for (int i = 0; win->title[i] != '\0'; i++) {
            char c = win->title[i];
            ttf_put_char_buf(db_get_buffer(), ScreenWidth, 
                            win->x + title_pen_x, 
                            win->y + 8, 
                            c, ADWAITA_TEXT);
            
            // Increment pen by actual glyph width instead of fixed (i * 10)
            title_pen_x += get_ttf_char_width(c) + 1; 
        }

    // Tell the engine this entire window space (plus shadow padding) changed
    db_mark_dirty(win->x, win->y, win->width + 5, win->height + 5);
}

// Draw taskbar
void kdesktop_draw_taskbar(void) {
    int taskbar_y = ScreenHeight - TASKBAR_HEIGHT;
    
    // Background
    db_fill_rect(0, taskbar_y, ScreenWidth, TASKBAR_HEIGHT, COLOR_TASKBAR);
    
    // Draw "start" area
    db_fill_rect(4, taskbar_y + 4, 100, TASKBAR_HEIGHT - 8, COLOR_WINDOW_TITLE);
    
    // Window buttons
    int x_offset = 110;
    Window* w = window_list;
    while (w) {
        if (w->visible) {
            uint32_t btn_color = w->focused ? COLOR_WINDOW_TITLE : 0x00404040;
            db_fill_rect(x_offset, taskbar_y + 4, 120, TASKBAR_HEIGHT - 8, btn_color);
            x_offset += 124;
        }
        w = w->next;
    }
}

// Draw desktop icons
void kdesktop_draw_icons(void) {
    //kprintf("[ICON] enter, icon_list=%p\n", icon_list);
    //kprintf("[ICON] buf=%p\n", db_get_buffer());
    
    DesktopIcon* icon = icon_list;
    while (icon) {
        //kprintf("[ICON] drawing: %s at %d,%d\n", icon->label, icon->x, icon->y);
        
        db_fill_rect(icon->x, icon->y, icon->width, icon->width, icon->color);
        //kprintf("[ICON] fill_rect done\n");
        
        if (icon->label[0] == 'T') {
            db_fill_rect(icon->x + 10, icon->y + 10, icon->width - 20, icon->width - 20, 0x00000000);
            db_draw_line(icon->x + 15, icon->y + 15, icon->x + 25, icon->y + 25, COLOR_TERMINAL_FG);
            //kprintf("[ICON] symbol done\n");
        }
        
        //kprintf("[ICON] measuring label\n");
        int label_width = 0;
        for(int i = 0; icon->label[i] != '\0'; i++) {
            //kprintf("[ICON] measuring char '%c'\n", icon->label[i]);
            label_width += get_ttf_char_width(icon->label[i]) + 1;
        }
        //kprintf("[ICON] label_width=%d\n", label_width);
        
        int pen_x = icon->x + (icon->width / 2) - (label_width / 2);
        
        for (int i = 0; icon->label[i] != '\0'; i++) {
            //kprintf("[ICON] drawing char '%c' at pen_x=%d\n", icon->label[i], pen_x);
            ttf_put_char_buf(db_get_buffer(), ScreenWidth,
                            pen_x, icon->y + icon->width + 5,
                            icon->label[i], ADWAITA_TEXT);
            pen_x += get_ttf_char_width(icon->label[i]) + 1;
        }
        //kprintf("[ICON] label drawn\n");
        
        icon = icon->next;
    }
    //kprintf("[ICON] done\n");
}

// Draw mouse cursor
void kdesktop_draw_cursor(int x, int y) {
    for (int i = 0; i < 16; i++) {
        db_draw_line(x, y + i, x + (16 - i), y + i, COLOR_TEXT_WHITE);
        if (i < 10) {
            db_put_pixel(x + (16 - i) + 1, y + i, 0x00000000);
        }
    }
}

void kdesktop_render(void) {
    db_clear(COLOR_DESKTOP_BG);
    
    kdesktop_draw_icons();
    
    Window* w = window_list;
    Window* stack[32];
    int count = 0;
    while (w && count < 32) {
        stack[count++] = w;
        w = w->next;
    }
    
    for (int i = count - 1; i >= 0; i--) {
        Window* win = stack[i];
        if (win != focused_window && win->visible) {
            kdesktop_draw_window(win);
            if (win->type == WINDOW_TERMINAL) {
                kdesktop_terminal_render(win);
            }
        }
    }
    
    if (focused_window && focused_window->visible) {
        kdesktop_draw_window(focused_window);
        if (focused_window->type == WINDOW_TERMINAL) {
            kdesktop_terminal_render(focused_window);
        }
    }
    
    kdesktop_draw_taskbar();
    kdesktop_draw_cursor(mouse_x, mouse_y);
    db_swap();
}

// Handle mouse events
void kdesktop_handle_mouse(OS_Event* e) {
    mouse_x = (int)e->data1;
    mouse_y = (int)e->data2;
    int buttons = (int)e->data3;
    int left_click = buttons & 0x01;
    
    // Handle window dragging
    if (focused_window && focused_window->dragging) {
        if (left_click) {
            focused_window->x = mouse_x - focused_window->drag_offset_x;
            focused_window->y = mouse_y - focused_window->drag_offset_y;
        } else {
            focused_window->dragging = 0;
        }
        return;
    }
    
    // Check for icon clicks
    if (left_click) {
        DesktopIcon* icon = icon_list;
        while (icon) {
            if (mouse_x >= icon->x && mouse_x < icon->x + icon->width &&
                mouse_y >= icon->y && mouse_y < icon->y + icon->height) {
                if (icon->on_click) {
                    icon->on_click();
                }
                return;
            }
            icon = icon->next;
        }
    }
    
    // Check for clicks on windows
// 3. Window Clicks
    if (left_click) {
        Window* w = window_list; // Ensure you check the list from front to back (Z-order)
        while (w) {
            if (w->visible) {
                // NEW: Title bar is now INSIDE the window rect (top 35 pixels)
                if (mouse_x >= w->x && mouse_x < w->x + w->width &&
                    mouse_y >= w->y && mouse_y < w->y + 35) {
                    
                    // Close button check (top right corner)
                    if (mouse_x >= w->x + w->width - 35) {
                        w->visible = 0;
                        return;
                    }
                    
                    // Start dragging
                    kdesktop_focus_window(w);
                    w->dragging = 1;
                    w->drag_offset_x = mouse_x - w->x;
                    w->drag_offset_y = mouse_y - w->y;
                    return;
                }
                
                // Click in the main body
                if (mouse_x >= w->x && mouse_x < w->x + w->width &&
                    mouse_y >= w->y && mouse_y < w->y + w->height) {
                    kdesktop_focus_window(w);
                    return;
                }
            }
            w = w->next;
        }
    }
}

// Handle keyboard events
// Forward declaration of your command processor
void process_command(char* cmd);

void kdesktop_exec_command(Window* win, char* cmd) {
    if (kstrcmp(cmd, "help") == 0) {
        kdesktop_terminal_write_string(win, "Desktop Cmds: help, clear, exit, heap\n");
    }
    else if (kstrcmp(cmd, "clear") == 0) {
        // Simple clear: wipe the buffer
        kmemset(win->terminal_buffer, 0, win->terminal_buffer_size);
        win->terminal_cursor_x = 0;
        win->terminal_cursor_y = 0;
    }
    else if (kstrcmp(cmd, "exit") == 0) {
        win->visible = 0; // Close window
    }
    else if (kstrcmp(cmd, "heap") == 0) {
        // Simple memory report
        char buf[64];
        uint64_t free_mem = pmm_get_free_pages() * 4096 / 1024 / 1024;
        // Assuming you have a version of sprintf (ksprintf/stb_sprintf)
        ksprintf(buf, "Free Memory: %d MB\n", free_mem);
        kdesktop_terminal_write_string(win, buf);
    }
    else if (kstrcmp(cmd, "mem") == 0) {
        char buf[64];
        uint64_t free_pages = pmm_get_free_pages();
        // Show exact pages to see even 4KB changes
        ksprintf(buf, "Free Pages: %d (approx %d MB)\n", free_pages, (free_pages * 4096) / 1024 / 1024);
        kdesktop_terminal_write_string(win, buf);
    }
    else if (cmd[0] != '\0') {
        kdesktop_terminal_write_string(win, "Unknown command.\n");
    }
}

void kdesktop_handle_keyboard(OS_Event* e) {
    char ascii = (char)e->data1;
    
    // ESC exits desktop
    if (ascii == 27) {
        desktop_running = 0;
        return;
    }
    
    if (focused_window && focused_window->type == WINDOW_TERMINAL) {
        // 1. Visual Echo (Keep this so you see what you type)
        kdesktop_terminal_write_char(focused_window, ascii);

        // 2. Command Capture Logic
        if (ascii == '\n') {
            // Null terminate the command
            focused_window->input_buffer[focused_window->input_pos] = '\0';
            
            // EXECUTE IT!
            // We need a special version of process_command that prints to THIS window
            kdesktop_exec_command(focused_window, focused_window->input_buffer);
            
            // Reset buffer for next command
            focused_window->input_pos = 0;
            focused_window->input_buffer[0] = '\0';
            
            // Print the prompt again
            kdesktop_terminal_write_string(focused_window, "> ");
            
        } else if (ascii == '\b') {
            // Handle backspace in buffer
            if (focused_window->input_pos > 0) {
                focused_window->input_pos--;
                focused_window->input_buffer[focused_window->input_pos] = '\0';
            }
        } else if (ascii >= 32 && ascii < 127) {
            // Add char to buffer if there is room
            if (focused_window->input_pos < 127) {
                focused_window->input_buffer[focused_window->input_pos++] = ascii;
            }
        }
    }
}


static veloxfs_handle g_fs;  // global so it survives past init
static veloxfs_io     g_io;  // already static but make it clear

void init_filesystem(void) {
    ram_disk_init();

    g_io.read      = veloxfs_read_adapter;
    g_io.write     = veloxfs_write_adapter;
    g_io.user_data = NULL;

    int status = veloxfs_mount(&g_fs, g_io);

    if (status == veloxfs_OK) {
        console_print("VeloxFS: Mounted successfully.\n");
        return;
    }

    kprintf("VeloxFS: Mount failed (status %d), formatting...\n", status);

    // 8MB disk = 16384 sectors
    veloxfs_format(g_io, 16384, 0);

    status = veloxfs_mount(&g_fs, g_io);
    if (status == veloxfs_OK) {
        console_print("VeloxFS: Formatted and mounted.\n");
    } else {
        kprintf("KERNEL PANIC: VeloxFS still failed after format (status %d)\n", status);
        while (1);
    }
}

// Main desktop loop
void kdesktop_run(void) {
    desktop_running = 1;
    int last_mouse_x = mouse_x;
    int last_mouse_y = mouse_y;
    //init_filesystem();

    while (desktop_running) {
        int needs_redraw = 0;
        int events_handled = 0;

        // DRAIN THE QUEUE: Process all waiting events first
        // Assuming pop_event returns a "NULL" or "EMPTY" type if queue is empty
        OS_Event e;
        while ((e = pop_event()).type != EVENT_NONE) { 
            events_handled = 1;

            if (e.type == EVENT_MOUSE) {
                kdesktop_handle_mouse(&e);
                // Check if mouse actually moved
                if (mouse_x != last_mouse_x || mouse_y != last_mouse_y) {
                    needs_redraw = 1;
                    last_mouse_x = mouse_x;
                    last_mouse_y = mouse_y;
                }
            } else if (e.type == EVENT_KEYBOARD) {
                kdesktop_handle_keyboard(&e);
                needs_redraw = 1;
            }
        }
        
        // 2. Only render ONCE after all current events are cleared
        if (needs_redraw) {
            kdesktop_render();
        }
        
        // 3. Prevent CPU "Meltdown"
        // If we didn't do anything, wait a bit. 
        // 10000 is very small; try 50000 for better stability in QEMU.
        if (!events_handled) {
            for (volatile int i = 0; i < 50000; i++);
        }
    }
}

// Shutdown desktop
void kdesktop_shutdown(void) {
    // Free all windows
    Window* w = window_list;
    while (w) {
        Window* next = w->next;
        if (w->terminal_buffer) kfree(w->terminal_buffer);
        kfree(w);
        w = next;
    }
    
    // Free all icons
    DesktopIcon* icon = icon_list;
    while (icon) {
        DesktopIcon* next = icon->next;
        kfree(icon);
        icon = next;
    }
    
    window_list = NULL;
    focused_window = NULL;
    icon_list = NULL;
}