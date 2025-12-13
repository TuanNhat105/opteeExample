HƯỚNG DẪN KỸ THUẬT
Nhúng C++ Runtime đầy đủ cho OP-TEE Trusted Application
(Dựa trên cách OpenEnclave triển khai – copy trực tiếp để nhanh và đúng)

1. MỤC TIÊU TÀI LIỆU
Tài liệu này hướng dẫn đội kỹ thuật cách nhúng C++ runtime đầy đủ vào OP-TEE Trusted Application (TA) để đạt khả năng sử dụng C++ tương đương với OpenEnclave, bao gồm:
std::vector, std::map, std::string, std::function
Exceptions (try/catch, throw)
RTTI (typeid, dynamic_cast)
Static local variables (__cxa_guard)
Global/static constructors & destructors
Stack unwinding (libunwind)
Thread-Local Storage (TLS)
Chiến lược chính:
Không tự viết lại runtime. Copy trực tiếp các thành phần runtime từ OpenEnclave (đã được chứng minh chạy trong TEE), sau đó build và link cho OP-TEE.

2. TƯ DUY KIẾN TRÚC (BẮT BUỘC HIỂU)
2.1. Sự thật quan trọng
OP-TEE KHÔNG cung cấp C++ runtime
Compiler chỉ tạo code C++, runtime phải do bạn cung cấp
OpenEnclave đã giải quyết toàn bộ vấn đề này
➡️ Cách nhanh – an toàn – đúng:
Copy runtime của OpenEnclave sang OP-TEE, không reinvent.
2.2. Những thành phần bắt buộc của C++ runtime
Thành phần
Vai trò
libc
malloc/free, errno, atexit
libc++
STL containers, algorithms
libcxxrt
RTTI, exceptions, _cxa*
libunwind
Stack unwinding
allocator
new/delete
startup
ctor/dtor
TLS
thread_local, errno


3. CẤU TRÚC THƯ MỤC ĐÍCH (OP-TEE TA)
project/
├── 3rdparty/
│   ├── libcxx/
│   ├── libcxxrt/
│   ├── libunwind/
│   ├── musl/
│   └── dlmalloc/
├── libc/
├── ta_src/
├── CMakeLists.txt
└── toolchain.cmake


4. CHECKLIST COPY FILE TỪ OPENENCLAVE (QUAN TRỌNG NHẤT)
4.1. libc++ (STL)
Nguồn (OpenEnclave):
openenclave/3rdparty/libcxx/

Copy sang:
project/3rdparty/libcxx/

Bắt buộc có:
include/
src/
→ Cung cấp std::vector, std::map, std::string, std::function

4.2. libcxxrt (C++ ABI / Exceptions / RTTI)
Nguồn:
openenclave/3rdparty/libcxxrt/libcxxrt/

Copy sang:
project/3rdparty/libcxxrt/

Các file CỰC KỲ QUAN TRỌNG:
exception.cc – __cxa_throw
typeinfo.cc – RTTI
guard.cc – __cxa_guard_*
cxa_atexit.c, cxa_finalize.c

4.3. libunwind (Stack Unwinding)
Nguồn:
openenclave/3rdparty/libunwind/

Copy sang:
project/3rdparty/libunwind/

→ Bắt buộc cho throw/catch hoạt động an toàn

4.4. libc (musl-based)
Nguồn:
openenclave/3rdparty/musl/
openenclave/libc/

Copy sang:
project/3rdparty/musl/
project/libc/

Vai trò:
malloc/free
errno
atexit

4.5. Allocator (dlmalloc)
Nguồn:
openenclave/3rdparty/dlmalloc/

Copy sang:
project/3rdparty/dlmalloc/

→ Backend cho operator new/delete

5. CÁC HÀM RUNTIME BẮT BUỘC PHẢI CÓ
Nhóm
Hàm
Exception
__cxa_throw, __cxa_allocate_exception
Guard
__cxa_guard_acquire/release
RTTI
typeinfo
Exit
__cxa_atexit, __cxa_finalize
Memory
operator new/delete
Abort
oe_abort → TEE_Panic

➡️ KHÔNG được stub rỗng

6. STARTUP: CTOR / DTOR
6.1. Bắt buộc gọi constructors
Trong entry của TA:
extern void __libc_init_array(void);

void TA_CreateEntryPoint(void)
{
    __libc_init_array();
}

6.2. atexit / destructors
Được xử lý bởi __cxa_atexit từ libcxxrt

7. TLS (THREAD LOCAL STORAGE)
7.1. errno
Phải có:
int* __oe_errno_location(void);

7.2. C++ thread_local
Hoạt động nhờ runtime OE + TLS section

8. BUILD SYSTEM – FLAG BẮT BUỘC
8.1. Compiler flags
-fexceptions
-frtti
-funwind-tables
-fno-omit-frame-pointer
-nostdinc
-nostdinc++
-ffreestanding

8.2. TUYỆT ĐỐI KHÔNG DÙNG
-fno-exceptions
-fno-rtti
-fno-threadsafe-statics


9. LINK ORDER (CỰC KỲ QUAN TRỌNG)
libc++
libcxxrt
libunwind
libc
allocator (dlmalloc)
OP-TEE libs

Sai thứ tự → crash runtime

10. NHỮNG THỨ KHÔNG HỖ TRỢ / LƯU Ý
❌ std::cout, iostream (phải redirect log)
⚠️ threading phức tạp
⚠️ locale, filesystem

11. CHECKLIST CUỐI CHO ĐỘI KỸ THUẬT
Copy đủ runtime từ OpenEnclave
Không stub _cxa* rỗng
Bật exception + RTTI
Link đúng thứ tự
Test: vector / map / throw / TLS

12. KẾT LUẬN
Nếu thực hiện đúng các bước trên:
OP-TEE TA có thể chạy C++ gần tương đương OpenEnclave, an toàn và ổn định.
Không cần reinvent runtime – OpenEnclave đã làm sẵn, chỉ việc dùng lại.
