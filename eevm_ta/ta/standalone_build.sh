#!/bin/bash
# Standalone build script for eEVM TA
# Bypasses OP-TEE build system restrictions

set -e

TOOLCHAIN=~/toolchain/bin/aarch64-none-linux-gnu
CC="${TOOLCHAIN}-gcc"
CXX="${TOOLCHAIN}-g++"
LD="${TOOLCHAIN}-ld"
OBJCOPY="${TOOLCHAIN}-objcopy"

# OP-TEE paths
TA_DEV_KIT=/home/abc/optee_os/out/arm-plat-rpi5/export-ta_arm64
EEVM_DIR=/home/abc/eEVM

# TA UUID
TA_UUID="a1b2c3d4-e5f6-4789-a1b2-c3d4e5f67890"

echo "=== Building eEVM TA with full C++ support ==="

# Step 1: Compile TA with full sysroot
echo "[1] Compiling eevm_ta.cpp..."
${CXX} \
    -fpic -Os -g \
    -std=c++17 \
    -fno-rtti -fno-threadsafe-statics \
    -DARM64=1 -D__LP64__=1 -DTRACE_LEVEL=4 \
    -DCFG_TEE_TA_LOG_LEVEL=4 \
    -I./include \
    -I${TA_DEV_KIT}/include \
    -I${EEVM_DIR}/include \
    -I${EEVM_DIR}/3rdparty \
    -I${EEVM_DIR}/3rdparty/intx/include \
    -D__FILE_ID__=eevm_ta_cpp \
    -c eevm_ta.cpp -o eevm_ta.o

echo "[2] Linking TA with eEVM library..."
${CXX} \
    -shared -fpic \
    -Wl,-T,${TA_DEV_KIT}/src/ta.ld.S \
    -Wl,-Map=${TA_UUID}.map \
    -Wl,--sort-section=alignment \
    -Wl,-z,max-page-size=4096 \
    -Wl,--as-needed \
    -o ${TA_UUID}.elf \
    eevm_ta.o \
    ${EEVM_DIR}/build_arm64/libeevm.a \
    ${TA_DEV_KIT}/lib/libutils.a \
    ${TA_DEV_KIT}/lib/libutee.a \
    -static-libstdc++ -lm -lgcc

echo "[3] Creating TA binary..."
${OBJCOPY} --strip-unneeded ${TA_UUID}.elf ${TA_UUID}.ta

echo "[4] Checking binary size..."
ls -lh ${TA_UUID}.ta ${TA_UUID}.elf

echo "=== Build complete! ==="
echo "TA binary: ${TA_UUID}.ta"
