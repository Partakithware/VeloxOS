#ifndef KSTDLIB_H
#define KSTDLIB_H

void ksprintf(char* buf, const char* fmt, ...);
void kprintf(const char* fmt, ...);
int snprintf(char* buf, size_t n, const char* fmt, ...);

double pow(double x, double y);
double sqrt(double x);
double ceil(double x);
double floor(double x);

#endif
