#!/bin/bash
# Add missing math functions to musl math.h for libcxx compatibility
# libcxx needs these as functions (not just macros) for 'using ::signbit;' etc.
set -e

MUSL_MATH_H="$1"

if [ ! -f "$MUSL_MATH_H" ]; then
    echo "Usage: $0 <path/to/musl/include/math.h>"
    exit 1
fi

echo "Adding missing math functions to: $MUSL_MATH_H"

# Check if already patched
if grep -q "__LIBCXX_MATH_FUNCTIONS__" "$MUSL_MATH_H"; then
    echo "Already patched! Skipping."
    exit 0
fi

# Backup original
cp "$MUSL_MATH_H" "$MUSL_MATH_H.backup"

# Find insertion point: after the macro definitions, before function declarations
# Insert right after isgreaterequal macro (line ~121) and before acos declarations

# Create a temporary file with the function implementations
TMP_PATCH=$(mktemp /tmp/math_patch.XXXXXX)
cat > "$TMP_PATCH" <<'MATH_PATCH_EOF'

/* Inline function implementations for libcxx compatibility */
/* libcxx needs these as functions for using ::signbit; etc. */
#ifndef __LIBCXX_MATH_FUNCTIONS__
#define __LIBCXX_MATH_FUNCTIONS__

#ifdef __cplusplus
extern "C" {
#endif

/* Undefine macros first to avoid conflicts, then define functions */
#ifdef signbit
#undef signbit
#endif
#ifdef fpclassify
#undef fpclassify
#endif
#ifdef isfinite
#undef isfinite
#endif
#ifdef isinf
#undef isinf
#endif
#ifdef isnan
#undef isnan
#endif
#ifdef isnormal
#undef isnormal
#endif
#ifdef isgreater
#undef isgreater
#endif
#ifdef isgreaterequal
#undef isgreaterequal
#endif
#ifdef isless
#undef isless
#endif
#ifdef islessequal
#undef islessequal
#endif
#ifdef islessgreater
#undef islessgreater
#endif
#ifdef isunordered
#undef isunordered
#endif

/* Provide inline functions that use musl's internal implementations */
/* These work even when the macros are undefined */
static __inline int signbit(double x) { return (int)(__DOUBLE_BITS(x)>>63); }
static __inline int signbitf(float x) { return (int)(__FLOAT_BITS(x)>>31); }
static __inline int signbitl(long double x) { return __signbitl(x); }

static __inline int fpclassify(double x) { return __fpclassify(x); }
static __inline int fpclassifyf(float x) { return __fpclassifyf(x); }
static __inline int fpclassifyl(long double x) { return __fpclassifyl(x); }

static __inline int isfinite(double x) { return (__DOUBLE_BITS(x) & -1ULL>>1) < 0x7ffULL<<52; }
static __inline int isfinitef(float x) { return (__FLOAT_BITS(x) & 0x7fffffff) < 0x7f800000; }
static __inline int isfinitel(long double x) { return __fpclassifyl(x) > FP_INFINITE; }

static __inline int isinf(double x) { return (__DOUBLE_BITS(x) & -1ULL>>1) == 0x7ffULL<<52; }
static __inline int isinff(float x) { return (__FLOAT_BITS(x) & 0x7fffffff) == 0x7f800000; }
static __inline int isinfl(long double x) { return __fpclassifyl(x) == FP_INFINITE; }

static __inline int isnan(double x) { return (__DOUBLE_BITS(x) & -1ULL>>1) > 0x7ffULL<<52; }
static __inline int isnanf(float x) { return (__FLOAT_BITS(x) & 0x7fffffff) > 0x7f800000; }
static __inline int isnanl(long double x) { return __fpclassifyl(x) == FP_NAN; }

static __inline int isnormal(double x) { return ((__DOUBLE_BITS(x)+(1ULL<<52)) & -1ULL>>1) >= 1ULL<<53; }
static __inline int isnormalf(float x) { return ((__FLOAT_BITS(x)+0x00800000) & 0x7fffffff) >= 0x01000000; }
static __inline int isnormall(long double x) { return __fpclassifyl(x) == FP_NORMAL; }

static __inline int isgreater(double x, double y) { return !((__DOUBLE_BITS(x) & -1ULL>>1) > 0x7ffULL<<52 || (__DOUBLE_BITS(y) & -1ULL>>1) > 0x7ffULL<<52) && x > y; }
static __inline int isgreaterf(float x, float y) { return !((__FLOAT_BITS(x) & 0x7fffffff) > 0x7f800000 || (__FLOAT_BITS(y) & 0x7fffffff) > 0x7f800000) && x > y; }
static __inline int isgreaterl(long double x, long double y) { return !isnanl(x) && !isnanl(y) && x > y; }

static __inline int isgreaterequal(double x, double y) { return !((__DOUBLE_BITS(x) & -1ULL>>1) > 0x7ffULL<<52 || (__DOUBLE_BITS(y) & -1ULL>>1) > 0x7ffULL<<52) && x >= y; }
static __inline int isgreaterequalf(float x, float y) { return !((__FLOAT_BITS(x) & 0x7fffffff) > 0x7f800000 || (__FLOAT_BITS(y) & 0x7fffffff) > 0x7f800000) && x >= y; }
static __inline int isgreaterequall(long double x, long double y) { return !isnanl(x) && !isnanl(y) && x >= y; }

static __inline int isless(double x, double y) { return !((__DOUBLE_BITS(x) & -1ULL>>1) > 0x7ffULL<<52 || (__DOUBLE_BITS(y) & -1ULL>>1) > 0x7ffULL<<52) && x < y; }
static __inline int islessf(float x, float y) { return !((__FLOAT_BITS(x) & 0x7fffffff) > 0x7f800000 || (__FLOAT_BITS(y) & 0x7fffffff) > 0x7f800000) && x < y; }
static __inline int islessl(long double x, long double y) { return !isnanl(x) && !isnanl(y) && x < y; }

static __inline int islessequal(double x, double y) { return !((__DOUBLE_BITS(x) & -1ULL>>1) > 0x7ffULL<<52 || (__DOUBLE_BITS(y) & -1ULL>>1) > 0x7ffULL<<52) && x <= y; }
static __inline int islessequalf(float x, float y) { return !((__FLOAT_BITS(x) & 0x7fffffff) > 0x7f800000 || (__FLOAT_BITS(y) & 0x7fffffff) > 0x7f800000) && x <= y; }
static __inline int islessequall(long double x, long double y) { return !isnanl(x) && !isnanl(y) && x <= y; }

static __inline int islessgreater(double x, double y) { return !((__DOUBLE_BITS(x) & -1ULL>>1) > 0x7ffULL<<52 || (__DOUBLE_BITS(y) & -1ULL>>1) > 0x7ffULL<<52) && x != y; }
static __inline int islessgreaterf(float x, float y) { return !((__FLOAT_BITS(x) & 0x7fffffff) > 0x7f800000 || (__FLOAT_BITS(y) & 0x7fffffff) > 0x7f800000) && x != y; }
static __inline int islessgreaterl(long double x, long double y) { return !isnanl(x) && !isnanl(y) && x != y; }

static __inline int isunordered(double x, double y) { return ((__DOUBLE_BITS(x) & -1ULL>>1) > 0x7ffULL<<52) ? 1 : ((__DOUBLE_BITS(y) & -1ULL>>1) > 0x7ffULL<<52); }
static __inline int isunorderedf(float x, float y) { return ((__FLOAT_BITS(x) & 0x7fffffff) > 0x7f800000) ? 1 : ((__FLOAT_BITS(y) & 0x7fffffff) > 0x7f800000); }
static __inline int isunorderedl(long double x, long double y) { return isnanl(x) ? 1 : isnanl(y); }

#ifdef __cplusplus
}
#endif

#endif /* __LIBCXX_MATH_FUNCTIONS__ */
MATH_PATCH_EOF

# Insert the patch after isgreaterequal macro
sed -i '/^#define isgreaterequal/r '"$TMP_PATCH" "$MUSL_MATH_H"
rm -f "$TMP_PATCH"

echo "Done! Backup saved to: $MUSL_MATH_H.backup"
