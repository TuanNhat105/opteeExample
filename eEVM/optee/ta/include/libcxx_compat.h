/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Compatibility header to resolve conflicts between libcxx and TEE headers
 */
#ifndef LIBCXX_COMPAT_H
#define LIBCXX_COMPAT_H

/* Undefine macros that conflict between musl/libcxx and TEE headers */
#ifdef __section
#undef __section
#endif

#ifdef __aligned
#undef __aligned
#endif

#ifdef __packed
#undef __packed
#endif

#endif /* LIBCXX_COMPAT_H */
