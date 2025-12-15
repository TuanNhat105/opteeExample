#ifndef _BITS_ALLTYPES_H
#define _BITS_ALLTYPES_H

/* Use OP-TEE compatible types (arm64: long = 64-bit) */
#ifndef __SIZE_TYPE__
typedef unsigned long size_t;
#endif

#ifndef __PTRDIFF_TYPE__
typedef long ptrdiff_t;
#endif

typedef long ssize_t;

#ifndef __UINTPTR_TYPE__
typedef unsigned long uintptr_t;
#endif

#ifndef __INTPTR_TYPE__
typedef long intptr_t;
#endif

/* Standard integer types - ARM64 uses long for 64-bit */
#ifndef __INT8_TYPE__
typedef signed char int8_t;
#endif

#ifndef __INT16_TYPE__
typedef short int16_t;
#endif

#ifndef __INT32_TYPE__
typedef int int32_t;
#endif

#ifndef __INT64_TYPE__
typedef long int64_t;  /* ARM64: long = 64-bit */
#endif

typedef long intmax_t;

#ifndef __UINT8_TYPE__
typedef unsigned char uint8_t;
#endif

#ifndef __UINT16_TYPE__
typedef unsigned short uint16_t;
#endif

#ifndef __UINT32_TYPE__
typedef unsigned int uint32_t;
#endif

#ifndef __UINT64_TYPE__
typedef unsigned long uint64_t;  /* ARM64: unsigned long = 64-bit */
#endif

typedef unsigned long uintmax_t;

typedef unsigned wint_t;
#ifndef __cplusplus
typedef unsigned wchar_t;
#endif

typedef struct __mbstate_t { unsigned __opaque; } mbstate_t;
typedef __builtin_va_list va_list;
typedef __builtin_va_list __isoc_va_list;
typedef long time_t;
typedef long off_t;
typedef void* locale_t;
typedef unsigned long wctype_t;

struct _IO_FILE;
typedef struct _IO_FILE FILE;

#ifndef NULL
#define NULL ((void*)0)
#endif

#endif
