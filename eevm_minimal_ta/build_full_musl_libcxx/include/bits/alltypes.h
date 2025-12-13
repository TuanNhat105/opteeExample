#ifndef _BITS_ALLTYPES_H
#define _BITS_ALLTYPES_H

/* Minimal alltypes.h for OP-TEE - C++ compatible */
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
/* wchar_t is C++ builtin, don't redefine */
#ifndef __cplusplus
typedef unsigned wchar_t;
#endif

typedef struct __mbstate_t { unsigned __opaque; } mbstate_t;

typedef __builtin_va_list va_list;
typedef long time_t;

/* FILE type for stdio */
struct _IO_FILE;
typedef struct _IO_FILE FILE;

typedef __builtin_va_list __isoc_va_list;

typedef long long off_t;

/* locale_t stub */
typedef void* locale_t;

/* wctype_t */
typedef unsigned long wctype_t;

#define NULL ((void*)0)

#endif /* _BITS_ALLTYPES_H */
