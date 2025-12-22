//  Copyright (c) 2011 The LevelDB Authors. All rights reserved.
//  Use of this source code is governed by a BSD-style license that can be
//  found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// OP-TEE Port: Single-threaded environment with dummy mutex/condvar

#ifndef STORAGE_LEVELDB_PORT_PORT_TEE_H_
#define STORAGE_LEVELDB_PORT_PORT_TEE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <cstring>

// Define platform macros
#undef PLATFORM_IS_LITTLE_ENDIAN
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define PLATFORM_IS_LITTLE_ENDIAN true
#else
#define PLATFORM_IS_LITTLE_ENDIAN false
#endif

namespace leveldb {
namespace port {

// Dummy Mutex for single-threaded OP-TEE environment
class Mutex {
 public:
  Mutex() = default;
  ~Mutex() = default;

  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void Lock() {}
  void Unlock() {}
  void AssertHeld() {}
};

// Dummy CondVar for single-threaded OP-TEE environment  
class CondVar {
 public:
  explicit CondVar(Mutex* mu) : mu_(mu) {}
  ~CondVar() = default;

  CondVar(const CondVar&) = delete;
  CondVar& operator=(const CondVar&) = delete;

  void Wait() {}
  void Signal() {}
  void SignalAll() {}

 private:
  Mutex* const mu_;
};

// Snappy compression is not supported
inline bool Snappy_Compress(const char* input, size_t length,
                             std::string* output) {
  return false;
}

inline bool Snappy_GetUncompressedLength(const char* input, size_t length,
                                          size_t* result) {
  return false;
}

inline bool Snappy_Uncompress(const char* input, size_t length, char* output) {
  return false;
}

// CRC32C implementation
uint32_t AcceleratedCRC32C(uint32_t crc, const char* buf, size_t size);

}  // namespace port
}  // namespace leveldb

#endif  // STORAGE_LEVELDB_PORT_PORT_TEE_H_
