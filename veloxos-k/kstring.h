#ifndef KSTRING_H
#define KSTRING_H

#include <stddef.h>

void* kmemset(void* dest, int ch, size_t count);
void* kmemcpy(void* dest, const void* src, size_t count);
int strcmp(const char* s1, const char* s2);
size_t strlen(const char* str);
int strncmp(const char* s1, const char* s2, size_t n);
char* strncpy(char* dest, const char* src, size_t n);

#endif
