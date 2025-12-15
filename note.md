sudo dd if=sdcard.img of=/dev/sda bs=4M status=progress conv=fsync
sudo dd if=sdcard-ok.img of=/dev/sda bs=4M status=progress conv=fsync
#build optee_os hỗ trợ c++
make PLATFORM=rpi5 \
  CFG_ARM64_core=y \
  CFG_USER_TA_TARGETS=ta_arm64 \
  CFG_DT=y \
  CFG_TEE_CORE_DEBUG=y \
  CFG_TEE_CORE_LOG_LEVEL=4 \
  CFG_CORE_ASLR=n \
  CFG_CXX=y
  
  
#chạy biên dịch tee
tee-supplicant &

export TA_DEV_KIT_DIR=/home/abc/optee_os/out/arm-plat-rpi5/export-ta_arm64
export TEEC_EXPORT=/home/abc/optee_client/out/export/usr

# 4. Chạy lệnh Build (với toolchain của bạn)
make CROSS_COMPILE=/home/abc/toolchain/bin/aarch64-none-linux-gnu-

# kích hoạt tee trên rpi5
modprobe optee 

# di chuyen 
scp -O ./ta_cmake/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta root@192.168.1.3:/lib/optee_armtz/
scp -O ./host_cmake/minimal_evm_host root@192.168.1.3:/usr/bin/

cd /home/abc/nhat/optee_examples/eevm_minimal_ta/build_cmake && scp -O ta_cmake/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta root@192.168.1.3:/lib/optee_armtz/ && scp -O host_cmake/minimal_evm_host root@192.168.1.3:/usr/bin/

export PATH=/home/abc/arm-toolchain/bin:$PATH
export TA_DEV_KIT_DIR=$(pwd)/out/arm-plat-rpi5/export-ta_arm64

  
  
  
  #### build lại root xóa optee os
  cd /home/abc/buildroot && make optee-os-dirclean 2>&1 | tail -10
  cd /home/abc/buildroot && make arm-trusted-firmware-dirclean 2>&1 | tail -10
  make optee-os arm-trusted-firmware  -j$(nproc)
  
  
  export CROSS_COMPILE=$(pwd)/toolchain/bin/aarch64-none-linux-gnu-
  
 
  
  
ssh-keygen -f "/home/abc/.ssh/known_hosts" -R "192.168.1.3"
ssh-keygen -f "/home/nhat/.ssh/known_hosts" -R "192.168.1.3"

"dmesg | grep -i 'optee\|tzdram' | head -10"

# xem size 
 cd /home/abc/nhat/optee_examples/eevm_minimal_ta/build_cmake/ta_cmake && /home/abc/arm-toolchain/bin/aarch64-none-linux-gnu-size -A 8aaaf200-2450-11e4-abe2-0002a5d5c51b.elf | head -30