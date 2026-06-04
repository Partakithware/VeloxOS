#include <stddef.h>

// Fills memory with a byte (Skyrockets screen clearing and buffer resets)
void* kmemset(void* dest, int ch, size_t count) {
    unsigned char* p = (unsigned char*)dest;
    while (count--) {
        *p++ = (unsigned char)ch;
    }
    return dest;
}

// Copies memory (Essential for window movement and command history)
void* kmemcpy(void* dest, const void* src, size_t count) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (count--) {
        *d++ = *s++;
    }
    return dest;
}

// Professional string comparison (To replace your manual kstrcmp)
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return (unsigned char)s1[i] - (unsigned char)s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) dest[i] = src[i];
    for ( ; i < n; i++) dest[i] = '\0';
    return dest;
}