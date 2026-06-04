#include "commands.h"
#include "console.h"
#include "memory.h"
#include "kstdlib.h"
#include "kstring.h"
#include "font_ttf.h"
#include "timer.h"
#include "pmm.h"
#include "double_buffer.h"
#include "kdesktop.h"

static char cmd_buffer[256];
static int cmd_ptr = 0;
static int char_widths[256]; // Stores the pixel width of each char in cmd_buffer

#define HISTORY_MAX 10
#define CMD_LEN 256

static char history[HISTORY_MAX][CMD_LEN];
static int history_count = 0;   // How many commands we've saved
static int history_index = -1;  // Where we are currently looking in the history

// Simple string comparison helper
int kstrcmp(char* s1, char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

void save_to_history(char* cmd) {
    if (cmd[0] == '\0') return; // Don't save empty lines

    // If it's the same as the last command, don't save it again (cleaner history)
    if (history_count > 0 && kstrcmp(cmd, history[(history_count - 1) % HISTORY_MAX]) == 0) {
        return;
    }

    // Copy the command into the history array
    // Since we don't have kstrcpy yet, we'll do a quick loop or use kmemcpy
    int i = 0;
    while (cmd[i] != '\0' && i < CMD_LEN - 1) {
        history[history_count % HISTORY_MAX][i] = cmd[i];
        i++;
    }
    history[history_count % HISTORY_MAX][i] = '\0';

    history_count++;
    history_index = history_count; // Reset history navigation
}

void kreboot_native() {
    uint8_t good = 0x02;
    while (good & 0x02) {
        // Wait for the input buffer to be empty
        __asm__ volatile ("inb $0x64, %0" : "=a"(good));
    }
    // Send the reset command to the controller
    __asm__ volatile ("outb %0, $0x64" : : "a"((uint8_t)0xFE));
}

void play_beep(uint32_t nFrequence) {
    uint32_t Div;
    uint8_t tmp;

    // Set the PIT to the desired frequency
    Div = 1193180 / nFrequence;
    __asm__ volatile ("outb %0, $0x43" : : "a"((uint8_t)0xB6));
    __asm__ volatile ("outb %0, $0x42" : : "a"((uint8_t)(Div & 0xFF)));
    __asm__ volatile ("outb %0, $0x42" : : "a"((uint8_t)((Div >> 8) & 0xFF)));

    // Actually turn the speaker on
    __asm__ volatile ("inb $0x61, %0" : "=a"(tmp));
    if (tmp != (tmp | 3)) {
        __asm__ volatile ("outb %0, $0x61" : : "a"((uint8_t)(tmp | 3)));
    }
}

void stop_beep() {
    uint8_t tmp;
    __asm__ volatile ("inb $0x61, %0" : "=a"(tmp));
    __asm__ volatile ("outb %0, $0x61" : : "a"((uint8_t)(tmp & 0xFC)));
}

void process_command(char* cmd) {
    save_to_history(cmd);
    if (kstrcmp(cmd, "help") == 0) {
        console_print("Commands: help, clear, version, reboot\n");
    } 
    else if (kstrcmp(cmd, "clear") == 0) {
       console_clear();
    }
    else if (kstrcmp(cmd, "version") == 0) {
        console_print("Lean Kernel v0.1 - Prompt Active\n");
    }
    else if (kstrcmp(cmd, "reboot") == 0) {
        // Standard UEFI Cold Reset
        //uefi_call_wrapper(RT->ResetSystem, 4, EfiResetCold, EFI_SUCCESS, 0, NULL);
        kreboot_native();
    }
    else if (kstrcmp(cmd, "malloc_test") == 0) {
        // 1. Request memory for a 50-character string
        char* dynamic_str = (char*)kmalloc(50);
        
        if (dynamic_str == NULL) {
            console_print("Malloc failed!\n");
        } else {
            // 2. Fill the memory manually (since we don't have strcpy yet)
            char* msg = "Dynamic Memory Working!";
            int i = 0;
            for (; msg[i] != '\0'; i++) {
                dynamic_str[i] = msg[i];
            }
            dynamic_str[i] = '\0';

            // 3. Print it
            console_print("Allocated at: ");
            // (Optional: you could use an itoa here to print the pointer address)
            console_print(dynamic_str);
            console_print("\n");

            // 4. Free it
            kfree(dynamic_str);
            console_print("Memory freed successfully.\n");
        }
    }
    else if (kstrcmp(cmd, "info") == 0) {
        char buf[128];
        // This would have been a nightmare to write manually:
        ksprintf(buf, "Res: %dx%d | FB: 0x%lx | Pitch: %d", 
                ScreenWidth, ScreenHeight, fb_base, PixelsPerScanLine);
        
        console_print(buf);
        console_print("\n");
    }
    else if (kstrcmp(cmd, "sys") == 0) {
        char* report = (char*)kmalloc(256); // Use your malloc!
        if (report) {
            // Use your new skyrocketed ksprintf!
            ksprintf(report, 
                "SYS REPORT:\n- Resolution: %dx%d\n- Pitch: %d pixels\n- Framebuffer: 0x%lx\n- Heap: Functional", 
                ScreenWidth, ScreenHeight, PixelsPerScanLine, fb_base);
            
            console_print(report);
            console_print("\n");
            kfree(report); // Always clean up!
        }
    }
    else if (kstrcmp(cmd, "uptime") == 0) {
        char buf[64];
        uint64_t ms = get_uptime_ms();
        uint64_t sec = ms / 1000;
        
        ksprintf(buf, "System Uptime: %d.%d seconds", sec, (ms % 1000) / 100);
        console_print(buf);
        console_print("\n");
    }
    else if (kstrcmp(cmd, "beep") == 0) {
        kprintf("Beep Test");
        play_beep(1);
        stop_beep();
    }
    else if (kstrcmp(cmd, "memory") == 0) {
        char buf[128];
        
        // 2. Use the functions pmm_get_total_pages() and pmm_get_free_pages()
        // Also, if PAGE_SIZE isn't in pmm.h, use 4096 directly or add it to pmm.h
        //ksprintf(buf, "Test: %d", 1048576 / 256); 
        //console_print(buf);

        uint64_t total_mb = pmm_get_total_pages() / 256; 
        uint64_t free_mb = pmm_get_free_pages() / 256;

        ksprintf(buf, "Memory: %d MB Total | %d MB Free", total_mb, free_mb);
        console_print(buf);
        console_print("\n");
    }
    else if (kstrcmp(cmd, "alloc_test") == 0) {
        // Allocate a 16KB buffer (4 pages)
        uint64_t addr = pmm_alloc_pages(4);
        
        if (addr == 0) {
            console_print("Allocation failed!\n");
        } else {
            char buf[128];
            ksprintf(buf, "Allocated 4 pages at 0x%lx\n", addr);
            console_print(buf);
            
            // You can write to this memory now!
            uint8_t* mem = (uint8_t*)addr;
            mem[0] = 0xDE;
            mem[1] = 0xAD;
            mem[2] = 0xBE;
            mem[3] = 0xEF;
            
            ksprintf(buf, "Wrote test pattern: %x %x %x %x\n", 
                    mem[0], mem[1], mem[2], mem[3]);
            console_print(buf);
            
            // Free it when done
            pmm_dealloc_pages(addr, 4);
            console_print("Pages freed!\n");
        }
    }
    else if (kstrcmp(cmd, "mem_stress") == 0) {
        char buf[64];
        uint64_t free_before = pmm_get_free_pages();
        
        // Allocate 10MB
        char* test = (char*)kmalloc(10 * 1024 * 1024);
        uint64_t free_after = pmm_get_free_pages();
        
        ksprintf(buf, "Allocated 10MB: %d pages used\n", free_before - free_after);
        console_print(buf);
        
        kfree(test);
        uint64_t free_final = pmm_get_free_pages();
        
        ksprintf(buf, "Freed: %d pages recovered\n", free_final - free_after);
        console_print(buf);
    }
    else if (kstrcmp(cmd, "gfx_test") == 0) {
        // Draw some shapes to test double buffering
        db_clear(0x001a1a2e); // Dark blue background
        
        // Draw some colorful rectangles
        db_fill_rect(100, 100, 200, 150, 0x00ff6b6b); // Red
        db_fill_rect(150, 150, 200, 150, 0x004ecdc4); // Cyan
        db_fill_rect(200, 200, 200, 150, 0x00ffe66d); // Yellow
        
        // Draw outline boxes
        db_draw_rect(50, 50, 300, 300, 0x00ffffff); // White border
        
        // Draw some lines
        db_draw_line(0, 0, ScreenWidth, ScreenHeight, 0x00ff00ff); // Magenta
        db_draw_line(ScreenWidth, 0, 0, ScreenHeight, 0x00ff00ff);
        
        // Make it visible!
        db_swap();
        
        console_print("Graphics test drawn! (screen won't update console until next swap)\n");
    }
    else if (kstrcmp(cmd, "anim_test") == 0) {
        console_print("Running animation (press ESC to stop - if you had ESC handling!)\n");
        
        // Simple bouncing box animation
        int x = 100, y = 100;
        int dx = 2, dy = 3;
        
        for (int frame = 0; frame < 500; frame++) { // 500 frames
            // Clear back buffer
            db_clear(0x001a1a2e);
            
            // Draw moving box
            db_fill_rect(x, y, 50, 50, 0x00ff6b6b);
            
            // Update position
            x += dx;
            y += dy;
            
            // Bounce off edges
            if (x <= 0 || x >= ScreenWidth - 50) dx = -dx;
            if (y <= 0 || y >= ScreenHeight - 50) dy = -dy;
            
            // Show the frame!
            db_swap();
            
            // Small delay (crude timing - you'd use your timer for real)
            for (volatile int i = 0; i < 100000; i++);
        }
        
        console_print("Animation complete!\n");
    }
    else if (kstrcmp(cmd, "mouse_test") == 0) {
        console_print("Mouse test - move mouse and watch coordinates\n");
        console_print("Press ESC to exit\n");
        
        for (int i = 0; i < 1000; i++) {
            OS_Event e = pop_event();
            
            if (e.type == EVENT_MOUSE) {
                char buf[64];
                ksprintf(buf, "Mouse: X=%d Y=%d Buttons=%d\n", 
                        (int)e.data1, (int)e.data2, (int)e.data3);
                console_print(buf);
            } else if (e.type == EVENT_KEYBOARD) {
                char ascii = (char)e.data1;
                if (ascii == 27) break; // ESC
            }
            
            // Small delay
            for (volatile int j = 0; j < 100000; j++);
        }
        
        console_print("Mouse test complete\n");
    }
    else if (kstrcmp(cmd, "desktop") == 0) {
        console_print("Starting desktop environment...\n");
        console_print("(Press ESC to return to shell)\n");
        
        // Small delay so user can read the message
        for (volatile int i = 0; i < 10000000; i++);
        
        // Initialize and run desktop
        init_filesystem();
        kdesktop_init();
        kdesktop_run();
        
        // Clean up when user exits
        kdesktop_shutdown();
        
        // Clear screen back to console
        db_clear(0x00000000);
        db_swap();
        
        console_print("\nReturned to shell.\n");
        console_print("> ");
    }
    else if (cmd[0] != '\0') {
        console_print("Unknown: ");
        console_print(cmd);
        console_print("\n");
    }

}


// Updated to accept both the character and the raw scancode
void handle_input(char c, uint8_t scancode) {
    // 1. Handle Special Keys (Scancodes) FIRST
   // 1. Handle History Navigation
    if (scancode == 0x48 || scancode == 0x50) { // UP (0x48) or DOWN (0x50)
        if (scancode == 0x48) { // UP
            if (history_index > 0 && history_index > (history_count > HISTORY_MAX ? history_count - HISTORY_MAX : 0)) {
                history_index--;
            } else { return; } // Boundary check
        } else { // DOWN
            if (history_index < history_count - 1) {
                history_index++;
            } else {
                // If we reach the bottom, clear the line for a fresh command
                while (cmd_ptr > 0) {
                    cmd_ptr--;
                    console_backspace_variable(char_widths[cmd_ptr]);
                }
                history_index = history_count;
                return;
            }
        }

        // Wipe current line visually
        while (cmd_ptr > 0) {
            cmd_ptr--;
            console_backspace_variable(char_widths[cmd_ptr]);
        }

        // Pull the selected command from history
        char* old_cmd = history[history_index % HISTORY_MAX];
        for (int i = 0; old_cmd[i] != '\0'; i++) {
            int w = get_ttf_char_width(old_cmd[i]) + 1;
            char_widths[cmd_ptr] = w;
            cmd_buffer[cmd_ptr++] = old_cmd[i];
            console_write_char(old_cmd[i]);
        }
        return; 
    }

    // 2. Handle Standard ASCII Keys
    if (c == '\n' || c == '\r') {
        console_write_char('\n');
        cmd_buffer[cmd_ptr] = '\0';
        
        process_command(cmd_buffer); //
        
        // Reset state for new line
        cmd_ptr = 0;
        for(int i = 0; i < 256; i++) char_widths[i] = 0; //
        console_print("> ");
    } 
    else if (c == '\b') {
        if (cmd_ptr > 0) {
            cmd_ptr--;
            int width_to_erase = char_widths[cmd_ptr]; //
            console_backspace_variable(width_to_erase); //
        }
    } 
    else if (cmd_ptr < 255 && c >= 32) {
        // Track width for the variable backspace
        int w = get_ttf_char_width(c) + 1; //
        char_widths[cmd_ptr] = w; //
        
        cmd_buffer[cmd_ptr++] = c;
        console_write_char(c);
    }
}