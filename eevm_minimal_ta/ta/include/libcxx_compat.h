/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Compatibility header for libcxx
 * Undefines OP-TEE macros that conflict with libcxx attribute parsing
 */

#ifndef LIBCXX_COMPAT_H
#define LIBCXX_COMPAT_H

// Undefine fallthrough macro before libcxx includes
// libcxx uses __has_attribute(fallthrough) which conflicts with OP-TEE's fallthrough macro
#ifdef fallthrough
#undef fallthrough
#endif

// Handle NULL redefinition warning
// GCC defines NULL as __null, musl defines it as 0L
// Both are equivalent, but we need to prevent the warning
// Save GCC's NULL definition before musl redefines it
#ifdef __null
// GCC's NULL is __null, which is fine
#elif defined(NULL)
// If NULL is already defined, undefine it so musl can define it consistently
#undef NULL
#endif

// Note: Cannot undefine __section as it's used by OP-TEE macros (__data, __bss, etc.)
// The __section conflict must be handled by include order or compiler flags

#endif /* LIBCXX_COMPAT_H */

