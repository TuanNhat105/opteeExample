// SPDX-License-Identifier: BSD-2-Clause
/*
 * Stub for leveldb::Env::Default()
 */

#include <leveldb/env.h>

namespace leveldb {

Env* Env::Default() {
    // This should never be called if we always provide custom env
    // Return nullptr to catch misuse
    return nullptr;
}

} // namespace leveldb
