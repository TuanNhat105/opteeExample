/* Minimal alltypes.h for OP-TEE */
typedef unsigned long size_t;
typedef unsigned long uintptr_t;
typedef long ptrdiff_t;
typedef long ssize_t;
typedef long intptr_t;

typedef signed char int8_t;
typedef short       int16_t;
typedef int         int32_t;
typedef long long   int64_t;
typedef long long   intmax_t;

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef unsigned long long uintmax_t;

typedef unsigned wint_t;
typedef unsigned wchar_t;

typedef struct { unsigned __opaque; } mbstate_t;

typedef __builtin_va_list va_list;
typedef long time_t;

#define NULL ((void*)0)
