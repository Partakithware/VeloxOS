#ifndef KDESKTOP_H
#define KDESKTOP_H

#include <stdint.h>
#include "event.h"

// Forward declarations
typedef struct Window Window;
typedef struct DesktopIcon DesktopIcon;

// Window types
typedef enum {
    WINDOW_NORMAL,
    WINDOW_TERMINAL
} WindowType;

// Window structure
struct Window {
    int x, y;
    int width, height;
    char title[64];
    uint32_t title_color;
    uint32_t border_color;
    uint32_t bg_color;
    int visible;
    int focused;
    int dragging;
    int drag_offset_x;
    int drag_offset_y;
    WindowType type;
    // NEW: Command Line Buffer
    char input_buffer[128]; 
    int input_pos;
    
    // Terminal-specific data
    char* terminal_buffer;      // Text content
    int terminal_cursor_x;      // Character position
    int terminal_cursor_y;      // Line number
    int terminal_buffer_size;
    
    Window* next;
};

// Desktop icon structure
struct DesktopIcon {
    int x, y;
    int width, height;
    char label[32];
    uint32_t color;
    void (*on_click)(void);  // Callback when clicked
    DesktopIcon* next;
};

// Desktop configuration
#define TASKBAR_HEIGHT 32
#define WINDOW_TITLE_HEIGHT 24
#define WINDOW_BORDER_WIDTH 2
#define ICON_SIZE 64
#define ICON_SPACING 20

// Color scheme (modern dark theme)
#define COLOR_DESKTOP_BG    0x002b2d31
#define COLOR_TASKBAR       0x001e1f22
#define COLOR_WINDOW_BG     0x00313338
#define COLOR_WINDOW_TITLE  0x005865f2
#define COLOR_WINDOW_BORDER 0x00040405
#define COLOR_TEXT_WHITE    0x00ffffff
#define COLOR_TEXT_GRAY     0x00b5bac1
#define COLOR_TERMINAL_BG   0x001a1a1a
#define COLOR_TERMINAL_FG   0x00c0c0c0
// Theme
#define ADWAITA_BG            0x1E1E1E  // Dark charcoal desktop
#define ADWAITA_WINDOW_BG     0x2D2D2D  // Lighter window body
#define ADWAITA_HEADER        0x353535  // Header bar
#define ADWAITA_BORDER        0x454545  // Soft border
#define ADWAITA_ACCENT        0x3584E4  // Adwaita Blue (for active elements)
#define ADWAITA_TEXT          0xEEEEEE  // Off-white text

// Initialize desktop environment
void kdesktop_init(void);

// Main desktop event loop (runs forever)
void kdesktop_run(void);

// Window management
Window* kdesktop_create_window(int x, int y, int width, int height, const char* title, WindowType type);
Window* kdesktop_create_terminal(int x, int y, int width, int height);
void kdesktop_destroy_window(Window* win);
void kdesktop_focus_window(Window* win);

// Desktop icon management
DesktopIcon* kdesktop_create_icon(int x, int y, const char* label, void (*callback)(void));

// Desktop rendering
void kdesktop_render(void);
void kdesktop_draw_window(Window* win);
void kdesktop_draw_taskbar(void);
void kdesktop_draw_cursor(int x, int y);
void kdesktop_draw_icons(void);

// Terminal functions
void kdesktop_terminal_write_char(Window* win, char c);
void kdesktop_terminal_write_string(Window* win, const char* str);
void kdesktop_terminal_render(Window* win);

// Event handlers
void kdesktop_handle_mouse(OS_Event* e);
void kdesktop_handle_keyboard(OS_Event* e);

// Shutdown desktop
void kdesktop_shutdown(void);

#endif