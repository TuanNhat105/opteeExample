#include <stddef.h>

// dlmalloc functions
void* dlmalloc(size_t);
void dlfree(void*);
void* dlcalloc(size_t, size_t);
void* dlrealloc(void*, size_t);

// oe_allocator layer
void* oe_malloc(size_t size) { return dlmalloc(size); }
void oe_free(void* ptr) { dlfree(ptr); }
void* oe_calloc(size_t nmemb, size_t size) { return dlcalloc(nmemb, size); }
void* oe_realloc(void* ptr, size_t size) { return dlrealloc(ptr, size); }

// Standard C library aliases
void* malloc(size_t size) { return oe_malloc(size); }
void free(void* ptr) { oe_free(ptr); }
void* calloc(size_t nmemb, size_t size) { return oe_calloc(nmemb, size); }
void* realloc(void* ptr, size_t size) { return oe_realloc(ptr, size); }

void abort(void) { while(1); }
