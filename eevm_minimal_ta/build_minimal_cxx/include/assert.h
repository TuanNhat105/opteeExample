#ifndef _ASSERT_H
#define _ASSERT_H
extern "C" {
void __assert_fail(const char*, const char*, unsigned, const char*) __attribute__((noreturn));
}
#define assert(x) ((x) ? (void)0 : __assert_fail(#x, __FILE__, __LINE__, __func__))
#endif
