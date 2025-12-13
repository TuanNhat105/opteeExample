libcxx-10.0.1 được thiết kế cho hosted environment (Linux/macOS/Windows)
Mọi STL header đều cascade qua cstdlib → stdlib.h → math.h → hàng trăm hàm toán học
OP-TEE là freestanding environment, không có math library

