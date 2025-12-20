# LevelDB with Shared Memory Ring Buffer for OP-TEE

## Tổng quan

Đây là implementation của LevelDB chạy trong OP-TEE Trusted Application (TA) sử dụng **Shared Memory Ring Buffer** để tối ưu hóa I/O operations.

### Kiến trúc

```
┌─────────────────────────────────────────┐
│         Trusted Application (TA)        │
│  ┌────────────────────────────────────┐ │
│  │  LevelDB Core                      │ │
│  │  ├─ Put/Get/Delete operations      │ │
│  │  └─ Custom RingBufferEnv           │ │
│  └────────────────┬───────────────────┘ │
│                   │                      │
│  ┌────────────────▼───────────────────┐ │
│  │  SecureRingBufferProducer          │ │
│  │  ├─ Push() - Ghi dữ liệu vào buffer│ │
│  │  └─ Flush() - Đồng bộ với REE      │ │
│  └────────────────┬───────────────────┘ │
└───────────────────┼─────────────────────┘
                    │
        ┌───────────▼────────────┐
        │ Shared Memory (4MB)    │
        │ Ring Buffer Control    │
        │ + Data Buffer          │
        └───────────┬────────────┘
                    │
┌───────────────────▼─────────────────────┐
│      Client Application (CA)            │
│  ┌────────────────────────────────────┐ │
│  │  RingBufferConsumer (Thread)       │ │
│  │  ├─ Đọc packets từ ring buffer     │ │
│  │  ├─ Ghi xuống file system          │ │
│  │  └─ fsync() khi nhận SYNC_FLUSH    │ │
│  └────────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

## Ưu điểm của Ring Buffer Architecture

### 1. **Loại bỏ World Switching Overhead**
- Mỗi lệnh `ocall` thông thường tốn ~3000-5000 CPU cycles
- Ring Buffer biến I/O thành RAM-to-RAM operations
- Giảm latency từ 50µs xuống còn <1µs cho mỗi lần ghi

### 2. **Batching Hiệu quả**
- LevelDB có thể push nhiều operations liên tiếp vào buffer
- Chỉ block khi gọi `Flush()` (khi cần đảm bảo durability)
- Tăng throughput lên 5-10 lần

### 3. **Tách biệt trách nhiệm (Decoupling)**
- TA tập trung vào logic nghiệp vụ
- REE xử lý I/O nặng và tối ưu với Page Cache của Linux

### 4. **Bảo mật được duy trì**
- Dữ liệu đi qua Ring Buffer đã được mã hóa (nếu cần)
- TA vẫn kiểm soát hoàn toàn timing của flush operations
- Chống TOCTOU attacks bằng cách copy atomic values vào stack

## Cấu trúc thư mục

```
leveldb_xapian/
├── include/
│   └── shared_ring_buffer.hpp      # Shared structures cho cả TA và CA
├── ta/
│   ├── include/
│   │   ├── secure_ring_buffer_producer.hpp  # Producer cho TA
│   │   └── leveldb_ring_buffer_env.hpp      # Custom LevelDB Env
│   ├── leveldb_ta_main.cpp                  # TA main logic
│   ├── ocall_logger.cpp                      # OCALL logging
│   ├── eevm_ta.h                            # Command definitions
│   ├── Makefile
│   └── sub.mk
└── host/
    ├── ring_buffer_consumer.hpp             # Consumer cho CA
    ├── leveldb_test_main.cpp                # Test program
    └── Makefile
```

## Dependencies

### TA Side
- OP-TEE OS (v3.x+)
- LevelDB source code (cần clone và build)
- eEVM minimal TA libraries (libcxx, libcxxrt, liboelibc)

### Host Side
- OP-TEE Client libraries (`libteec`)
- C++17 compiler
- pthread support

## Build Instructions

### 1. Chuẩn bị LevelDB

```bash
# Clone LevelDB
cd /home/abc/nhat
git clone https://github.com/google/leveldb.git
cd leveldb
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### 2. Build TA

```bash
cd /home/abc/nhat/optee_examples/leveldb_xapian/ta
export TA_DEV_KIT_DIR=/path/to/optee_os/out/arm/export-ta_arm64
make clean
make
```

**Lưu ý**: Bạn cần chỉnh sửa `LEVELDB_ROOT` trong `ta/sub.mk` cho đúng với đường dẫn LevelDB của bạn.

### 3. Build Host Application

```bash
cd /home/abc/nhat/optee_examples/leveldb_xapian/host
make clean
make
```

### 4. Deploy lên Target Device

```bash
# Copy TA binary
scp ../ta/*.ta root@<target-ip>:/lib/optee_armtz/

# Copy host binary
scp leveldb_host root@<target-ip>:/root/

# SSH vào device và chạy
ssh root@<target-ip>
./leveldb_host
```

## API Commands

### TA_EEVM_CMD_INIT_LEVELDB (Command 3)
Khởi tạo LevelDB với Ring Buffer.

**Parameters:**
- `params[0]`: MEMREF_INOUT - Shared memory buffer
- `params[1]`: VALUE_INPUT - Buffer size (default: 4MB)
- `params[2]`: MEMREF_OUTPUT - OCALL logs

### TA_EEVM_CMD_LEVELDB_PUT (Command 4)
Ghi key-value vào database.

**Parameters:**
- `params[0]`: MEMREF_INPUT - Key data
- `params[1]`: MEMREF_INPUT - Value data
- `params[2]`: VALUE_OUTPUT - Status (0=success, 1=failure)
- `params[3]`: MEMREF_OUTPUT - OCALL logs

### TA_EEVM_CMD_LEVELDB_GET (Command 5)
Đọc value theo key.

**Parameters:**
- `params[0]`: MEMREF_INPUT - Key data
- `params[1]`: MEMREF_OUTPUT - Value buffer
- `params[2]`: VALUE_OUTPUT - Status (0=found, 1=not found, 2=error)
- `params[3]`: MEMREF_OUTPUT - OCALL logs

### TA_EEVM_CMD_LEVELDB_DELETE (Command 6)
Xóa key khỏi database.

**Parameters:**
- `params[0]`: MEMREF_INPUT - Key data
- `params[1]`: VALUE_OUTPUT - Status
- `params[2]`: MEMREF_OUTPUT - OCALL logs

## Performance Tuning

### Ring Buffer Size
Mặc định là 4MB. Có thể điều chỉnh trong `shared_ring_buffer.hpp`:

```cpp
constexpr size_t DEFAULT_RING_BUFFER_SIZE = 4 * 1024 * 1024; // 4MB
```

**Khuyến nghị:**
- Workload nhẹ: 2MB
- Workload trung bình: 4MB (default)
- Workload nặng với nhiều batch writes: 8MB

### LevelDB Options

Trong `leveldb_ta_main.cpp`, bạn có thể tinh chỉnh:

```cpp
leveldb::Options options;
options.write_buffer_size = 1 * 1024 * 1024; // 1MB memtable
options.max_open_files = 100;
options.block_cache = leveldb::NewLRUCache(8 * 1024 * 1024); // 8MB cache
```

## Security Considerations

### 1. TOCTOU Protection
- Tất cả atomic values từ shared memory được copy vào local stack trước khi check
- Sử dụng `std::memory_order_acquire/release` để đảm bảo memory consistency

### 2. Buffer Overflow Prevention
- Kiểm tra kỹ `head` và `tail` trước mỗi lần Push
- Wrap-around được xử lý an toàn với `WRAP_MARKER`

### 3. Data Integrity
- `Flush()` sử dụng sequence tracking với `sync_counter`
- REE phải gọi `fsync()` và xác nhận qua `last_flushed_id`

### 4. Potential Attacks
⚠️ **Side-channel**: Tần suất ghi vào Ring Buffer có thể leak thông tin về workload
⚠️ **REE compromise**: Nếu REE bị tấn công, dữ liệu có thể bị modify sau khi rời khỏi TEE

**Giải pháp:**
- Encrypt dữ liệu trước khi push vào Ring Buffer
- Sử dụng authenticated encryption (AES-GCM)
- Add random delays để chống timing attacks

## Troubleshooting

### TA không khởi tạo được LevelDB
- **Check**: LEVELDB_ROOT path trong `ta/sub.mk`
- **Check**: Tất cả .cc files của LevelDB có được include không
- **Check**: Log từ OCALL để xem error message chi tiết

### Ring Buffer bị đầy
- Tăng `DEFAULT_RING_BUFFER_SIZE`
- Kiểm tra Consumer thread có đang chạy không
- Reduce LevelDB `write_buffer_size` để giảm burst writes

### Consumer không flush data
- Check file permissions của `/tmp/leveldb_secure/`
- Đảm bảo `RegisterFile()` được gọi trước khi TA ghi
- Verify `fsync()` không bị block bởi slow storage

## Next Steps

1. **Add Encryption**: Tích hợp AES-GCM để mã hóa dữ liệu trong Ring Buffer
2. **Metrics**: Thêm counters để theo dõi throughput và latency
3. **Xapian Integration**: Mở rộng để hỗ trợ full-text search với Xapian
4. **Compression**: Nén dữ liệu trước khi ghi vào buffer (Snappy/LZ4)

## References

- [LevelDB Documentation](https://github.com/google/leveldb/blob/main/doc/index.md)
- [OP-TEE Documentation](https://optee.readthedocs.io/)
- [Lock-free Ring Buffers](https://www.snellman.net/blog/archive/2016-12-13-ring-buffers/)
- [ARM Memory Barriers](https://developer.arm.com/documentation/100941/0101/Barriers)

## License

BSD-2-Clause (tương thích với LevelDB và OP-TEE)
