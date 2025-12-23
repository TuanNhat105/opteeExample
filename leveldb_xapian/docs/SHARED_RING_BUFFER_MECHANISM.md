# Cơ Chế Shared Ring Buffer cho Real-time Data Transfer

## 📋 Tổng Quan

Shared Ring Buffer là một cơ chế cho phép **Secure World (TA)** gửi dữ liệu realtime đến **Normal World (Host)** mà **không cần đợi command kết thúc**. Đây là giải pháp thay thế cho việc truyền dữ liệu qua parameter (chỉ nhận được khi command hoàn thành).

## 🏗️ Kiến Trúc Tổng Quan

```
┌─────────────────────────────────────────────────────────────┐
│                    NORMAL WORLD (Host)                       │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  Main Thread:                    Reader Thread:             │
│  ┌─────────────┐                ┌──────────────┐            │
│  │ Allocate    │                │ Read from   │            │
│  │ Shared Mem  │                │ Ring Buffer │            │
│  │             │                │ (Realtime)  │            │
│  │ Initialize  │                │             │            │
│  │ Ring Buffer │                │ Display     │            │
│  │             │                │ Logs       │            │
│  │ Start       │───────────────→│             │            │
│  │ Reader      │                │             │            │
│  │ Thread      │                │             │            │
│  │             │                │             │            │
│  │ Invoke      │                │             │            │
│  │ Command     │                │             │            │
│  │ (Blocking)  │                │ (Non-block) │            │
│  └─────────────┘                └──────────────┘            │
│         │                              │                     │
│         └──────────┬───────────────────┘                     │
│                    │                                          │
│                    ▼                                          │
│         ┌──────────────────────┐                             │
│         │  Shared Memory       │                             │
│         │  (Ring Buffer)       │                             │
│         │  ┌───────────────┐   │                             │
│         │  │ head (TA)    │   │                             │
│         │  │ tail (Host)  │   │                             │
│         │  │ size         │   │                             │
│         │  │ data[]       │   │                             │
│         │  └───────────────┘   │                             │
│         └──────────────────────┘                             │
│                    ▲                                          │
│                    │                                          │
└────────────────────┼──────────────────────────────────────────┘
                     │
                     │ TEEC_InvokeCommand
                     │
┌────────────────────┼──────────────────────────────────────────┐
│                    │                                          │
│         ┌──────────▼──────────┐                              │
│         │  SECURE WORLD (TA)   │                              │
│         │                     │                              │
│         │  Command Handler:   │                              │
│         │  ┌───────────────┐  │                              │
│         │  │ Process Data  │  │                              │
│         │  │               │  │                              │
│         │  │ Write to      │──┼──→ Ring Buffer               │
│         │  │ Ring Buffer   │  │    (Realtime)                │
│         │  │ (Realtime)    │  │                              │
│         │  │               │  │                              │
│         │  │ Continue      │  │                              │
│         │  │ Processing    │  │                              │
│         │  └───────────────┘  │                              │
│         │                     │                              │
│         └─────────────────────┘                              │
│                                                               │
└───────────────────────────────────────────────────────────────┘
```

## 📐 Cấu Trúc Dữ Liệu

### SharedRingBuffer Structure

```c
struct SharedRingBuffer {
    volatile uint32_t head;  // Vị trí TA đang ghi tới (Write Index)
    volatile uint32_t tail;  // Vị trí Host đã đọc tới (Read Index)
    uint32_t size;           // Kích thước vùng data buffer
    uint8_t data[];          // Mảng dữ liệu thực tế (Flexible array member)
};
```

**Giải thích các trường:**

1. **`head` (volatile)**: 
   - TA (Producer) ghi vào đây
   - Tăng lên mỗi khi TA ghi một byte
   - **volatile** để compiler không tối ưu hóa, đảm bảo đọc/ghi từ memory thực

2. **`tail` (volatile)**:
   - Host (Consumer) đọc từ đây
   - Tăng lên mỗi khi Host đọc một byte
   - **volatile** để đảm bảo Host luôn thấy giá trị mới nhất từ TA

3. **`size`**:
   - Kích thước của data buffer (không bao gồm header)
   - Tính bằng: `total_shared_memory_size - sizeof(SharedRingBuffer)`

4. **`data[]`**:
   - Flexible array member - mảng dữ liệu thực tế
   - Kích thước = `size` bytes

### Ring Buffer Layout trong Memory

```
┌─────────────────────────────────────────────────────────┐
│ Shared Memory (64KB total)                              │
├─────────────────────────────────────────────────────────┤
│                                                          │
│ Offset 0x0000:  SharedRingBuffer Header                 │
│   ├─ head:     uint32_t (4 bytes)                      │
│   ├─ tail:     uint32_t (4 bytes)                      │
│   ├─ size:     uint32_t (4 bytes)                       │
│   └─ padding:  (để align)                               │
│                                                          │
│ Offset 0x0010:  data[] buffer (65524 bytes)             │
│   ├─ [0]       [1]       [2]       ...    [65523]     │
│   └─ Circular: Khi head/tail đến cuối, quay về đầu      │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

## 🔄 Cơ Chế Hoạt Động

### 1. Khởi Tạo (Initialization)

**Host Side:**
```cpp
// 1. Allocate shared memory
TEEC_SharedMemory ring_shm;
ring_shm.size = 64 * 1024;  // 64KB
ring_shm.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
TEEC_AllocateSharedMemory(&ctx, &ring_shm);

// 2. Initialize ring buffer structure
struct SharedRingBuffer* rb = (struct SharedRingBuffer*)ring_shm.buffer;
rb->head = 0;   // TA chưa ghi gì
rb->tail = 0;   // Host chưa đọc gì
rb->size = ring_shm.size - sizeof(SharedRingBuffer);  // 65524 bytes

// 3. Memory barrier để đảm bảo initialization visible
__sync_synchronize();
```

**Tại sao cần memory barrier?**
- Đảm bảo các write operations hoàn thành trước khi TA đọc
- Tránh compiler reordering
- Đảm bảo cache coherence

### 2. Producer (TA - Secure World) - Ghi Dữ Liệu

**Flow:**
```
TA muốn ghi message "Hello\n"
  ↓
Với mỗi ký tự trong message:
  1. Đọc head và tail hiện tại (với memory barrier)
  2. Tính next_head = (head + 1) % size
  3. Kiểm tra buffer đầy: next_head == tail?
     - Nếu đầy: Skip byte này (hoặc wait)
     - Nếu chưa đầy: Tiếp tục
  4. Ghi byte vào data[head]
  5. Memory barrier
  6. Update head = next_head
  7. Memory barrier (để Host thấy được)
```

**Code trong TA:**
```cpp
static void write_to_ring_buffer(struct SharedRingBuffer* rb, 
                                  const char* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        // 1. Đọc head và tail với memory barrier
        __asm__ volatile("dmb ish" ::: "memory");
        uint32_t current_head = rb->head;
        uint32_t current_tail = rb->tail;
        __asm__ volatile("dmb ish" ::: "memory");
        
        // 2. Tính next position
        uint32_t next_head = (current_head + 1) % rb->size;
        
        // 3. Kiểm tra buffer đầy
        if (next_head == current_tail) {
            continue;  // Buffer đầy, skip
        }
        
        // 4. Ghi byte
        rb->data[current_head] = data[i];
        
        // 5. Memory barrier trước khi update head
        __asm__ volatile("dmb ish" ::: "memory");
        
        // 6. Update head
        rb->head = next_head;
        
        // 7. Memory barrier để Host thấy được
        __asm__ volatile("dmb ish" ::: "memory");
    }
}
```

**Tại sao cần nhiều memory barrier?**
- **Barrier 1**: Đảm bảo đọc head/tail từ memory thực, không từ cache
- **Barrier 2**: Đảm bảo data được ghi trước khi update head
- **Barrier 3**: Đảm bảo head update visible cho Host

### 3. Consumer (Host - Normal World) - Đọc Dữ Liệu

**Flow:**
```
Reader Thread (chạy liên tục):
  ↓
Loop:
  1. Memory barrier
  2. Đọc head và tail
  3. Memory barrier
  4. Nếu head != tail: Có dữ liệu mới
     - Đọc byte từ data[tail]
     - Xử lý byte (accumulate vào line buffer)
     - Nếu gặp '\n': Output cả dòng
     - Update tail = (tail + 1) % size
     - Memory barrier
  5. Nếu head == tail: Không có dữ liệu
     - Sleep 10 microseconds
     - Quay lại bước 1
```

**Code trong Host:**
```cpp
void ReaderLoop() {
    char line_buffer[1024];
    size_t line_pos = 0;
    
    while (running) {
        // 1. Memory barrier
        __sync_synchronize();
        uint32_t h = rb->head;
        __sync_synchronize();
        uint32_t t = rb->tail;
        
        // 2. Đọc dữ liệu nếu có
        while (h != t) {
            char c = rb->data[t];
            
            // 3. Accumulate vào line buffer
            if (c == '\n') {
                line_buffer[line_pos] = '\0';
                callback(line_buffer, line_pos);  // Output dòng
                line_pos = 0;
            } else {
                line_buffer[line_pos++] = c;
            }
            
            // 4. Update tail
            t = (t + 1) % rb->size;
            __sync_synchronize();
            rb->tail = t;
            __sync_synchronize();
            
            // 5. Re-check head (TA có thể đã ghi thêm)
            __sync_synchronize();
            h = rb->head;
            __sync_synchronize();
        }
        
        // 6. Sleep nếu không có dữ liệu
        usleep(10);
    }
}
```

## 🔄 Circular Buffer (Ring Buffer) Logic

### Wrap-around Mechanism

```
Buffer size = 10 bytes

Initial state:
head = 0, tail = 0
[0][1][2][3][4][5][6][7][8][9]
 ^
head, tail

TA ghi 8 bytes: "Hello\n"
head = 8, tail = 0
[H][e][l][l][o][\n][ ][ ][ ][ ]
 ^              ^
tail           head

Host đọc 6 bytes: "Hello\n"
head = 8, tail = 6
[H][e][l][l][o][\n][ ][ ][ ][ ]
              ^  ^
            tail head

TA ghi thêm 4 bytes: "Test"
head = 0 (wrap!), tail = 6
[T][e][s][t][o][\n][ ][ ][ ][ ]
 ^           ^
head        tail

Host đọc 4 bytes: "Test"
head = 0, tail = 0 (wrap!)
[T][e][s][t][o][\n][ ][ ][ ][ ]
 ^
head, tail (buffer trống)
```

**Công thức wrap-around:**
```cpp
next_position = (current_position + 1) % buffer_size
```

**Ví dụ:**
- Buffer size = 10
- Current = 9
- Next = (9 + 1) % 10 = 0 (quay về đầu)

## 🔒 Thread Safety & Race Conditions

### Vấn Đề Race Condition

**Scenario nguy hiểm:**
```
TA Thread:                    Host Thread:
Read head = 5                 Read head = 5
Read tail = 3                 Read tail = 3
Calculate next = 6            Calculate next = 6
Write data[5] = 'A'          Read data[3] = 'X'
Update head = 6               Update tail = 4
```

**Giải pháp:**
1. **Memory Barriers**: Đảm bảo thứ tự operations
2. **Volatile**: Tránh compiler optimization
3. **Atomic Operations**: Đảm bảo read-modify-write atomic

### Memory Barrier Types

**`dmb ish` (Data Memory Barrier - Inner Shareable):**
- Đảm bảo tất cả memory operations trước barrier hoàn thành trước các operations sau barrier
- **ish** = Inner Shareable domain (giữa các CPU cores)

**`__sync_synchronize()` (GCC builtin):**
- Full memory barrier
- Tương đương `dmb ish` trên ARM

### Cache Coherency

**Vấn đề:**
- TA có thể ghi vào L1 cache của Secure World
- Host có thể đọc từ L1 cache của Normal World
- Cache không sync → Host không thấy dữ liệu mới

**Giải pháp:**
- Shared memory được allocate bởi `TEEC_AllocateSharedMemory` là **cacheable**
- OP-TEE kernel tự động quản lý cache coherency
- Memory barriers đảm bảo cache flush/invalidate đúng lúc

## 📊 Flow Diagram Chi Tiết

### Complete Flow: Từ TA Ghi đến Host Đọc

```
┌─────────────────────────────────────────────────────────────┐
│ STEP 1: TA muốn ghi "Hello\n"                              │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ STEP 2: write_to_ring_buffer() được gọi                    │
│   - Loop qua từng ký tự: 'H', 'e', 'l', 'l', 'o', '\n'     │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ STEP 3: Với mỗi ký tự:                                      │
│                                                              │
│   a) Memory barrier (dmb ish)                               │
│   b) Read head = 0, tail = 0                               │
│   c) Memory barrier                                         │
│   d) Calculate next_head = 1                               │
│   e) Check: next_head (1) != tail (0) → OK                 │
│   f) Write: data[0] = 'H'                                   │
│   g) Memory barrier                                         │
│   h) Update: head = 1                                       │
│   i) Memory barrier                                         │
│                                                              │
│   Repeat cho 'e', 'l', 'l', 'o', '\n'                       │
│   Final: head = 6, tail = 0                                │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ STEP 4: Host Reader Thread (đang chạy song song)           │
│                                                              │
│   a) Memory barrier                                         │
│   b) Read head = 6, tail = 0                               │
│   c) Memory barrier                                         │
│   d) Check: head (6) != tail (0) → Có dữ liệu!             │
│   e) Loop:                                                  │
│      - Read data[0] = 'H' → add to line_buffer             │
│      - Read data[1] = 'e' → add to line_buffer             │
│      - ...                                                  │
│      - Read data[5] = '\n' → Output line "Hello"           │
│   f) Update tail = 6                                       │
│   g) Memory barrier                                         │
│   h) Check: head (6) == tail (6) → Buffer trống            │
│   i) Sleep 10us                                             │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ STEP 5: Host hiển thị:                                      │
│   [REALTIME] Hello                                           │
└─────────────────────────────────────────────────────────────┘
```

## 🎯 Tại Sao Cơ Chế Này Hoạt Động?

### 1. **Shared Memory Persists Across Command Execution**

- Shared memory được allocate bởi `TEEC_AllocateSharedMemory` tồn tại trong suốt session
- Không bị unmapped khi command đang chạy
- TA có thể ghi vào shared memory bất cứ lúc nào trong command execution

### 2. **Reader Thread Chạy Độc Lập**

- Reader thread chạy trong Normal World, không bị block bởi `TEEC_InvokeCommand`
- Thread này poll ring buffer liên tục, đọc dữ liệu ngay khi có

### 3. **Memory Barriers Đảm Bảo Visibility**

- TA ghi → Memory barrier → Head update → Host thấy được
- Host đọc → Memory barrier → Tail update → TA thấy được

### 4. **Volatile Prevents Optimization**

- Compiler không thể cache giá trị `head`/`tail` trong register
- Mỗi lần đọc đều từ memory thực

## ⚠️ Lưu Ý Quan Trọng

### 1. **Buffer Overflow Protection**

```cpp
// Kiểm tra buffer đầy
if (next_head == tail) {
    // Buffer đầy - có 2 lựa chọn:
    // 1. Skip byte (mất dữ liệu)
    // 2. Wait (có thể deadlock nếu Host không đọc)
    continue;  // Chọn skip
}
```

### 2. **Reader Thread Phải Chạy Trước Command**

```cpp
// ĐÚNG:
g_ring_reader->Start();           // Start reader thread
sleep(100ms);                     // Đợi thread khởi động
TEEC_InvokeCommand(...);          // Invoke command

// SAI:
TEEC_InvokeCommand(...);          // Invoke command
g_ring_reader->Start();           // Start reader thread (quá muộn!)
```

### 3. **Memory Barriers Là Bắt Buộc**

- Không có memory barrier → Race condition
- Không có memory barrier → Cache coherency issues
- Không có memory barrier → Data corruption

### 4. **Ring Buffer Size**

- Quá nhỏ → Dễ bị đầy, mất dữ liệu
- Quá lớn → Lãng phí memory
- **Khuyến nghị**: 64KB - 256KB

## 🔍 Debug Tips

### Kiểm Tra Ring Buffer State

```cpp
// In trạng thái ring buffer
printf("head=%u, tail=%u, size=%u, available=%u\n",
       rb->head, rb->tail, rb->size,
       (rb->head >= rb->tail) ? 
           (rb->size - (rb->head - rb->tail)) : 
           (rb->tail - rb->head));
```

### Kiểm Tra Data Corruption

```cpp
// Verify ring buffer structure
if (rb->size == 0 || rb->size > MAX_SIZE) {
    // Invalid size
}
if (rb->head >= rb->size || rb->tail >= rb->size) {
    // Invalid indices
}
```

## 📈 Performance Considerations

### Latency

- **TA → Host**: ~10-50 microseconds (tùy vào reader thread polling interval)
- **Memory barrier overhead**: ~10-20 nanoseconds mỗi barrier
- **Cache miss**: ~100-200 nanoseconds

### Throughput

- **Maximum**: ~10-50 MB/s (tùy vào buffer size và polling frequency)
- **Typical**: ~1-5 MB/s cho logging

### Optimization Tips

1. **Tăng buffer size**: Giảm risk of overflow
2. **Giảm polling interval**: Giảm latency nhưng tăng CPU usage
3. **Batch writes**: Ghi nhiều bytes một lúc thay vì từng byte
4. **Use larger messages**: Giảm overhead của memory barriers

## 💡 Ví Dụ Cụ Thể

### Scenario: TA Gửi Log "Processing step 1/5\n"

**Timeline:**

```
Time    TA (Producer)                    Host (Consumer)
─────────────────────────────────────────────────────────────
T0      head=0, tail=0
        [ ][ ][ ][ ][ ][ ][ ][ ][ ][ ]
         ^
        head, tail

T1      Ghi 'P' vào data[0]
        head=1, tail=0
        [P][ ][ ][ ][ ][ ][ ][ ][ ][ ]
         ^  ^
        tail head

T2      Ghi 'r' vào data[1]
        head=2, tail=0
        [P][r][ ][ ][ ][ ][ ][ ][ ][ ]
         ^     ^
        tail  head

T3      ... (tiếp tục ghi)
        head=6, tail=0
        [P][r][o][c][e][s][ ][ ][ ][ ]
         ^           ^
        tail        head

T4      Reader thread đọc:
        - Đọc data[0]='P' → line_buffer
        - Đọc data[1]='r' → line_buffer
        - ...
        - Đọc data[5]='s' → line_buffer
        head=6, tail=6
        [P][r][o][c][e][s][ ][ ][ ][ ]
                    ^  ^
                  tail head

T5      TA ghi '\n' vào data[6]
        head=7, tail=6
        [P][r][o][c][e][s][\n][ ][ ][ ]
                    ^     ^
                  tail  head

T6      Reader thread đọc '\n'
        - Output: "[REALTIME] Process"
        - Reset line_buffer
        head=7, tail=7
        [P][r][o][c][e][s][\n][ ][ ][ ]
                        ^  ^
                      tail head
```

### Code Flow Example

**TA Side:**
```cpp
// Trong command handler
static TEE_Result test_simple_shm(...) {
    // Get ring buffer từ parameter
    struct SharedRingBuffer* rb = 
        (struct SharedRingBuffer*)params[0].memref.buffer;
    
    // Ghi log realtime
    char msg[] = "Processing step 1/5\n";
    write_to_ring_buffer(rb, msg, strlen(msg));
    
    // Command vẫn tiếp tục chạy...
    // Host đã nhận được log rồi!
    
    return TEE_SUCCESS;
}
```

**Host Side:**
```cpp
// Main thread
int main() {
    // 1. Allocate và init ring buffer
    TEEC_SharedMemory ring_shm;
    TEEC_AllocateSharedMemory(&ctx, &ring_shm);
    
    struct SharedRingBuffer* rb = 
        (struct SharedRingBuffer*)ring_shm.buffer;
    rb->head = 0;
    rb->tail = 0;
    rb->size = 65524;
    
    // 2. Start reader thread (TRƯỚC khi invoke command!)
    g_ring_reader = std::make_unique<SimpleRingReader>(ring_shm.buffer);
    g_ring_reader->SetCallback([](const char* data, size_t len) {
        std::cout << "[REALTIME] " << data << std::endl;
    });
    g_ring_reader->Start();
    
    // 3. Invoke command (blocking)
    // Trong khi command đang chạy, reader thread đang đọc logs realtime!
    TEEC_InvokeCommand(&sess, TA_EEVM_CMD_TEST_SIMPLE_SHM, &op, &err);
    
    // 4. Command kết thúc, reader thread vẫn chạy để đọc logs còn lại
    sleep(500ms);
    
    // 5. Stop reader thread
    g_ring_reader->Stop();
}
```

## 🔬 So Sánh Với Các Phương Pháp Khác

### ❌ Phương Pháp 1: Parameter Passing (Cũ)

```
TA: Process → Accumulate logs → Return via parameter
Host: Invoke → Wait → Receive logs (chỉ khi command xong)
```

**Nhược điểm:**
- Phải đợi command kết thúc mới nhận được logs
- Không realtime
- Buffer size bị giới hạn bởi parameter size

### ❌ Phương Pháp 2: Persistent Shared Memory (FAILED)

```
TA: Store pointer to shared memory globally
Host: Background thread reads from shared memory
```

**Vấn đề:**
- `TEE_ERROR_TARGET_DEAD (0xFFFF3024)`
- OP-TEE unmaps shared memory sau command
- Global pointer trở thành invalid

### ✅ Phương Pháp 3: Ring Buffer (HIỆN TẠI)

```
TA: Write to ring buffer (passed via parameter)
Host: Reader thread reads from ring buffer
```

**Ưu điểm:**
- ✅ Realtime logs
- ✅ Không cần đợi command kết thúc
- ✅ Shared memory valid trong suốt command execution
- ✅ Thread-safe với memory barriers
- ✅ Circular buffer - tái sử dụng memory

## 🎓 Tóm Tắt

**Shared Ring Buffer cho phép:**
- ✅ Real-time data transfer từ TA → Host
- ✅ Không cần đợi command kết thúc
- ✅ Thread-safe với memory barriers
- ✅ Cache coherent với OP-TEE shared memory
- ✅ Circular buffer để tái sử dụng memory

**Các thành phần chính:**
1. **SharedRingBuffer structure**: Header với head/tail/size
2. **Producer (TA)**: Ghi dữ liệu với memory barriers
3. **Consumer (Host)**: Đọc dữ liệu trong reader thread
4. **Memory barriers**: Đảm bảo visibility và ordering

**Kết quả:**
- Dữ liệu xuất hiện realtime trên Host terminal
- Không có lỗi `TEE_ERROR_TARGET_DEAD`
- Thread-safe và cache coherent

