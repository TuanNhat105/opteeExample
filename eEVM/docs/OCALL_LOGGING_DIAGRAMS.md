# OCALL Logging Architecture Diagrams

## 1. High-Level Architecture

```
┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃             Normal World (Linux - REE)               ┃
┃                                                      ┃
┃  ┌──────────────────────────────────────────────┐   ┃
┃  │  Host Application (main.cpp)                 │   ┃
┃  │                                              │   ┃
┃  │  1. char log_buffer[8192];                   │   ┃
┃  │  2. Pass buffer → params[2]                  │   ┃
┃  │  3. TEEC_InvokeCommand(TA_CMD)              │   ┃
┃  │  4. Print: std::cout << log_buffer;          │   ┃
┃  │                                              │   ┃
┃  │  Output:                                     │   ┃
┃  │  ┌────────────────────────────────────┐     │   ┃
┃  │  │ [TA] Starting test...              │     │   ┃
┃  │  │ [TA] Step 1: Init                  │     │   ┃
┃  │  │ [TA] Step 2: Process               │     │   ┃
┃  │  │ [TA] Success!                      │     │   ┃
┃  │  └────────────────────────────────────┘     │   ┃
┃  └──────────────────────────────────────────────┘   ┃
┗━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
                     ║ SMC (Secure Monitor Call)
                     ║ Shared Memory
┏━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃            Secure World (OP-TEE - TEE)               ┃
┃                                                      ┃
┃  ┌──────────────────────────────────────────────┐   ┃
┃  │  Trusted Application (eevm_ta_main.cpp)     │   ┃
┃  │                                              │   ┃
┃  │  1. ocall_log_init();                        │   ┃
┃  │  2. OCALL_LOG("Starting test...");           │   ┃
┃  │     OCALL_LOG("Step 1: Init");               │   ┃
┃  │     OCALL_LOG("Step 2: Process");            │   ┃
┃  │     OCALL_LOG("Success!");                   │   ┃
┃  │                                              │   ┃
┃  │  3. ocall_log_flush_to_params(params[2]);    │   ┃
┃  │  4. return TEE_SUCCESS;                      │   ┃
┃  │                                              │   ┃
┃  │  Internal Buffer (8KB):                      │   ┃
┃  │  ┌────────────────────────────────────┐     │   ┃
┃  │  │ [TA] Starting test...\n            │     │   ┃
┃  │  │ [TA] Step 1: Init\n                │     │   ┃
┃  │  │ [TA] Step 2: Process\n             │     │   ┃
┃  │  │ [TA] Success!\n                    │     │   ┃
┃  │  └────────────────────────────────────┘     │   ┃
┃  └──────────────────────────────────────────────┘   ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
```

## 2. Data Flow Diagram

```
   Time
    │
    │  ┌─────────────────────────────────────────────────────┐
    │  │ Phase 1: Initialization                            │
    ▼  └─────────────────────────────────────────────────────┘

┌──────────┐                                      ┌──────────┐
│    CA    │                                      │    TA    │
│ (Normal) │                                      │ (Secure) │
└────┬─────┘                                      └─────┬────┘
     │                                                  │
     │ 1. Allocate log_buffer[8192]                    │
     │    memset(log_buffer, 0, 8192)                  │
     │                                                  │
     │ 2. Setup params[2]                              │
     │    params[2].buffer = log_buffer                │
     │    params[2].size = 8192                        │
     │                                                  │

    │  ┌─────────────────────────────────────────────────────┐
    │  │ Phase 2: Invocation                                │
    ▼  └─────────────────────────────────────────────────────┘

     │ 3. TEEC_InvokeCommand()                         │
     │ ──────────────────────────────────────────────► │
     │                                                  │ 4. Entry: cmd_handler()
     │                                                  │
     │                                                  │ 5. ocall_log_init()
     │                                                  │    g_ocall_pos = 0
     │                                                  │    g_ocall_buffer[0] = '\0'
     │                                                  │

    │  ┌─────────────────────────────────────────────────────┐
    │  │ Phase 3: Logging (Multiple calls)                  │
    ▼  └─────────────────────────────────────────────────────┘

     │                                                  │ 6. OCALL_LOG("Test 1")
     │                                                  │    ↓
     │                                                  │    snprintf(tmp, "[TA] Test 1\n")
     │                                                  │    ↓
     │                                                  │    memcpy(g_ocall_buffer + pos, tmp)
     │                                                  │    pos += len
     │                                                  │
     │                                                  │ 7. OCALL_LOG("Test 2")
     │                                                  │    ↓
     │                                                  │    (same process)
     │                                                  │
     │                                                  │ ... (more logs) ...
     │                                                  │

    │  ┌─────────────────────────────────────────────────────┐
    │  │ Phase 4: Flush & Return                            │
    ▼  └─────────────────────────────────────────────────────┘

     │                                                  │ 8. ocall_log_flush_to_params()
     │                                                  │    memcpy(params[2].buffer,
     │                                                  │           g_ocall_buffer,
     │                                                  │           g_ocall_pos)
     │                                                  │
     │                                                  │ 9. return TEE_SUCCESS
     │ ◄────────────────────────────────────────────── │
     │                                                  │

    │  ┌─────────────────────────────────────────────────────┐
    │  │ Phase 5: Display                                   │
    ▼  └─────────────────────────────────────────────────────┘

     │ 10. std::cout << log_buffer                     │
     │     (Displays all logs from TA)                 │
     │                                                  │
     ▼                                                  ▼
```

## 3. Memory Layout

```
┌─────────────────────────────────────────────────────────────┐
│                    Normal World Memory                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  CA Process Address Space:                                 │
│                                                             │
│  Stack:                                                     │
│    ┌──────────────────────────────────┐                    │
│    │ char log_buffer[8192]            │                    │
│    │  ┌───────────────────────────┐   │                    │
│    │  │ [TA] Starting test...\n   │   │ ◄──────┐          │
│    │  │ [TA] Step 1...\n          │   │        │          │
│    │  │ [TA] Success!\n            │   │        │          │
│    │  └───────────────────────────┘   │        │          │
│    │  0x00007fff12340000             │        │ memcpy    │
│    └──────────────────────────────────┘        │          │
│                                                             │
│  Heap: (other allocations)                                 │
│                                                             │
└─────────────────────────────────────────────────────────────┘
                              ▲
                              │ OP-TEE OS maps this memory
                              │ into Secure World address space
                              │
┌─────────────────────────────┼───────────────────────────────┐
│                    Secure World Memory                      │
├─────────────────────────────┼───────────────────────────────┤
│                             │                               │
│  TA Address Space:          │                               │
│                             │                               │
│  Static Data:               │                               │
│    ┌──────────────────────────────────┐                    │
│    │ static char g_ocall_buffer[8192] │                    │
│    │  ┌───────────────────────────┐   │                    │
│    │  │ [TA] Starting test...\n   │   │ ───────┐          │
│    │  │ [TA] Step 1...\n          │   │        │          │
│    │  │ [TA] Success!\n            │   │        │ memcpy  │
│    │  └───────────────────────────┘   │        │          │
│    │  0x40000000 (Secure memory)      │        │          │
│    └──────────────────────────────────┘        │          │
│    static size_t g_ocall_pos = 47;             │          │
│                                                 │          │
│                                                 ▼          │
│  Shared Memory Region (mapped from Normal World):         │
│    ┌──────────────────────────────────┐                   │
│    │ params[2].memref.buffer          │                   │
│    │  (points to log_buffer in CA)    │ ◄─────────────┘   │
│    │  0x40010000 (Mapped address)     │                   │
│    └──────────────────────────────────┘                   │
│                                                            │
│  Stack: (function call frames)                            │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

## 4. Function Call Flow

```
┌──────────────────────────────────────────────────────────────┐
│                   OCALL_LOG("Hello %d", 42)                  │
└────────────────────────────┬─────────────────────────────────┘
                             │
                             ▼
        ┌────────────────────────────────────────┐
        │  #define OCALL_LOG(fmt, ...)           │
        │    ocall_log_print(fmt, ##__VA_ARGS__) │
        └────────────────┬───────────────────────┘
                         │
                         ▼
    ┌─────────────────────────────────────────────────┐
    │  void ocall_log_print(const char* fmt, ...)     │
    ├─────────────────────────────────────────────────┤
    │  1. Create tmp buffer (512 bytes)               │
    │     char tmp[512];                              │
    │                                                 │
    │  2. Format with prefix                          │
    │     snprintf(tmp, "[TA] ");                     │
    │     vsnprintf(tmp+5, fmt, args);                │
    │     → tmp = "[TA] Hello 42"                     │
    │                                                 │
    │  3. Add newline                                 │
    │     strcat(tmp, "\n");                          │
    │     → tmp = "[TA] Hello 42\n"                   │
    │                                                 │
    │  4. Check space in g_ocall_buffer               │
    │     if (g_ocall_pos + len < 8192) {             │
    │                                                 │
    │  5. Append to buffer                            │
    │       memcpy(g_ocall_buffer + g_ocall_pos,      │
    │              tmp, len);                         │
    │       g_ocall_pos += len;                       │
    │       g_ocall_buffer[g_ocall_pos] = '\0';       │
    │     }                                           │
    └─────────────────────────────────────────────────┘
                         │
                         ▼
    ┌─────────────────────────────────────────────────┐
    │  g_ocall_buffer state:                          │
    │  ┌──────────────────────────────────────┐       │
    │  │ [TA] Hello 42\n                      │       │
    │  │ [TA] Previous log\n                  │       │
    │  │ [TA] Another log\n                   │       │
    │  │ ...                                  │       │
    │  └──────────────────────────────────────┘       │
    │  g_ocall_pos = 60                               │
    └─────────────────────────────────────────────────┘
```

## 5. Complete Lifecycle

```
Time  CA (Normal World)                 TA (Secure World)
═══════════════════════════════════════════════════════════════
 0    main() starts
 1    │
 2    ├─ Allocate log_buffer[8K]
 3    │
 4    ├─ Setup params[2]
 5    │   params[2] → log_buffer
 6    │
 7    ├─ TEEC_InvokeCommand() ────────►  Command handler
 8    │                                  │
 9    │                                  ├─ ocall_log_init()
10    │                                  │   g_ocall_buffer = ""
11    │                                  │   g_ocall_pos = 0
12    │                                  │
13    │                                  ├─ OCALL_LOG("Step 1")
14    │                                  │   append to g_ocall_buffer
15    │                                  │
16    │                                  ├─ OCALL_LOG("Step 2")
17    │                                  │   append to g_ocall_buffer
18    │                                  │
19    │                                  ├─ (more logs...)
20    │                                  │
21    │                                  ├─ ocall_log_flush_to_params()
22    │                                  │   memcpy(params[2].buffer,
23    │                                  │          g_ocall_buffer)
24    │                                  │
25    │                                  └─ return TEE_SUCCESS
26    │  ◄────────────────────────────────
27    │
28    ├─ Check result
29    │
30    ├─ Print log_buffer
31    │   std::cout << log_buffer
32    │
33    │   Console Output:
34    │   [TA] Step 1
35    │   [TA] Step 2
36    │   [TA] (more logs...)
37    │
38    └─ Done
```

## 6. Buffer Growth Example

```
Time  OCALL_LOG calls               g_ocall_buffer state
═══════════════════════════════════════════════════════════════

t0    ocall_log_init()             ""
                                    pos=0
                                    ┌────────────────┐
                                    │                │
                                    └────────────────┘

t1    OCALL_LOG("Start")           "[TA] Start\n"
                                    pos=12
                                    ┌────────────────┐
                                    │[TA] Start\n    │
                                    └────────────────┘

t2    OCALL_LOG("Step 1")          "[TA] Start\n[TA] Step 1\n"
                                    pos=25
                                    ┌────────────────┐
                                    │[TA] Start\n    │
                                    │[TA] Step 1\n   │
                                    └────────────────┘

t3    OCALL_LOG("Value: %d", 42)   "[TA] Start\n[TA] Step 1\n[TA] Value: 42\n"
                                    pos=42
                                    ┌────────────────┐
                                    │[TA] Start\n    │
                                    │[TA] Step 1\n   │
                                    │[TA] Value: 42\n│
                                    └────────────────┘

t4    OCALL_LOG("Done")            "[TA] Start\n[TA] Step 1\n[TA] Value: 42\n[TA] Done\n"
                                    pos=53
                                    ┌────────────────┐
                                    │[TA] Start\n    │
                                    │[TA] Step 1\n   │
                                    │[TA] Value: 42\n│
                                    │[TA] Done\n     │
                                    └────────────────┘

t5    flush_to_params()            Copy all 53 bytes → params[2].buffer
```

## 7. Error Handling Flow

```
┌──────────────────────────────────────────────────────────┐
│  TA Command Handler                                      │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  ocall_log_init();                                       │
│  OCALL_LOG("Starting operation...");                     │
│                                                          │
│  if (!validate_input()) {                               │
│      OCALL_LOG("[ERROR] Invalid input!");  ◄─────┐      │
│      goto error;                                  │      │
│  }                                                │      │
│                                                   │      │
│  try {                                            │      │
│      process();                                   │      │
│      OCALL_LOG("[SUCCESS] Completed");           │      │
│  }                                                │      │
│  catch (std::exception& e) {                      │      │
│      OCALL_LOG("[EXCEPTION] %s", e.what()); ◄────┤      │
│      goto error;                                  │      │
│  }                                                │      │
│                                                   │      │
│  // Success path                                  │      │
│  if (params[2].memref.buffer) {                   │      │
│      ocall_log_flush_to_params(...);              │      │
│  }                                                │      │
│  return TEE_SUCCESS;                              │      │
│                                                   │      │
│  error:  ◄────────────────────────────────────────┘      │
│      // CRITICAL: Must flush logs even on error!        │
│      if (params[2].memref.buffer) {                     │
│          ocall_log_flush_to_params(...); ◄─── Always!   │
│      }                                                   │
│      return TEE_ERROR_GENERIC;                          │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

---

**Note**: All diagrams use UTF-8 box drawing characters. View with a monospace font for best results.

**Created**: December 20, 2025  
**Version**: 1.0
