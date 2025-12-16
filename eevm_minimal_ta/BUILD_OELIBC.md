# Building Complete OpenEnclave libc for OP-TEE TA

## Overview

This document explains how to build and use a complete OpenEnclave-based musl libc in OP-TEE Trusted Applications, replacing custom stubs with real standard library implementations.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                     TA Application                       │
│                  (minimal_evm_ta.cpp)                    │
└─────────────────────────────────────────────────────────┘
                          │
        ┌─────────────────┼─────────────────┐
        │                 │                 │
        ▼                 ▼                 ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│   libcxx     │  │  liboelibc   │  │  cxx_stubs   │
│ (OpenEnclave)│  │(musl+stubs)  │  │  (minimal)   │
└──────────────┘  └──────────────┘  └──────────────┘
        │                 │
        │         ┌───────┴───────┐
        │         │               │
        ▼         ▼               ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│   libcxxrt   │  │  musl libc   │  │minimal_stubs │
│  (exception) │  │(615 objects) │  │(errno,lock)  │
└──────────────┘  └──────────────┘  └──────────────┘
```

## What Gets Built

### 1. **libmusl_complete.a** (1.5MB, 615 objects)
Complete musl libc implementation from OpenEnclave's source list:
- **Math**: All floating point functions (sin, cos, exp, log, sqrt, etc.)
- **String**: Complete string manipulation (strcpy, strcmp, strlen, etc.)
- **Stdlib**: Conversion functions (strtol, strtod, atoi, etc.)
- **Stdio**: File operations (printf, scanf, fopen, etc.)
- **Ctype**: Character classification (isalpha, isdigit, etc.)
- **Time**: Time/date functions (strftime, mktime, etc.)
- **Locale**: Locale support
- **Complex**: Complex number math

### 2. **minimal_stubs.c** (4.7KB, 1 object)
Minimal implementations for functions that musl depends on but aren't needed in TA:
- **Threading**: `__lockfile`, `__unlockfile`, `__lock`, `__unlock`, `__ofl_lock`, `__ofl_unlock`
  - Implementation: No-ops (TA is single-threaded)
- **Wide char**: `mbtowc`, `fputwc`, `btowc`, `fwide`
  - Implementation: ASCII-only stubs
- **Error strings**: `strerror`
  - Implementation: Simple error messages
- **errno**: `__errno_location`, `___errno_location`
  - Implementation: Thread-local errno variable

### 3. **liboelibc.a** (1.5MB, 616 objects)
Final combined library = musl + minimal_stubs

## Build Process

### Step 1: Build liboelibc.a

```bash
cd /home/abc/nhat/optee_examples/eevm_minimal_ta
./build_oelibc_complete.sh
```

This script:
1. Extracts source list from OpenEnclave's CMakeLists.txt
2. Compiles 666 musl source files (615 succeed, 33 fail, 18 not found)
3. Creates `minimal_stubs.c` with required helper functions
4. Combines everything into `liboelibc.a`

**Output:**
```
build_oelibc_complete/
├── libmusl_complete.a          # 615 musl objects only
├── liboelibc.a                 # 616 objects (musl + stubs) ← USE THIS
├── minimal_stubs.c             # Source for helper stubs
├── minimal_stubs.o             # Compiled stubs
├── musl_*.o                    # 615 individual musl objects
└── musl_*.log                  # Build logs for debugging
```

### Step 2: Link with TA

The `ta/Makefile` is already configured to link `liboelibc.a`:

```makefile
# Complete OpenEnclave libc (musl + wrappers)
LIBOELIBC := ../build_oelibc_complete/liboelibc.a

# Link order: objs + libcxx + liboelibc + libgcc
$(BINARY).elf: $(objs) $(LIBCXX_OBJS) $(LIBOELIBC) ...
	$(LDta_arm64) ... $(LIBCXX_OBJS) $(LIBOELIBC) $(LIBGCC) -o $@
```

### Step 3: Build TA

```bash
./build.sh
```

**Result:** `ta/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta` (374KB)

## What Changed from Stubs

### Before (with stubs in cxx_stubs.cpp):
```cpp
extern "C" {
long strtol(const char* str, char** endptr, int base) {
    if (endptr) *endptr = (char*)str;
    errno = 22; // EINVAL
    return 0;  // Always return 0 - WRONG!
}

double strtod(const char* str, char** endptr) {
    if (endptr) *endptr = (char*)str;
    errno = 22; // EINVAL
    return 0.0;  // Always return 0 - WRONG!
}

static int g_errno = 0;
int* __errno_location(void) {
    return &g_errno;
}
}
```

### After (with real musl implementations):
```cpp
// All removed! Real implementations from musl:
// - strtol/strtod: Full parsing with error handling
// - errno: Proper errno infrastructure
// - malloc/free: OpenEnclave wrappers
// Only minimal exception stubs remain
```

## Symbol Verification

Verify that TA uses real musl functions:

```bash
cd ta
aarch64-none-linux-gnu-nm 8aaaf200-2450-11e4-abe2-0002a5d5c51b.elf | \
    grep -E " (strtol|strtod|malloc|free|__errno_location)$"
```

**Expected output:**
```
00000000000458a4 t ___errno_location  ← 3 underscores (OpenEnclave)
0000000000045888 t __errno_location   ← 2 underscores (standard)
0000000000012a24 T free
000000000001292c T malloc
0000000000045d88 T strtod              ← Real musl implementation!
0000000000045f1c T strtol              ← Real musl implementation!
```

## Key Differences: OpenEnclave vs Standard musl

### errno Location
- **Standard musl**: `__errno_location()` (2 underscores)
- **OpenEnclave musl**: `___errno_location()` (3 underscores)
- **Our solution**: Provide both versions pointing to same variable

### Threading
- **Standard musl**: Uses pthread infrastructure
- **OpenEnclave**: Stubs pthread (SGX/TrustZone limitations)
- **Our solution**: Simple no-op locks (TA is single-threaded)

### stdio/FILE*
- **Standard musl**: Full stdio with file descriptors
- **OpenEnclave**: Limited stdio support
- **Our solution**: Link stdio but it's unused (TA doesn't do file I/O)

## What Works

✅ **String conversions**: `strtol`, `strtod`, `strtof`, `strtold`, `atoi`, `atol`, `atoll`
✅ **errno handling**: Proper error reporting in string conversion
✅ **Math functions**: Complete math library available
✅ **String operations**: Full string manipulation
✅ **Character classification**: All ctype functions
✅ **Memory allocation**: OpenEnclave's malloc/free wrappers

## What Doesn't Work (and we don't need)

❌ **File I/O**: stdio functions present but won't work (no file system in TA)
❌ **Threading**: pthread functions are stubs (TA is single-threaded)
❌ **Locale**: Limited locale support (ASCII only)
❌ **Wide char**: Minimal wide character support

## Size Impact

| Component | Size | Description |
|-----------|------|-------------|
| liboelibc.a | 1.5MB | Complete library (not all linked) |
| TA .elf size increase | +48KB | Only used functions linked (326KB → 374KB) |
| Actual cost | ~13% | Reasonable for real implementations |

## Rebuild from Scratch

If you need to rebuild everything:

```bash
# Clean all built objects
rm -rf build_oelibc_complete

# Rebuild library
./build_oelibc_complete.sh

# Rebuild TA
./build.sh

# Deploy to Pi
scp -O ta/*.ta root@192.168.1.203:/lib/optee_armtz/
scp -O host/minimal_evm_host root@192.168.1.203:/usr/bin/
```

## Debugging

### Build Errors
Check individual object logs:
```bash
cd build_oelibc_complete
cat musl_stdlib_strtol.log   # Check why strtol failed to build
cat musl_stdio___lockfile.log # Check threading issues
```

### Linker Errors
If you see "undefined reference to X":
1. Check if symbol exists in library:
   ```bash
   nm liboelibc.a | grep " T symbol_name"
   ```
2. Add stub to `minimal_stubs.c` if needed
3. Rebuild: `./build_oelibc_complete.sh`

### Runtime Errors
Enable TA debug output:
```c
DMSG("strtol result: %ld", strtol("123", NULL, 10));
DMSG("errno: %d", errno);
```

## Why This Approach?

1. **No manual stub maintenance**: Real implementations from musl
2. **Correct behavior**: String conversion actually works
3. **Future-proof**: Easy to add more musl functions
4. **Minimal overhead**: Only used functions get linked (~48KB)
5. **Proper errno**: Error handling works correctly

## Credits

- **OpenEnclave SDK**: Source of musl integration approach
- **musl libc**: Lightweight, correct C standard library
- **OP-TEE**: Trusted Execution Environment framework

## References

- OpenEnclave libc: `external/openenclave/libc/CMakeLists.txt`
- musl source: `build_oe_libs/musl/src/src/`
- Build script: `build_oelibc_complete.sh`
- TA Makefile: `ta/Makefile`
