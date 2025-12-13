typedef __SIZE_TYPE__ size_t;

extern "C" {
    void* malloc(size_t);
    void free(void*);
    void abort(void);
}

void* operator new(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) abort();
    return ptr;
}

void* operator new[](size_t size) {
    void* ptr = malloc(size);
    if (!ptr) abort();
    return ptr;
}

void operator delete(void* ptr) noexcept {
    if (ptr) free(ptr);
}

void operator delete[](void* ptr) noexcept {
    if (ptr) free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    if (ptr) free(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
    if (ptr) free(ptr);
}
