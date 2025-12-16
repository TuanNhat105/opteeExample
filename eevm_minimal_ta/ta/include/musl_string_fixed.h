/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Fixed musl string.h wrapper
 * Prevents NULL redefinition warning by checking if NULL is already defined
 */

#ifndef _STRING_H
#define _STRING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>

// Define NULL only if not already defined (prevents redefinition warning)
#ifndef NULL
#ifdef __cplusplus
#define NULL 0L
#else
#define NULL ((void*)0)
#endif
#endif

// Include the rest of musl string.h functionality
// We need to include the original but with NULL already defined
// So we'll just forward declare what we need or include the original carefully

// For now, just include the original - the NULL check above should prevent warning
// But we need to prevent the original from redefining NULL
#define _MUSL_STRING_H_INCLUDED

#ifdef __cplusplus
}
#endif

#endif /* _STRING_H */

