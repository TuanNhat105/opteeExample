#pragma once
#include "secure_ring_buffer_producer.hpp"
#include <leveldb/env.h>
#include <leveldb/slice.h>
#include <leveldb/status.h>
#include <map>
#include <memory>
#include <string>
#include <atomic>
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
class RingBufferEnv : public leveldb::Env {
private:
    SecureRingBufferProducer* producer_;
    std::atomic<uint32_t> next_virtual_fd_{1000};
    std::map<std::string, uint32_t> file_fd_map_;
    std::map<std::string, std::shared_ptr<MemoryFile>> memory_files_;
    std::map<std::string, std::vector<std::string>> directories_;
    uint64_t start_micros_;

public:
    explicit RingBufferEnv(SecureRingBufferProducer* producer)
        : producer_(producer), start_micros_(0) {
        directories_[""] = std::vector<std::string>(); // Root directory
    }

    virtual ~RingBufferEnv() {}

    // File operations
    virtual leveldb::Status NewWritableFile(const std::string& fname, leveldb::WritableFile** result) override;
    virtual leveldb::Status NewAppendableFile(const std::string& fname, leveldb::WritableFile** result) override;
    virtual leveldb::Status NewSequentialFile(const std::string& fname, leveldb::SequentialFile** result) override;
    virtual leveldb::Status NewRandomAccessFile(const std::string& fname, leveldb::RandomAccessFile** result) override;

    virtual bool FileExists(const std::string& fname) override {
        return memory_files_.find(fname) != memory_files_.end();
    }

    virtual leveldb::Status GetChildren(const std::string& dir, std::vector<std::string>* result) override {
        auto it = directories_.find(dir);
        if (it != directories_.end()) {
            *result = it->second;
            return leveldb::Status::OK();
        }
        return leveldb::Status::IOError("Directory not found");
    }

    virtual leveldb::Status DeleteFile(const std::string& fname) override {
        memory_files_.erase(fname);
        file_fd_map_.erase(fname);
        // Remove from parent directory listing
        size_t last_slash = fname.find_last_of('/');
        std::string dir = (last_slash == std::string::npos) ? "" : fname.substr(0, last_slash);
        std::string basename = (last_slash == std::string::npos) ? fname : fname.substr(last_slash + 1);
        
        auto dir_it = directories_.find(dir);
        if (dir_it != directories_.end()) {
            auto& files = dir_it->second;
            files.erase(std::remove(files.begin(), files.end(), basename), files.end());
        }
        return leveldb::Status::OK();
    }

    virtual leveldb::Status CreateDir(const std::string& dirname) override {
        directories_[dirname] = std::vector<std::string>();
        return leveldb::Status::OK();
    }

    virtual leveldb::Status DeleteDir(const std::string& dirname) override {
        auto it = directories_.find(dirname);
        if (it != directories_.end() && it->second.empty()) {
            directories_.erase(it);
            return leveldb::Status::OK();
        }
        return leveldb::Status::IOError("Directory not empty or not found");
    }

    virtual leveldb::Status GetFileSize(const std::string& fname, uint64_t* file_size) override {
        auto it = memory_files_.find(fname);
        if (it != memory_files_.end()) {
            *file_size = it->second->Size();
            return leveldb::Status::OK();
        }
        return leveldb::Status::IOError("File not found");
    }

    virtual leveldb::Status RenameFile(const std::string& src, const std::string& target) override {
        auto it = memory_files_.find(src);
        if (it != memory_files_.end()) {
            memory_files_[target] = it->second;
            memory_files_.erase(it);
            
            // Update file_fd_map
            auto fd_it = file_fd_map_.find(src);
            if (fd_it != file_fd_map_.end()) {
                uint32_t fd = fd_it->second;
                file_fd_map_.erase(fd_it);
                file_fd_map_[target] = fd;
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
    virtual void Schedule(void (*function)(void* arg), void* arg) override {
        function(arg); // Execute immediately
    }

    virtual void StartThread(void (*function)(void* arg), void* arg) override {
        // Single-threaded: just execute the function
        function(arg);
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
        auto it = file_fd_map_.find(fname);
        if (it != file_fd_map_.end()) {
            return it->second;
        }
        uint32_t vfd = next_virtual_fd_.fetch_add(1, std::memory_order_relaxed);
        file_fd_map_[fname] = vfd;
        
        // Add to directory listing
        size_t last_slash = fname.find_last_of('/');
        std::string dir = (last_slash == std::string::npos) ? "" : fname.substr(0, last_slash);
        std::string basename = (last_slash == std::string::npos) ? fname : fname.substr(last_slash + 1);
        directories_[dir].push_back(basename);
        
        return vfd;
    }

    std::shared_ptr<MemoryFile> GetOrCreateMemoryFile(const std::string& fname) {
        auto it = memory_files_.find(fname);
        if (it != memory_files_.end()) {
            return it->second;
        }
        auto file = std::make_shared<MemoryFile>();
        memory_files_[fname] = file;
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
        // Store in memory for later reads
        mem_file_->Append(data.data(), data.size());
        
        // Send to Normal World via ring buffer with retry logic
        SecureRingBufferProducer* producer = env_->GetProducer();
        std::string_view data_view(data.data(), data.size());
        
        // Retry up to 10 times with small delays if buffer is full
        for (int retry = 0; retry < 10; retry++) {
            if (producer->Push(virtual_fd_, data_view)) {
                return leveldb::Status::OK();
            }
            
            // Brief wait to let consumer process data
            // Note: In real TEE, use TEE_Wait() or similar
            for (volatile int i = 0; i < 10000; i++) { /* busy wait */ }
        }
        
        return leveldb::Status::IOError("Ring buffer full after retries");
    }

    virtual leveldb::Status Close() override {
        return Flush();
    }

    virtual leveldb::Status Flush() override {
        SecureRingBufferProducer* producer = env_->GetProducer();
        if (producer->Flush()) {
            return leveldb::Status::OK();
        }
        return leveldb::Status::IOError("Flush timeout");
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
    auto it = memory_files_.find(fname);
    if (it != memory_files_.end()) {
        *result = new SimpleSequentialFile(it->second);
        return leveldb::Status::OK();
    }
    return leveldb::Status::IOError("File not found");
}

inline leveldb::Status RingBufferEnv::NewRandomAccessFile(const std::string& fname, leveldb::RandomAccessFile** result) {
    auto it = memory_files_.find(fname);
    if (it != memory_files_.end()) {
        *result = new SimpleRandomAccessFile(it->second);
        return leveldb::Status::OK();
    }
    return leveldb::Status::IOError("File not found");
}
