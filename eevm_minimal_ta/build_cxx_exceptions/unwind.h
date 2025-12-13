/* Minimal unwind.h for libcxxrt without libunwind */
#ifndef _UNWIND_H
#define _UNWIND_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    _URC_NO_REASON = 0,
    _URC_FOREIGN_EXCEPTION_CAUGHT = 1,
    _URC_FATAL_PHASE2_ERROR = 2,
    _URC_FATAL_PHASE1_ERROR = 3,
    _URC_NORMAL_STOP = 4,
    _URC_END_OF_STACK = 5,
    _URC_HANDLER_FOUND = 6,
    _URC_INSTALL_CONTEXT = 7,
    _URC_CONTINUE_UNWIND = 8
} _Unwind_Reason_Code;

typedef enum {
    _UA_SEARCH_PHASE = 1,
    _UA_CLEANUP_PHASE = 2,
    _UA_HANDLER_FRAME = 4,
    _UA_FORCE_UNWIND = 8,
    _UA_END_OF_STACK = 16
} _Unwind_Action;

struct _Unwind_Exception;
struct _Unwind_Context;

typedef void (*_Unwind_Exception_Cleanup_Fn)(
    _Unwind_Reason_Code reason,
    struct _Unwind_Exception* exc);

struct _Unwind_Exception {
    unsigned long long exception_class;
    _Unwind_Exception_Cleanup_Fn exception_cleanup;
    unsigned long private_1;
    unsigned long private_2;
} __attribute__((__aligned__));

typedef unsigned long _Unwind_Ptr;
typedef long _sleb128_t;
typedef unsigned long _uleb128_t;

/* Stub implementations - will panic if called */
extern _Unwind_Reason_Code _Unwind_RaiseException(struct _Unwind_Exception*);
extern void _Unwind_Resume(struct _Unwind_Exception*);
extern void _Unwind_DeleteException(struct _Unwind_Exception*);
extern unsigned long _Unwind_GetGR(struct _Unwind_Context*, int);
extern void _Unwind_SetGR(struct _Unwind_Context*, int, unsigned long);
extern unsigned long _Unwind_GetIP(struct _Unwind_Context*);
extern void _Unwind_SetIP(struct _Unwind_Context*, unsigned long);
extern unsigned long _Unwind_GetLanguageSpecificData(struct _Unwind_Context*);
extern unsigned long _Unwind_GetRegionStart(struct _Unwind_Context*);

#ifdef __cplusplus
}
#endif

#endif /* _UNWIND_H */
