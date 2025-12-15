#!/bin/bash
# Add missing math functions to musl math.h for libcxx compatibility
set -e

MUSL_MATH_H="$1"

if [ ! -f "$MUSL_MATH_H" ]; then
    echo "Usage: $0 <path/to/musl/include/math.h>"
    exit 1
fi

echo "Adding missing math functions to: $MUSL_MATH_H"

# Check if already patched
if grep -q "__LIBCXX_MATH_COMPAT__" "$MUSL_MATH_H"; then
    echo "Already patched! Skipping."
    exit 0
fi

# Backup original
cp "$MUSL_MATH_H" "$MUSL_MATH_H.backup"

# Find what's missing by checking if musl already has these
HAS_SIGNBIT=$(grep -c "^#define signbit" "$MUSL_MATH_H" || echo "0")
HAS_ISGREATER=$(grep -c "^#define isgreater" "$MUSL_MATH_H" || echo "0")

if [ "$HAS_SIGNBIT" -gt 0 ] && [ "$HAS_ISGREATER" -gt 0 ]; then
    echo "Musl already has all required math functions!"
    exit 0
fi

# Only add what's missing
cat >> "$MUSL_MATH_H" <<'EOF'

/* Additional math comparison functions for libcxx */
#ifndef __LIBCXX_MATH_COMPAT__
#define __LIBCXX_MATH_COMPAT__

#ifndef isgreater
#define isgreater(x, y) (!isunordered(x, y) && (x) > (y))
#endif

#ifndef isgreaterequal
#define isgreaterequal(x, y) (!isunordered(x, y) && (x) >= (y))
#endif

#ifndef isless
#define isless(x, y) (!isunordered(x, y) && (x) < (y))
#endif

#ifndef islessequal
#define islessequal(x, y) (!isunordered(x, y) && (x) <= (y))
#endif

#ifndef islessgreater
#define islessgreater(x, y) (!isunordered(x, y) && ((x) < (y) || (x) > (y)))
#endif

#ifndef isunordered
#define isunordered(x, y) (isnan(x) || isnan(y))
#endif

#endif /* __LIBCXX_MATH_COMPAT__ */
EOF

echo "Done! Backup saved to: $MUSL_MATH_H.backup"
