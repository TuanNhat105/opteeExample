/* OP-TEE specific fix for libunwind getcontext
 * Fixes: https://github.com/openenclave/openenclave/issues/2274
 * 
 * Override unw_tdep_getcontext with OP-TEE compatible version
 * that correctly captures frame pointer, stack pointer, and PC.
 * 
 * Key fixes:
 * 1. Fix register saving bug (x14, x13 -> x14, x15)
 * 2. Save SP to uc_mcontext.sp (not just regs[])
 * 3. Save PC to uc_mcontext.pc (not just regs[])
 * 4. Ensure all registers are saved correctly
 */
#undef unw_tdep_getcontext
#define unw_tdep_getcontext(uc) (({					\
  unw_tdep_context_t *unw_ctx = (uc);					\
  register uint64_t *unw_base asm ("x0") = (uint64_t*) unw_ctx->uc_mcontext.regs;		\
  register uint64_t unw_sp asm ("x1");					\
  register uint64_t unw_pc asm ("x2");					\
  __asm__ __volatile__ (						\
     "mov %[sp], sp\n"							\
     "mov %[pc], x30\n"							\
     "stp x0, x1, [%[base], #0]\n"					\
     "stp x2, x3, [%[base], #16]\n"					\
     "stp x4, x5, [%[base], #32]\n"					\
     "stp x6, x7, [%[base], #48]\n"					\
     "stp x8, x9, [%[base], #64]\n"					\
     "stp x10, x11, [%[base], #80]\n"					\
     "stp x12, x13, [%[base], #96]\n"					\
     "stp x14, x15, [%[base], #112]\n"					\
     "stp x16, x17, [%[base], #128]\n"					\
     "stp x18, x19, [%[base], #144]\n"					\
     "stp x20, x21, [%[base], #160]\n"					\
     "stp x22, x23, [%[base], #176]\n"					\
     "stp x24, x25, [%[base], #192]\n"					\
     "stp x26, x27, [%[base], #208]\n"					\
     "stp x28, x29, [%[base], #224]\n"					\
     "str x30, [%[base], #240]\n"					\
     : [base] "+r" (unw_base), [sp] "=r" (unw_sp), [pc] "=r" (unw_pc) : : "memory");	\
  unw_ctx->uc_mcontext.sp = unw_sp;					\
  unw_ctx->uc_mcontext.pc = unw_pc;					\
  }), 0)
