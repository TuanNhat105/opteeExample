#include <tee_internal_api.h>
#include "unwind.h"

/* Stub implementations - will panic */
_Unwind_Reason_Code _Unwind_RaiseException(struct _Unwind_Exception* exc) {
    TEE_Panic(0xECEP0001);
    return _URC_FATAL_PHASE1_ERROR;
}

void _Unwind_Resume(struct _Unwind_Exception* exc) {
    TEE_Panic(0xECEP0002);
}

void _Unwind_DeleteException(struct _Unwind_Exception* exc) {
    /* No-op */
}

unsigned long _Unwind_GetGR(struct _Unwind_Context* ctx, int index) {
    return 0;
}

void _Unwind_SetGR(struct _Unwind_Context* ctx, int index, unsigned long value) {
}

unsigned long _Unwind_GetIP(struct _Unwind_Context* ctx) {
    return 0;
}

void _Unwind_SetIP(struct _Unwind_Context* ctx, unsigned long value) {
}

unsigned long _Unwind_GetLanguageSpecificData(struct _Unwind_Context* ctx) {
    return 0;
}

unsigned long _Unwind_GetRegionStart(struct _Unwind_Context* ctx) {
    return 0;
}
