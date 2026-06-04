// stdint.h - Standard integer types for freestanding environment
#ifndef _STDINT_H
#define _STDINT_H

typedef signed char        int8_t;
typedef unsigned char      uint8_t;
typedef signed short       int16_t;
typedef unsigned short     uint16_t;
typedef signed int         int32_t;
typedef unsigned int       uint32_t;
typedef signed long long   int64_t;
typedef unsigned long long uint64_t;

typedef unsigned long      size_t;
typedef long               ssize_t;
typedef long               ptrdiff_t;

#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL

#define NULL ((void*)0)

#endif // _STDINT_H