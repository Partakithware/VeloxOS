#include <stdarg.h> 
#include "memory.h"
#include "console.h"

#define STB_SPRINTF_IMPLEMENTATION
// Optional: If you want to use the shorter names, you can define this, 
// but it's safer to just use the library's official names.
#include "stb_sprintf.h"
// Tell stb_truetype to use your kernel memory
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x,u)  kmalloc(x)
#define STBTT_free(x,u)    kfree(x)

#include "stb_truetype.h"
#include <stdarg.h>


void ksprintf(char* buf, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    // CHANGE THIS LINE: Add 'sp' to the name
    stbsp_vsprintf(buf, fmt, args);
    
    va_end(args);
}

void kprintf(const char* fmt, ...) {
    char buf[512]; // Static buffer for quick printing
    va_list args;
    va_start(args, fmt);
    stbsp_vsprintf(buf, fmt, args);
    va_end(args);
    
    console_print(buf);
}

int snprintf(char* buf, size_t n, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = stbsp_vsnprintf(buf, (int)n, fmt, args);
    va_end(args);
    return ret;
}

// stb_truetype needs these to function
double pow(double x, double y) { return 0; } // Basic TTF doesn't use pow heavily
double sqrt(double x) {
    double res;
    __asm__ ("fsqrt" : "=t" (res) : "0" (x)); // Use x86 FPU for sqrt
    return res;
}
double ceil(double x) {
    int i = (int)x;
    return (x > i) ? i + 1 : i;
}
double floor(double x) {
    int i = (int)x;
    return (x < i) ? i - 1 : i;
}