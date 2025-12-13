#!/usr/bin/env python3
"""
Patch exception.cc to remove OpenEnclave dependencies
"""
import re
import sys

def patch_exception_cc(input_file, output_file):
    with open(input_file, 'r') as f:
        lines = f.readlines()
    
    # Process line by line to insert undef before first stdlib.h
    output_lines = []
    stdlib_patched = False
    
    for line in lines:
        # Replace stdlib.h with our wrapper
        if '#include <stdlib.h>' in line:
            output_lines.append('#include "stdlib_wrapper.h"\n')
            continue
        
        # Remove OpenEnclave include
        if '#include <openenclave/internal/sgx/td.h>' in line:
            continue
        
        # Remove pthread.h include
        if '#include <pthread.h>' in line:
            continue
        
        output_lines.append(line)
    
    content = ''.join(output_lines)
    
    # Now do regex-based replacements on the full content
    
    # Add pthread_stub.h after cxxabi.h
    content = re.sub(
        r'(#include "cxxabi\.h")',
        r'\1\n#ifdef fallthrough\n#undef fallthrough\n#endif\n#include "../pthread_stub.h"',
        content
    )
    
    # 3. Replace thread_info() function - match actual whitespace pattern
    thread_info_pattern = r'static __cxa_thread_info \*thread_info\(\)\s*\{\s*oe_thread_data_t\* td = oe_get_thread_data\(\);\s*return \(__cxa_thread_info\*\) td->__cxx_thread_info;\s*\}'
    thread_info_replacement = '''static __cxa_thread_info *thread_info()
{
    static pthread_key_t key = 0;
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    
    // Initialize TLS key once
    static auto init_fn = []() { pthread_key_create(&key, nullptr); };
    pthread_once(&once, +init_fn);
    
    // Get or create thread info
    __cxa_thread_info* info = (__cxa_thread_info*)pthread_getspecific(key);
    if (!info) {
        info = new __cxa_thread_info();
        pthread_setspecific(key, info);
    }
    return info;
}'''
    content = re.sub(thread_info_pattern, thread_info_replacement, content, flags=re.DOTALL)
    
    # 4. Replace thread_info_fast() function
    thread_info_fast_pattern = r'static __cxa_thread_info \*thread_info_fast\(\)\s*\{\s*oe_thread_data_t\* td = oe_get_thread_data\(\);\s*return \(__cxa_thread_info\*\) td->__cxx_thread_info;\s*\}'
    thread_info_fast_replacement = '''static __cxa_thread_info *thread_info_fast()
{
    return thread_info(); // Same as thread_info() in single-threaded case
}'''
    content = re.sub(thread_info_fast_pattern, thread_info_fast_replacement, content, flags=re.DOTALL)
    
    # 5. Fix PTHREAD_COND_INITIALIZER 
    content = re.sub(
        r'pthread_cond_t emergency_malloc_wait = PTHREAD_COND_INITIALIZER;',
        'pthread_cond_t emergency_malloc_wait = { 0 };',
        content
    )
    
    # 6. Stub out trace() function - find opening brace and matching closing brace
    # This is complex because function body spans multiple lines with nested braces
    trace_start = content.find('static _Unwind_Reason_Code trace(struct _Unwind_Context *context, void *c)')
    if trace_start != -1:
        # Find opening brace
        brace_start = content.find('{', trace_start)
        if brace_start != -1:
            # Find matching closing brace
            depth = 1
            pos = brace_start + 1
            while pos < len(content) and depth > 0:
                if content[pos] == '{':
                    depth += 1
                elif content[pos] == '}':
                    depth -= 1
                pos += 1
            
            if depth == 0:
                # Replace function body
                new_body = '''\n\t(void)context; (void)c; // Stubbed - dladdr not available
\treturn _URC_CONTINUE_UNWIND;\n'''
                content = content[:brace_start+1] + new_body + content[pos-1:]
    
    # 7. Remove comment about oe_thread_data_t
    content = re.sub(r'// For accessing oe_thread_data_t\n', '', content)
    
    with open(output_file, 'w') as f:
        f.write(content)
    
    print(f"✓ Patched {input_file} -> {output_file}")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input> <output>")
        sys.exit(1)
    patch_exception_cc(sys.argv[1], sys.argv[2])
