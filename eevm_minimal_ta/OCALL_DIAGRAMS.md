# OCALL Architecture Diagrams

## 1. High-Level Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                         NORMAL WORLD                             │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  Host Application (main.cpp)                               │ │
│  │                                                            │ │
│  │  1. Prepare log buffer (8KB)                              │ │
│  │     char log_buffer[8192];                                │ │
│  │                                                            │ │
│  │  2. Set up operation                                      │ │
│  │     op.params[1].tmpref.buffer = log_buffer;             │ │
│  │                                                            │ │
│  │  3. Invoke TA command                                     │ │
│  │     TEEC_InvokeCommand(sess, cmd, &op, ...);             │ │
│  │                                                            │ │
│  │  4. Print received logs                                   │ │
│  │     cout << log_buffer;                                   │ │
│  └────────────────────────────────────────────────────────────┘ │
└───────────────────────────────┬─────────────────────────────────┘
                                │
                                │ TEE Client API
                                │ (temp buffer mapping)
                                │
┌───────────────────────────────▼─────────────────────────────────┐
│                         SECURE WORLD                             │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  Trusted Application (minimal_evm_ta.cpp)                  │ │
│  │                                                            │ │
│  │  static char g_ocall_buffer[8192];  // Local buffer       │ │
│  │  static size_t g_ocall_pos = 0;                           │ │
│  │                                                            │ │
│  │  test_string_with_ocall() {                               │ │
│  │    1. Reset buffer                                        │ │
│  │       g_ocall_pos = 0;                                    │ │
│  │                                                            │ │
│  │    2. Execute test with logging                           │ │
│  │       OCALL_LOG("Test 1...");  ───► Append to buffer     │ │
│  │       std::string str;                                    │ │
│  │       OCALL_LOG("size=%zu", str.size());  ───► Append     │ │
│  │                                                            │ │
│  │    3. Copy accumulated logs to host buffer                │ │
│  │       memcpy(params[1].memref.buffer,                     │ │
│  │              g_ocall_buffer, g_ocall_pos);                │ │
│  │  }                                                         │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. OCALL_LOG Macro Expansion

```
OCALL_LOG("[INFO] size=%zu", str.size())
           ↓
┌──────────────────────────────────────────────────────────┐
│ 1. Format string into temp buffer (512 bytes)           │
│    snprintf(ocall_tmp, 512, "[TA] [INFO] size=%zu\n",   │
│             str.size())                                  │
│    Result: "[TA] [INFO] size=0\n"                       │
└─────────────────────┬────────────────────────────────────┘
                      ↓
┌──────────────────────────────────────────────────────────┐
│ 2. Append to global buffer                               │
│    ocall_append(ocall_tmp)                               │
│                                                           │
│    g_ocall_buffer[g_ocall_pos..]  = "[TA] [INFO]..."   │
│    g_ocall_pos += strlen(ocall_tmp)                      │
└──────────────────────────────────────────────────────────┘
```

---

## 3. Memory Layout

```
NORMAL WORLD                      SECURE WORLD
┌──────────────────┐             ┌──────────────────┐
│  Host Stack      │             │  TA Global BSS   │
│                  │             │                  │
│  log_buffer[0]   │             │ g_ocall_buffer[0]│
│  log_buffer[1]   │             │ g_ocall_buffer[1]│
│  ...             │             │ ...              │
│  log_buffer[8191]│             │ g_ocall_buffer[  │
│                  │             │          8191]   │
└──────────────────┘             └──────────────────┘
         ▲                                │
         │                                │
         │      During InvokeCommand:     │
         │         memcpy(host ← TA)      │
         └────────────────────────────────┘
```

---

## 4. Command Execution Timeline

```
Time ───────────────────────────────────────────────────────►

Normal World (Host):
  │
  ├─► TEEC_InvokeCommand(cmd, log_buffer)
  │       │
  │       └─────────────────────────────────┐
  │                                         │
  │   [Waiting for TA to complete]         │
  │                                         │
  │                                         │
  │       ◄─────────────────────────────────┘
  │       │
  ├─► cout << log_buffer
  │   (prints all accumulated logs)
  │

Secure World (TA):
                  │
                  ├─► g_ocall_pos = 0 (reset)
                  │
                  ├─► OCALL_LOG("Test 1") ──► append [0..20]
                  │
                  ├─► OCALL_LOG("Test 2") ──► append [21..40]
                  │
                  ├─► OCALL_LOG("Test 3") ──► append [41..60]
                  │
                  ├─► memcpy(host_buffer, g_ocall_buffer, 60)
                  │
                  └─► Return TEE_SUCCESS
                      (control back to host)
```

---

## 5. Dual-Mode Parameter Checking

```
Host calls TA with different param_types:

MODE 1: WITH LOGS
┌────────────────────────────────────────────┐
│ param_types = (VALUE_INOUT,                │
│                MEMREF_OUTPUT, ◄── Log buf  │
│                NONE,                        │
│                NONE)                        │
└──────────────────┬─────────────────────────┘
                   │
                   ▼
         ┌─────────────────────┐
         │ TA checks:          │
         │ has_log_output=true │
         │ Enable OCALL        │
         │ Copy logs at end    │
         └─────────────────────┘

MODE 2: NO LOGS (backward compatible)
┌────────────────────────────────────────────┐
│ param_types = (VALUE_INOUT,                │
│                NONE,        ◄── No log     │
│                NONE,                        │
│                NONE)                        │
└──────────────────┬─────────────────────────┘
                   │
                   ▼
         ┌─────────────────────┐
         │ TA checks:          │
         │ has_log_output=false│
         │ Skip OCALL copy     │
         │ Still accumulates   │
         │ (for DMSG debug)    │
         └─────────────────────┘
```

---

## 6. Error Scenarios

### Scenario A: Buffer Overflow
```
TA Side:
  g_ocall_buffer[8192]
  g_ocall_pos = 8100
  
  OCALL_LOG("Very long message...") [200 bytes]
  
  Check: 8100 + 200 + 1 < 8192? 
         8301 < 8192? FALSE
  
  Action: Skip append (silent truncation)
  Warning: Last logs may be lost!
```

### Scenario B: Parameter Mismatch
```
Host sends:   (VALUE_INOUT, MEMREF_OUTPUT, NONE, NONE)
TA expects:   (VALUE_INOUT, NONE, NONE, NONE)

Result: TEE_ERROR_BAD_PARAMETERS (0xFFFF0006)

Solution: TA must check BOTH modes:
  if (param_types == with_log) { ... }
  else if (param_types == no_log) { ... }
  else return TEE_ERROR_BAD_PARAMETERS;
```

---

## 7. Comparison with Other Approaches

```
┌──────────────────────────────────────────────────────────────┐
│ Approach 1: Shared Memory + Background Thread (FAILED)      │
├──────────────────────────────────────────────────────────────┤
│                                                               │
│  Normal World: Background thread polls shared memory         │
│      while(true) {                                           │
│        read g_shared_buffer;  // ERROR: stale after return  │
│      }                                                       │
│                                                               │
│  Secure World: TA writes to shared memory                    │
│      OCALL_LOG(...) {                                        │
│        write g_shared_buffer; // ERROR: unmapped after cmd   │
│      }                                                       │
│                                                               │
│  Problem: OP-TEE unmaps shared memory after InvokeCommand   │
│           returns → TEE_ERROR_TARGET_DEAD (0xFFFF3024)      │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│ Approach 2: Simple Buffer (WORKS!)                          │
├──────────────────────────────────────────────────────────────┤
│                                                               │
│  Normal World: Wait for command completion                   │
│      TEEC_InvokeCommand(cmd);                               │
│      cout << log_buffer; // All logs at once                │
│                                                               │
│  Secure World: Accumulate in TA-local buffer                │
│      OCALL_LOG(...) {                                        │
│        append g_ocall_buffer; // Always valid               │
│      }                                                       │
│      // At end:                                              │
│      memcpy(host_buffer, g_ocall_buffer);                   │
│                                                               │
│  Success: Buffer lifecycle matches command execution         │
└──────────────────────────────────────────────────────────────┘
```

---

## 8. Complete Call Stack

```
main()
  │
  ├─► run_test_with_logs()
  │     │
  │     ├─► TEEC_InvokeCommand(TA_CMD_TEST_STRING_OCALL)
  │     │     │
  │     │     └──► [Switch to Secure World]
  │     │           │
  │     │           ├─► TA_InvokeCommandEntryPoint()
  │     │           │     │
  │     │           │     └─► test_string_with_ocall()
  │     │           │           │
  │     │           │           ├─► g_ocall_pos = 0
  │     │           │           │
  │     │           │           ├─► OCALL_LOG("Test 1")
  │     │           │           │     └─► ocall_append()
  │     │           │           │
  │     │           │           ├─► std::string str
  │     │           │           │
  │     │           │           ├─► OCALL_LOG("size=%zu")
  │     │           │           │     └─► ocall_append()
  │     │           │           │
  │     │           │           └─► memcpy(host, TA)
  │     │           │
  │     │           └──► [Return to Normal World]
  │     │
  │     ├─► cout << log_buffer
  │     │
  │     └─► Check pass/fail
  │
  └─► Print results
```

---

## 9. Data Flow Example

```
Step 1: Host prepares buffer
─────────────────────────────
Host:  log_buffer[8192] = { 0 }
       op.params[1].buffer = log_buffer

Step 2: Call TA
─────────────────────────────
Host → TA: TEEC_InvokeCommand()

Step 3: TA accumulates logs
─────────────────────────────
TA:    g_ocall_pos = 0
       g_ocall_buffer = ""

       OCALL_LOG("Test 1")
       g_ocall_buffer = "[TA] Test 1\n"
       g_ocall_pos = 12

       OCALL_LOG("size=0")
       g_ocall_buffer = "[TA] Test 1\n[TA] size=0\n"
       g_ocall_pos = 25

Step 4: TA copies to host
─────────────────────────────
TA:    memcpy(params[1].memref.buffer,
              g_ocall_buffer,
              25)

Step 5: Host receives logs
─────────────────────────────
Host:  log_buffer = "[TA] Test 1\n[TA] size=0\n"
       cout << log_buffer

Output:
[TA] Test 1
[TA] size=0
```

---

**End of Diagrams**

These diagrams illustrate the complete OCALL implementation architecture, from high-level flow to detailed memory operations.
