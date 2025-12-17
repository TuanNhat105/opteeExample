/* OP-TEE Fix for aarch64 uc_addr function
 * 
 * Problem: uc_addr() in Ginit.c only maps regs[reg] for X0-X31,
 * but SP (UNW_AARCH64_SP = 31) and PC (UNW_AARCH64_PC = 32) are
 * stored in uc_mcontext.sp and uc_mcontext.pc, not in regs[] array.
 * 
 * This causes unwinder to fail finding SP/PC, leading to stack
 * frame location errors in OP-TEE (issue #2274).
 * 
 * Solution: Override tdep_uc_addr to correctly map SP and PC.
 * 
 * Note: This file must be compiled with same flags as Ginit.c
 * and linked AFTER Ginit.c to override tdep_uc_addr.
 */

#define UNW_LOCAL_ONLY
#include "unwind_i.h"

/* Override tdep_uc_addr to handle SP and PC correctly */
HIDDEN void *
tdep_uc_addr (ucontext_t *uc, int reg)
{
  /* Handle SP and PC specially - they're not in regs[] array */
  if (reg == UNW_AARCH64_SP)
    return (void*)&uc->uc_mcontext.sp;
  else if (reg == UNW_AARCH64_PC)
    return (void*)&uc->uc_mcontext.pc;
  else if (reg >= UNW_AARCH64_X0 && reg < UNW_AARCH64_V0)
    return &uc->uc_mcontext.regs[reg];
  else if (reg >= UNW_AARCH64_V0 && reg <= UNW_AARCH64_V31)
    return &GET_FPCTX(uc)->vregs[reg - UNW_AARCH64_V0];
  else
    return NULL;
}

