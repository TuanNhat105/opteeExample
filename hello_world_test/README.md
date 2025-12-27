# Hello World OP-TEE Example

Simple hello world example for OP-TEE Trusted Application (TA) and host application.

## Prerequisites

1. OP-TEE OS built (provides TA_DEV_KIT_DIR)
2. optee_client built (provides TEEC_EXPORT)
3. Cross-compiler toolchain (aarch64-none-linux-gnu- or aarch64-linux-gnu-)

## Building

### Standard build (dynamic linking)

```bash
export PATH="/home/abc/arm-toolchain/bin:$PATH"
export CROSS_COMPILE="aarch64-none-linux-gnu-"
./build.sh
```

### Static build (no shared libraries needed)

```bash
export PATH="/home/abc/arm-toolchain/bin:$PATH"
export CROSS_COMPILE="aarch64-none-linux-gnu-"
STATIC_BUILD=1 ./build.sh
```

## Deployment

### Option 1: Using deploy script with sudo (recommended for non-root users)

```bash
# For non-root user (orangepi, etc.) - uses sudo on device
DEVICE_HOST=192.168.1.160 DEVICE_USER=orangepi ./deploy_sudo.sh

# Or pass as arguments
./deploy_sudo.sh 192.168.1.160 orangepi
```

This script will:
- Copy files to `/tmp/` first (user has write permission)
- Use `sudo` on device to move files to system directories
- Copy TA to `/lib/optee_armtz/`
- Copy host binary to `/usr/bin/`

**Note:** Requires passwordless sudo or SSH key with sudo access on device.

### Option 2: Using deploy script (for root user)

```bash
# Set device IP
DEVICE_HOST=192.168.1.234 DEVICE_USER=root ./deploy.sh

# Or pass as argument
./deploy.sh 192.168.1.234
```

The deploy script will:
- Copy TA to `/lib/optee_armtz/`
- Copy host binary to `/usr/bin/`
- Copy libteec.so to `/usr/lib/` and create symlinks

### Option 3: Manual deployment (static build)

If you built with `STATIC_BUILD=1`, you only need to copy the binaries:

```bash
# Copy TA
scp ta/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta root@<device>:/lib/optee_armtz/

# Copy host binary (static, no library needed)
scp host/optee_example_hello_world root@<device>:/usr/bin/
```

### Option 4: Manual deployment (dynamic build)

```bash
# Copy TA
scp ta/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta root@<device>:/lib/optee_armtz/

# Copy host binary
scp host/optee_example_hello_world root@<device>:/usr/bin/

# Copy library
scp /home/abc/optee_client/out/export/usr/lib/libteec.so.2.0.0 root@<device>:/usr/lib/

# On device, create symlinks and update library cache
ssh root@<device>
cd /usr/lib
ln -sf libteec.so.2.0.0 libteec.so.2
ln -sf libteec.so.2.0.0 libteec.so
ldconfig
```

## Running

On the device:

```bash
optee_example_hello_world
```

Expected output:
```
Invoking TA to increment 100
TA incremented value to 101
```

## Troubleshooting

### Error: Permission denied when copying files

**Problem:** `/lib/optee_armtz/` and `/usr/bin/` require root permissions to write.

**Solution 1:** Use deploy script with sudo (recommended)
```bash
./deploy_sudo.sh <device_ip> <username>
```

**Solution 2:** Copy to /tmp first, then use sudo on device
```bash
# Copy to /tmp (user has write permission)
scp ta/*.ta orangepi@192.168.1.160:/tmp/
scp host/optee_example_hello_world orangepi@192.168.1.160:/tmp/

# On device, use sudo to move
ssh orangepi@192.168.1.160
sudo mkdir -p /lib/optee_armtz
sudo cp /tmp/*.ta /lib/optee_armtz/
sudo cp /tmp/optee_example_hello_world /usr/bin/
sudo chmod +x /usr/bin/optee_example_hello_world
```

**Solution 3:** Use root user
```bash
scp ta/*.ta root@192.168.1.160:/lib/optee_armtz/
scp host/optee_example_hello_world root@192.168.1.160:/usr/bin/
```

### Error: libteec.so.2: cannot open shared object file

**Solution 1:** Use static build (recommended for simplicity)
```bash
STATIC_BUILD=1 ./build.sh
```

**Solution 2:** Copy library to device
```bash
./deploy.sh <device_ip>
```

**Solution 3:** Set LD_LIBRARY_PATH on device
```bash
export LD_LIBRARY_PATH=/usr/lib:$LD_LIBRARY_PATH
optee_example_hello_world
```

### Error: TEEC_InitializeContext failed

- Make sure OP-TEE OS is running on device
- Check that tee-supplicant is running: `ps aux | grep tee-supplicant`

### Error: TEEC_Opensession failed / TE_ERROR_ITEM_NOT_FOUND (0xFFFF0008)

**This error means the TA cannot be found.** Common causes:

1. **TA not copied to device**
   ```bash
   # Copy TA
   scp ta/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta root@<device>:/lib/optee_armtz/
   ```

2. **tee-supplicant not running**
   ```bash
   # Check if running
   ssh root@<device> "pgrep tee-supplicant"
   
   # Start if not running
   ssh root@<device> "tee-supplicant &"
   ```

3. **OP-TEE driver not loaded**
   ```bash
   # Check device nodes
   ssh root@<device> "ls -l /dev/tee*"
   
   # Load modules if needed
   ssh root@<device> "modprobe optee && modprobe optee_rpc"
   ```

**Quick fix:**
```bash
# Run diagnostic
./check_optee.sh <device_ip>

# Or auto-fix
./fix_optee.sh <device_ip>
```

**Manual check:**
- Make sure TA is in `/lib/optee_armtz/`
- Check TA permissions: `ls -l /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta`
- Check OP-TEE logs: `dmesg | grep -i optee`
- Verify tee-supplicant: `ps aux | grep tee-supplicant`

## Files

- `ta/` - Trusted Application source and binary
- `host/` - Host application source and binary
- `build.sh` - Build script
- `deploy.sh` - Deployment script (for root user, dynamic builds)
- `deploy_sudo.sh` - Deployment script with sudo (for non-root users)
- `check_optee.sh` - Diagnostic script to check OP-TEE setup
- `fix_optee.sh` - Auto-fix script for common OP-TEE issues

