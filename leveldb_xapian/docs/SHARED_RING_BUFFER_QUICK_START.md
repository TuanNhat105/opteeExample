# Quick Start: Shared Ring Buffer

## 🚀 Sử Dụng Nhanh

### 1. Host Side - Setup Ring Buffer

```cpp
#include "simple_ring_reader.hpp"

// Allocate shared memory
TEEC_SharedMemory ring_shm;
ring_shm.size = 64 * 1024;  // 64KB
ring_shm.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
TEEC_AllocateSharedMemory(&ctx, &ring_shm);

// Initialize ring buffer structure
struct SharedRingBuffer* rb = 
    (struct SharedRingBuffer*)ring_shm.buffer;
rb->head = 0;
rb->tail = 0;
rb->size = ring_shm.size - sizeof(SharedRingBuffer);

// Start reader thread
SimpleRingReader reader(ring_shm.buffer);
reader.SetCallback([](const char* data, size_t len) {
    std::cout << "[REALTIME] " << data << std::endl;
});
reader.Start();

// Pass ring buffer to TA via parameter
op.params[0].memref.parent = &ring_shm;
op.params[0].memref.offset = 0;
op.params[0].memref.size = ring_shm.size;

// Invoke command
TEEC_InvokeCommand(&sess, CMD_ID, &op, &err);
```

### 2. TA Side - Write to Ring Buffer

```cpp
#include "simple_shared_ring_buffer.h"

static TEE_Result my_command(uint32_t param_types, TEE_Param params[4]) {
    // Get ring buffer from parameter
    struct SharedRingBuffer* rb = 
        (struct SharedRingBuffer*)params[0].memref.buffer;
    
    // Write logs realtime
    write_to_ring_buffer(rb, "Starting processing...\n", 24);
    
    // Do some work
    for (int i = 0; i < 10; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Step %d/10\n", i+1);
        write_to_ring_buffer(rb, msg, strlen(msg));
        
        // Simulate work
        TEE_Wait(100);  // 100ms
    }
    
    write_to_ring_buffer(rb, "Done!\n", 6);
    
    return TEE_SUCCESS;
}
```

### 3. Helper Function (TA Side)

```cpp
static void write_to_ring_buffer(struct SharedRingBuffer* rb, 
                                  const char* data, size_t len) {
    if (!rb || !data || len == 0) return;
    
    for (size_t i = 0; i < len; i++) {
        // Read current state
        __asm__ volatile("dmb ish" ::: "memory");
        uint32_t head = rb->head;
        uint32_t tail = rb->tail;
        __asm__ volatile("dmb ish" ::: "memory");
        
        // Calculate next position
        uint32_t next = (head + 1) % rb->size;
        
        // Check if buffer full
        if (next == tail) continue;
        
        // Write byte
        rb->data[head] = data[i];
        
        // Update head
        __asm__ volatile("dmb ish" ::: "memory");
        rb->head = next;
        __asm__ volatile("dmb ish" ::: "memory");
    }
}
```

## 📝 Checklist

- [ ] Allocate shared memory với `TEEC_AllocateSharedMemory`
- [ ] Initialize ring buffer structure (head=0, tail=0, size=...)
- [ ] Start reader thread TRƯỚC khi invoke command
- [ ] Pass ring buffer qua parameter (MEMREF_WHOLE)
- [ ] Use memory barriers trong TA khi ghi
- [ ] Use memory barriers trong Host khi đọc
- [ ] Stop reader thread sau khi command kết thúc

## ⚠️ Common Mistakes

1. **Start reader thread SAU invoke command** → Mất logs
2. **Quên memory barriers** → Race condition
3. **Buffer quá nhỏ** → Mất dữ liệu khi đầy
4. **Không check buffer full** → Overwrite dữ liệu chưa đọc

## 📚 Xem Thêm

- `SHARED_RING_BUFFER_MECHANISM.md` - Giải thích chi tiết
- `test_simple_shm.cpp` - Example code đầy đủ
- `simple_ring_reader.hpp` - Reader implementation
- `simple_shared_ring_buffer.h` - Structure definition

