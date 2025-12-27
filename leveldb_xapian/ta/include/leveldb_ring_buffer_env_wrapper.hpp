#pragma once
#include "secure_ring_buffer_producer.hpp"
#include "ocall_logger.h"
#include <leveldb/env.h>
#include <leveldb/slice.h>
#include <leveldb/status.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Forward declarations
class RingBufferEnvImpl;
class RingBufferWritableFile;
class SimpleSequentialFile;
class SimpleRandomAccessFile;

// Simple in-memory file for TEE environment (no POSIX dependencies)
class MemoryFile {
public:
    std::vector<char> data;
    size_t read_pos = 0;
    
    void Append(const char* buf, size_t size) {
        data.insert(data.end(), buf, buf + size);
    }
    
    size_t Read(size_t n, char* scratch) {
        size_t available = data.size() - read_pos;
        size_t to_read = (n < available) ? n : available;
        if (to_read > 0) {
            std::memcpy(scratch, data.data() + read_pos, to_read);
            read_pos += to_read;
        }
        return to_read;
    }
    
    size_t Size() const { return data.size(); }
};

// Implementation class - NO inheritance from leveldb::Env
// This avoids potential issues with base class initialization
class RingBufferEnvImpl {
private:
    SecureRingBufferProducer* producer_;
    uint32_t next_virtual_fd_;
    // Use raw pointers to delay std::map initialization until first use
    std::map<std::string, uint32_t>* file_fd_map_;
    std::map<std::string, std::shared_ptr<MemoryFile>>* memory_files_;
    std::map<std::string, std::vector<std::string>>* directories_;
    uint64_t start_micros_;
    
    // Lazy initialization helpers
    std::map<std::string, uint32_t>& get_file_fd_map() {
        if (!file_fd_map_) {
            file_fd_map_ = new std::map<std::string, uint32_t>();
        }
        return *file_fd_map_;
    }
    
    std::map<std::string, std::shared_ptr<MemoryFile>>& get_memory_files() {
        if (!memory_files_) {
            memory_files_ = new std::map<std::string, std::shared_ptr<MemoryFile>>();
        }
        return *memory_files_;
    }
    
    std::map<std::string, std::vector<std::string>>& get_directories() {
        if (!directories_) {
            directories_ = new std::map<std::string, std::vector<std::string>>();
        }
        return *directories_;
    }

public:
    explicit RingBufferEnvImpl(SecureRingBufferProducer* producer)
        : producer_(producer), next_virtual_fd_(1000), 
          file_fd_map_(nullptr), memory_files_(nullptr), directories_(nullptr),
          start_micros_(0) {
        // CRITICAL: No base class to initialize - just simple member initialization
        // All std::map initialization is delayed until first use
        extern void ocall_log_init(void);
        extern void OCALL_LOG(const char* fmt, ...);
        ocall_log_init();
        OCALL_LOG("[RingBufferEnvImpl] Constructor body entered");
        OCALL_LOG("[RingBufferEnvImpl] Producer: %p", producer_);
        OCALL_LOG("[RingBufferEnvImpl] Constructor body completed");
    }

    ~RingBufferEnvImpl() {
        delete file_fd_map_;
        delete memory_files_;
        delete directories_;
    }

    // File operations - same interface as leveldb::Env but without inheritance
    leveldb::Status NewWritableFile(const std::string& fname, leveldb::WritableFile** result);
    leveldb::Status NewAppendableFile(const std::string& fname, leveldb::WritableFile** result);
    leveldb::Status NewSequentialFile(const std::string& fname, leveldb::SequentialFile** result);
    leveldb::Status NewRandomAccessFile(const std::string& fname, leveldb::RandomAccessFile** result);

    bool FileExists(const std::string& fname) {
        if (!memory_files_) return false;
        return get_memory_files().find(fname) != get_memory_files().end();
    }

    leveldb::Status GetChildren(const std::string& dir, std::vector<std::string>* result) {
        try {
            auto& dirs = get_directories();
            auto it = dirs.find(dir);
            if (it == dirs.end()) {
                dirs[dir] = std::vector<std::string>();
                it = dirs.find(dir);
            }
            if (it != dirs.end()) {
                *result = it->second;
                return leveldb::Status::OK();
            }
            return leveldb::Status::IOError("Directory not found");
        } catch (const std::bad_alloc&) {
            return leveldb::Status::IOError("Out of memory");
        } catch (...) {
            return leveldb::Status::IOError("Failed to create directory");
        }
    }

    leveldb::Status DeleteFile(const std::string& fname) {
        if (memory_files_) {
            get_memory_files().erase(fname);
        }
        if (file_fd_map_) {
            get_file_fd_map().erase(fname);
        }
        size_t last_slash = fname.find_last_of('/');
        std::string dir = (last_slash == std::string::npos) ? "" : fname.substr(0, last_slash);
        std::string basename = (last_slash == std::string::npos) ? fname : fname.substr(last_slash + 1);
        if (directories_) {
            auto& dirs = get_directories();
            auto dir_it = dirs.find(dir);
            if (dir_it != dirs.end()) {
                auto& files = dir_it->second;
                files.erase(std::remove(files.begin(), files.end(), basename), files.end());
            }
        }
        return leveldb::Status::OK();
    }

    leveldb::Status CreateDir(const std::string& dirname) {
        try {
            get_directories()[dirname] = std::vector<std::string>();
            return leveldb::Status::OK();
        } catch (const std::bad_alloc&) {
            return leveldb::Status::IOError("Out of memory");
        } catch (...) {
            return leveldb::Status::IOError("Failed to create directory");
        }
    }

    leveldb::Status DeleteDir(const std::string& dirname) {
        if (!directories_) return leveldb::Status::IOError("Directory not found");
        auto& dirs = get_directories();
        auto it = dirs.find(dirname);
        if (it != dirs.end() && it->second.empty()) {
            dirs.erase(it);
            return leveldb::Status::OK();
        }
        return leveldb::Status::IOError("Directory not empty or not found");
    }

    leveldb::Status GetFileSize(const std::string& fname, uint64_t* file_size) {
        if (memory_files_) {
            auto& files = get_memory_files();
            auto it = files.find(fname);
            if (it != files.end()) {
                *file_size = it->second->Size();
                return leveldb::Status::OK();
            }
        }
        *file_size = 0;
        return leveldb::Status::OK();
    }

    leveldb::Status RenameFile(const std::string& src, const std::string& target) {
        if (!memory_files_) return leveldb::Status::IOError("Source file not found");
        auto& files = get_memory_files();
        auto it = files.find(src);
        if (it != files.end()) {
            files[target] = it->second;
            files.erase(it);
            if (file_fd_map_) {
                auto& fd_map = get_file_fd_map();
                auto fd_it = fd_map.find(src);
                if (fd_it != fd_map.end()) {
                    uint32_t fd = fd_it->second;
                    fd_map.erase(fd_it);
                    fd_map[target] = fd;
                }
            }
            return leveldb::Status::OK();
        }
        return leveldb::Status::IOError("Source file not found");
    }

    leveldb::Status LockFile(const std::string& fname, leveldb::FileLock** lock) {
        *lock = reinterpret_cast<leveldb::FileLock*>(1);
        return leveldb::Status::OK();
    }

    leveldb::Status UnlockFile(leveldb::FileLock* lock) {
        return leveldb::Status::OK();
    }

    void Schedule(void (*function)(void* arg), void* arg) {
        (void)function;
        (void)arg;
    }

    void StartThread(void (*function)(void* arg), void* arg) {
        (void)function;
        (void)arg;
    }

    leveldb::Status GetTestDirectory(std::string* path) {
        *path = "/tmp/leveldb_test";
        CreateDir(*path);
        return leveldb::Status::OK();
    }

    leveldb::Status NewLogger(const std::string& fname, leveldb::Logger** result) {
        *result = nullptr;
        return leveldb::Status::OK();
    }

    uint64_t NowMicros() {
        return start_micros_++;
    }

    void SleepForMicroseconds(int micros) {
        (void)micros;
    }

    // Helper functions
    uint32_t GetOrCreateVirtualFD(const std::string& fname) {
        auto& fd_map = get_file_fd_map();
        auto it = fd_map.find(fname);
        if (it != fd_map.end()) {
            return it->second;
        }
        uint32_t vfd = next_virtual_fd_++;
        fd_map[fname] = vfd;
        size_t last_slash = fname.find_last_of('/');
        std::string dir = (last_slash == std::string::npos) ? "" : fname.substr(0, last_slash);
        std::string basename = (last_slash == std::string::npos) ? fname : fname.substr(last_slash + 1);
        get_directories()[dir].push_back(basename);
        return vfd;
    }

    std::shared_ptr<MemoryFile> GetOrCreateMemoryFile(const std::string& fname) {
        auto& files = get_memory_files();
        auto it = files.find(fname);
        if (it != files.end()) {
            return it->second;
        }
        auto file = std::make_shared<MemoryFile>();
        files[fname] = file;
        return file;
    }

    SecureRingBufferProducer* GetProducer() { return producer_; }
};

// CRITICAL FIX: Try to avoid vtable issues by using a different approach
// Instead of inheriting directly, we'll create a minimal wrapper that
// only sets up the vtable pointer, then initializes everything else later

// Forward declaration
class RingBufferEnvImpl;

// CRITICAL FIX: Revert to direct inheritance from leveldb::Env
// EnvWrapper requires an Env* target which we don't have
// We'll try the simplest possible approach: minimal constructor with logging
class RingBufferEnv : public leveldb::Env {
private:
    RingBufferEnvImpl* impl_;
    SecureRingBufferProducer* producer_;  // Store producer for lazy initialization
    
    // Private constructor - use Create() factory function instead
    explicit RingBufferEnv(SecureRingBufferProducer* producer)
        : leveldb::Env(),  // Base class - very simple default constructor
          impl_(nullptr),  // Will be initialized later
          producer_(producer) {
        // CRITICAL: Add logging to see if we reach constructor body
        // This will help us determine if crash is in base class constructor or here
        extern void ocall_log_init(void);
        extern void OCALL_LOG(const char* fmt, ...);
        ocall_log_init();
        OCALL_LOG("[RingBufferEnv] Constructor body entered");
        OCALL_LOG("[RingBufferEnv] this = %p", this);
        OCALL_LOG("[RingBufferEnv] producer_ = %p", producer_);
        OCALL_LOG("[RingBufferEnv] impl_ = %p", impl_);
        OCALL_LOG("[RingBufferEnv] Constructor body completed");
    }
    
public:
    // Factory function to create RingBufferEnv safely
    // This ensures vtable is set up before any complex operations
    static RingBufferEnv* Create(SecureRingBufferProducer* producer, void* storage) {
        extern void ocall_log_init(void);
        extern void OCALL_LOG(const char* fmt, ...);
        ocall_log_init();
        OCALL_LOG("[RingBufferEnv] Create() factory called");
        OCALL_LOG("[RingBufferEnv] Storage: %p, Producer: %p", storage, producer);
        
        OCALL_LOG("[RingBufferEnv] About to call placement new...");
        OCALL_LOG("[RingBufferEnv] sizeof(RingBufferEnv) = %zu", sizeof(RingBufferEnv));
        OCALL_LOG("[RingBufferEnv] alignof(RingBufferEnv) = %zu", alignof(RingBufferEnv));
        
        // Construct in provided storage
        // CRITICAL: This is where crash might happen - during base class construction
        RingBufferEnv* env = nullptr;
        try {
            OCALL_LOG("[RingBufferEnv] Calling placement new operator...");
            env = new (storage) RingBufferEnv(producer);
            OCALL_LOG("[RingBufferEnv] Placement new completed, object at %p", env);
        } catch (const std::exception& e) {
            OCALL_LOG("[RingBufferEnv] EXCEPTION in placement new: %s", e.what());
            throw;
        } catch (...) {
            OCALL_LOG("[RingBufferEnv] UNKNOWN EXCEPTION in placement new");
            throw;
        }
        
        OCALL_LOG("[RingBufferEnv] Object constructed successfully");
        OCALL_LOG("[RingBufferEnv] About to call Initialize()...");
        
        // Initialize impl_ AFTER object is fully constructed
        env->Initialize();
        OCALL_LOG("[RingBufferEnv] Initialize() completed");
        OCALL_LOG("[RingBufferEnv] Create() factory completed successfully");
        
        return env;
    }
    
    // Separate initialization method - call this AFTER object construction
    // This avoids vtable issues during constructor
    void Initialize() {
        extern void ocall_log_init(void);
        extern void OCALL_LOG(const char* fmt, ...);
        extern void* TEE_Malloc(size_t size, uint32_t hint);
        extern void TEE_Free(void* buffer);
        ocall_log_init();
        OCALL_LOG("[RingBufferEnv] Initialize() called, creating impl...");
        
        if (impl_) {
            OCALL_LOG("[RingBufferEnv] Already initialized");
            return;
        }
        
        // Allocate memory for impl using TEE_Malloc
        size_t impl_size = sizeof(RingBufferEnvImpl);
        void* impl_mem = TEE_Malloc(impl_size, 0);
        if (!impl_mem) {
            OCALL_LOG("[RingBufferEnv] TEE_Malloc failed for impl");
            throw std::bad_alloc();
        }
        OCALL_LOG("[RingBufferEnv] Memory allocated for impl at: %p", impl_mem);
        
        try {
            OCALL_LOG("[RingBufferEnv] About to construct RingBufferEnvImpl using placement new...");
            impl_ = new (impl_mem) RingBufferEnvImpl(producer_);
            OCALL_LOG("[RingBufferEnv] Impl created successfully, pointer: %p", impl_);
        } catch (const std::exception& e) {
            OCALL_LOG("[RingBufferEnv] Exception creating impl: %s", e.what());
            TEE_Free(impl_mem);
            throw;
        } catch (...) {
            OCALL_LOG("[RingBufferEnv] Unknown exception creating impl");
            TEE_Free(impl_mem);
            throw;
        }
        
        OCALL_LOG("[RingBufferEnv] Initialize() completed");
    }

    virtual ~RingBufferEnv() {
        if (impl_) {
            extern void TEE_Free(void* buffer);
            impl_->~RingBufferEnvImpl();
            TEE_Free(impl_);
        }
    }
    
    // Check if initialized
    bool IsInitialized() const { return impl_ != nullptr; }

    // Delegate all calls to impl_
    // Lazy initialization in each method to avoid vtable issues
    virtual leveldb::Status NewWritableFile(const std::string& fname, leveldb::WritableFile** result) override {
        if (!impl_) Initialize();
        return impl_->NewWritableFile(fname, result);
    }

    virtual leveldb::Status NewAppendableFile(const std::string& fname, leveldb::WritableFile** result) override {
        return impl_->NewAppendableFile(fname, result);
    }

    virtual leveldb::Status NewSequentialFile(const std::string& fname, leveldb::SequentialFile** result) override {
        return impl_->NewSequentialFile(fname, result);
    }

    virtual leveldb::Status NewRandomAccessFile(const std::string& fname, leveldb::RandomAccessFile** result) override {
        return impl_->NewRandomAccessFile(fname, result);
    }

    virtual bool FileExists(const std::string& fname) override {
        return impl_->FileExists(fname);
    }

    virtual leveldb::Status GetChildren(const std::string& dir, std::vector<std::string>* result) override {
        return impl_->GetChildren(dir, result);
    }

    virtual leveldb::Status DeleteFile(const std::string& fname) override {
        return impl_->DeleteFile(fname);
    }

    virtual leveldb::Status CreateDir(const std::string& dirname) override {
        return impl_->CreateDir(dirname);
    }

    virtual leveldb::Status DeleteDir(const std::string& dirname) override {
        return impl_->DeleteDir(dirname);
    }

    virtual leveldb::Status GetFileSize(const std::string& fname, uint64_t* file_size) override {
        return impl_->GetFileSize(fname, file_size);
    }

    virtual leveldb::Status RenameFile(const std::string& src, const std::string& target) override {
        return impl_->RenameFile(src, target);
    }

    virtual leveldb::Status LockFile(const std::string& fname, leveldb::FileLock** lock) override {
        return impl_->LockFile(fname, lock);
    }

    virtual leveldb::Status UnlockFile(leveldb::FileLock* lock) override {
        return impl_->UnlockFile(lock);
    }

    virtual void Schedule(void (*function)(void* arg), void* arg) override {
        impl_->Schedule(function, arg);
    }

    virtual void StartThread(void (*function)(void* arg), void* arg) override {
        impl_->StartThread(function, arg);
    }

    virtual leveldb::Status GetTestDirectory(std::string* path) override {
        return impl_->GetTestDirectory(path);
    }

    virtual leveldb::Status NewLogger(const std::string& fname, leveldb::Logger** result) override {
        return impl_->NewLogger(fname, result);
    }

    virtual uint64_t NowMicros() override {
        return impl_->NowMicros();
    }

    virtual void SleepForMicroseconds(int micros) override {
        impl_->SleepForMicroseconds(micros);
    }
};

// File implementations (need to be defined after RingBufferEnvImpl)
class RingBufferWritableFile : public leveldb::WritableFile {
private:
    RingBufferEnvImpl* env_;
    uint32_t virtual_fd_;
    std::string filename_;
    std::shared_ptr<MemoryFile> mem_file_;

public:
    RingBufferWritableFile(RingBufferEnvImpl* env, uint32_t vfd, const std::string& fname)
        : env_(env), virtual_fd_(vfd), filename_(fname) {
        mem_file_ = env_->GetOrCreateMemoryFile(fname);
    }

    virtual ~RingBufferWritableFile() {
        Close();
    }

    virtual leveldb::Status Append(const leveldb::Slice& data) override {
        mem_file_->Append(data.data(), data.size());
        SecureRingBufferProducer* producer = env_->GetProducer();
        if (!producer) {
            return leveldb::Status::OK();
        }
        std::string_view data_view(data.data(), data.size());
        if (producer->Push(virtual_fd_, data_view)) {
            return leveldb::Status::OK();
        }
        return leveldb::Status::OK();
    }

    virtual leveldb::Status Close() override {
        return Flush();
    }

    virtual leveldb::Status Flush() override {
        return leveldb::Status::OK();
    }

    virtual leveldb::Status Sync() override {
        return Flush();
    }
};

class SimpleSequentialFile : public leveldb::SequentialFile {
private:
    std::shared_ptr<MemoryFile> file_;

public:
    explicit SimpleSequentialFile(std::shared_ptr<MemoryFile> file) : file_(file) {}
    virtual ~SimpleSequentialFile() = default;

    virtual leveldb::Status Read(size_t n, leveldb::Slice* result, char* scratch) override {
        size_t bytes_read = file_->Read(n, scratch);
        *result = leveldb::Slice(scratch, bytes_read);
        return leveldb::Status::OK();
    }

    virtual leveldb::Status Skip(uint64_t n) override {
        file_->read_pos += n;
        if (file_->read_pos > file_->Size()) {
            file_->read_pos = file_->Size();
        }
        return leveldb::Status::OK();
    }
};

class SimpleRandomAccessFile : public leveldb::RandomAccessFile {
private:
    std::shared_ptr<MemoryFile> file_;

public:
    explicit SimpleRandomAccessFile(std::shared_ptr<MemoryFile> file) : file_(file) {}
    virtual ~SimpleRandomAccessFile() = default;

    virtual leveldb::Status Read(uint64_t offset, size_t n, leveldb::Slice* result,
                                  char* scratch) const override {
        if (offset >= file_->Size()) {
            *result = leveldb::Slice();
            return leveldb::Status::OK();
        }
        size_t available = file_->Size() - offset;
        size_t to_read = (n < available) ? n : available;
        std::memcpy(scratch, file_->data.data() + offset, to_read);
        *result = leveldb::Slice(scratch, to_read);
        return leveldb::Status::OK();
    }
};

// Implementations
inline leveldb::Status RingBufferEnvImpl::NewWritableFile(const std::string& fname, leveldb::WritableFile** result) {
    uint32_t vfd = GetOrCreateVirtualFD(fname);
    *result = new RingBufferWritableFile(this, vfd, fname);
    return leveldb::Status::OK();
}

inline leveldb::Status RingBufferEnvImpl::NewAppendableFile(const std::string& fname, leveldb::WritableFile** result) {
    return NewWritableFile(fname, result);
}

inline leveldb::Status RingBufferEnvImpl::NewSequentialFile(const std::string& fname, leveldb::SequentialFile** result) {
    auto file = GetOrCreateMemoryFile(fname);
    *result = new SimpleSequentialFile(file);
    return leveldb::Status::OK();
}

inline leveldb::Status RingBufferEnvImpl::NewRandomAccessFile(const std::string& fname, leveldb::RandomAccessFile** result) {
    auto file = GetOrCreateMemoryFile(fname);
    *result = new SimpleRandomAccessFile(file);
    return leveldb::Status::OK();
}

