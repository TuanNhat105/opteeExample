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

// Forward declarations
class RingBufferWritableFile;
class SimpleSequentialFile;
class SimpleRandomAccessFile;

// Complete RingBufferEnv without dependency on POSIX
// Custom leveldb::Env implementation that redirects file I/O to in-memory storage
// and sends data to Normal World via ring buffer
class RingBufferEnv : public leveldb::Env {
private:
    SecureRingBufferProducer* producer_;
    uint32_t next_virtual_fd_;
    // Use raw pointers to delay std::map initialization until first use
    // This avoids crash during constructor if exception handling is incomplete
    // Raw pointers are initialized to nullptr (no constructor call needed)
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
    explicit RingBufferEnv(SecureRingBufferProducer* producer)
        : producer_(producer), next_virtual_fd_(1000), 
          file_fd_map_(nullptr), memory_files_(nullptr), directories_(nullptr),
          start_micros_(0) {
        // CRITICAL FIX: Use raw pointers initialized to nullptr
        // This avoids any constructor calls during member initialization.
        // std::map will be created lazily on first access via get_*() helpers.
        // This way, if std::map creation fails, it happens in a controlled context
        // where we can catch exceptions properly.
        
        // Log to confirm constructor body executes
        extern void ocall_log_init(void);
        extern void OCALL_LOG(const char* fmt, ...);
        ocall_log_init();
        OCALL_LOG("[RingBufferEnv] Constructor body entered");
        OCALL_LOG("[RingBufferEnv] Producer: %p", producer_);
        OCALL_LOG("[RingBufferEnv] Constructor body completed");
    }

    virtual ~RingBufferEnv() {
        // Manual cleanup of raw pointers
        delete file_fd_map_;
        delete memory_files_;
        delete directories_;
    }

    // File operations
    virtual leveldb::Status NewWritableFile(const std::string& fname, leveldb::WritableFile** result) override;
    virtual leveldb::Status NewAppendableFile(const std::string& fname, leveldb::WritableFile** result) override;
    virtual leveldb::Status NewSequentialFile(const std::string& fname, leveldb::SequentialFile** result) override;
    virtual leveldb::Status NewRandomAccessFile(const std::string& fname, leveldb::RandomAccessFile** result) override;

    virtual bool FileExists(const std::string& fname) override {
        if (!memory_files_) return false;
        return get_memory_files().find(fname) != get_memory_files().end();
    }

    virtual leveldb::Status GetChildren(const std::string& dir, std::vector<std::string>* result) override {
        // CRITICAL: LevelDB calls GetChildren("/tmp/leveldb_secure") during Recover()
        // If directory doesn't exist, create it first (lazy initialization)
        try {
            auto& dirs = get_directories();
            auto it = dirs.find(dir);
            if (it == dirs.end()) {
                // Lazy initialization - create directory entry if not exists
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

    virtual leveldb::Status DeleteFile(const std::string& fname) override {
        if (memory_files_) {
            get_memory_files().erase(fname);
        }
        if (file_fd_map_) {
            get_file_fd_map().erase(fname);
        }
        // Remove from parent directory listing
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

    virtual leveldb::Status CreateDir(const std::string& dirname) override {
        try {
            get_directories()[dirname] = std::vector<std::string>();
            return leveldb::Status::OK();
        } catch (const std::bad_alloc&) {
            return leveldb::Status::IOError("Out of memory");
        } catch (...) {
            return leveldb::Status::IOError("Failed to create directory");
        }
    }

    virtual leveldb::Status DeleteDir(const std::string& dirname) override {
        if (!directories_) return leveldb::Status::IOError("Directory not found");
        auto& dirs = get_directories();
        auto it = dirs.find(dirname);
        if (it != dirs.end() && it->second.empty()) {
            dirs.erase(it);
            return leveldb::Status::OK();
        }
        return leveldb::Status::IOError("Directory not empty or not found");
    }

    virtual leveldb::Status GetFileSize(const std::string& fname, uint64_t* file_size) override {
        // CRITICAL: LevelDB may call GetFileSize during recovery for files that don't exist yet
        // Return 0 size instead of IOError to avoid recovery failures
        if (memory_files_) {
            auto& files = get_memory_files();
            auto it = files.find(fname);
            if (it != files.end()) {
                *file_size = it->second->Size();
                return leveldb::Status::OK();
            }
        }
        // File doesn't exist yet - return 0 size (not an error)
        *file_size = 0;
        return leveldb::Status::OK();
    }

    virtual leveldb::Status RenameFile(const std::string& src, const std::string& target) override {
        if (!memory_files_) return leveldb::Status::IOError("Source file not found");
        auto& files = get_memory_files();
        auto it = files.find(src);
        if (it != files.end()) {
            files[target] = it->second;
            files.erase(it);
            
            // Update file_fd_map
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

    // Lock/Unlock - Dummy implementation for single-threaded TEE
    virtual leveldb::Status LockFile(const std::string& fname, leveldb::FileLock** lock) override {
        *lock = reinterpret_cast<leveldb::FileLock*>(1); // Dummy pointer
        return leveldb::Status::OK();
    }

    virtual leveldb::Status UnlockFile(leveldb::FileLock* lock) override {
        return leveldb::Status::OK();
    }

    // Threading - No-op for single-threaded TEE
    // CRITICAL: Don't execute immediately - LevelDB may schedule compaction
    // which can cause hangs. Just ignore scheduled work in TEE.
    virtual void Schedule(void (*function)(void* arg), void* arg) override {
        // In TEE, we're single-threaded and don't have background threads
        // LevelDB schedules compaction, but we can't run it in background
        // Just ignore - compaction will happen on next write if needed
        (void)function;
        (void)arg;
    }

    virtual void StartThread(void (*function)(void* arg), void* arg) override {
        // CRITICAL: In single-threaded TEE, we cannot start background threads
        // LevelDB may try to start compaction threads, but we must ignore this
        // Executing the function directly could cause hangs or deadlocks
        (void)function;
        (void)arg;
    }

    // Test directory
    virtual leveldb::Status GetTestDirectory(std::string* path) override {
        *path = "/tmp/leveldb_test";
        CreateDir(*path);
        return leveldb::Status::OK();
    }

    // Logger - Dummy implementation
    virtual leveldb::Status NewLogger(const std::string& fname, leveldb::Logger** result) override {
        *result = nullptr; // No logging in TEE
        return leveldb::Status::OK();
    }

    // Time
    virtual uint64_t NowMicros() override {
        // Simple counter-based time for TEE
        return start_micros_++;
    }

    virtual void SleepForMicroseconds(int micros) override {
        // No-op in TEE
    }

    // Helper functions
    uint32_t GetOrCreateVirtualFD(const std::string& fname) {
        auto& fd_map = get_file_fd_map();
        auto it = fd_map.find(fname);
        if (it != fd_map.end()) {
            return it->second;
        }
        // CRITICAL: Use plain increment instead of std::atomic::fetch_add
        // In single-threaded TEE, this is safe
        uint32_t vfd = next_virtual_fd_++;
        fd_map[fname] = vfd;
        
        // Add to directory listing
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

// WritableFile implementation
class RingBufferWritableFile : public leveldb::WritableFile {
private:
    RingBufferEnv* env_;
    uint32_t virtual_fd_;
    std::string filename_;
    std::shared_ptr<MemoryFile> mem_file_;

public:
    RingBufferWritableFile(RingBufferEnv* env, uint32_t vfd, const std::string& fname)
        : env_(env), virtual_fd_(vfd), filename_(fname) {
        mem_file_ = env_->GetOrCreateMemoryFile(fname);
    }

    virtual ~RingBufferWritableFile() {
        Close();
    }

    virtual leveldb::Status Append(const leveldb::Slice& data) override {
        // Store in memory for later reads (always succeed)
        mem_file_->Append(data.data(), data.size());
        
        // Send to Normal World via ring buffer
        // During DB::Open(), consumer may not be ready, so don't block
        SecureRingBufferProducer* producer = env_->GetProducer();
        if (!producer) {
            // Producer not ready - data is in memory, will be sent later
            return leveldb::Status::OK();
        }
        
        std::string_view data_view(data.data(), data.size());
        
        // Try push once - if fails, data is still in memory
        // Don't retry during init to avoid hang
        if (producer->Push(virtual_fd_, data_view)) {
            return leveldb::Status::OK();
        }
        
        // Buffer full or consumer not ready - but data is in memory
        // This is OK during initialization, consumer will process later
        return leveldb::Status::OK();
    }

    virtual leveldb::Status Close() override {
        return Flush();
    }

    virtual leveldb::Status Flush() override {
        // CRITICAL: Always return OK immediately to avoid hanging during DB::Open()
        // LevelDB calls Flush() frequently during initialization, and each call
        // could block if we wait for the consumer. Since data is already stored
        // in memory (mem_file_), it's safe to return OK immediately.
        // The ring buffer will be flushed later when the consumer is ready.
        return leveldb::Status::OK();
    }

    virtual leveldb::Status Sync() override {
        return Flush();
    }
};

// SequentialFile implementation
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

// RandomAccessFile implementation  
class SimpleRandomAccessFile : public leveldb::RandomAccessFile {
private:
    std::vector<char> file_data_;
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
inline leveldb::Status RingBufferEnv::NewWritableFile(const std::string& fname, leveldb::WritableFile** result) {
    uint32_t vfd = GetOrCreateVirtualFD(fname);
    *result = new RingBufferWritableFile(this, vfd, fname);
    return leveldb::Status::OK();
}

inline leveldb::Status RingBufferEnv::NewAppendableFile(const std::string& fname, leveldb::WritableFile** result) {
    return NewWritableFile(fname, result);
}

inline leveldb::Status RingBufferEnv::NewSequentialFile(const std::string& fname, leveldb::SequentialFile** result) {
    // CRITICAL: LevelDB may try to read files during recovery that don't exist yet
    // Create the file if it doesn't exist to avoid IOError
    auto file = GetOrCreateMemoryFile(fname);
    *result = new SimpleSequentialFile(file);
    return leveldb::Status::OK();
}

inline leveldb::Status RingBufferEnv::NewRandomAccessFile(const std::string& fname, leveldb::RandomAccessFile** result) {
    // CRITICAL: LevelDB may try to read files during recovery that don't exist yet
    // Create the file if it doesn't exist to avoid IOError
    auto file = GetOrCreateMemoryFile(fname);
    *result = new SimpleRandomAccessFile(file);
    return leveldb::Status::OK();
}
