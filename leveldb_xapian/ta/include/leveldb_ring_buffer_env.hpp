#pragma once
#include "secure_ring_buffer_producer.hpp"
#include <leveldb/env.h>
#include <leveldb/slice.h>
#include <leveldb/status.h>
#include <map>
#include <memory>
#include <string>
#include <atomic>

// Forward declaration
class RingBufferWritableFile;
class RingBufferSequentialFile;

class RingBufferEnv : public leveldb::Env {
private:
    leveldb::Env* base_env_;
    SecureRingBufferProducer* producer_;
    std::atomic<uint32_t> next_virtual_fd_{1000}; // Bắt đầu từ 1000
    std::map<std::string, uint32_t> file_fd_map_;

public:
    explicit RingBufferEnv(SecureRingBufferProducer* producer, leveldb::Env* base = leveldb::Env::Default())
        : base_env_(base), producer_(producer) {}

    virtual ~RingBufferEnv() {}

    // Các hàm quan trọng cho LevelDB I/O
    virtual leveldb::Status NewWritableFile(const std::string& fname, leveldb::WritableFile** result) override;
    virtual leveldb::Status NewAppendableFile(const std::string& fname, leveldb::WritableFile** result) override;
    virtual leveldb::Status NewSequentialFile(const std::string& fname, leveldb::SequentialFile** result) override;
    virtual leveldb::Status NewRandomAccessFile(const std::string& fname, leveldb::RandomAccessFile** result) override;

    // Delegate các hàm khác cho base_env
    virtual bool FileExists(const std::string& fname) override {
        return base_env_->FileExists(fname);
    }

    virtual leveldb::Status GetChildren(const std::string& dir, std::vector<std::string>* result) override {
        return base_env_->GetChildren(dir, result);
    }

    virtual leveldb::Status DeleteFile(const std::string& fname) override {
        file_fd_map_.erase(fname);
        return base_env_->DeleteFile(fname);
    }

    virtual leveldb::Status CreateDir(const std::string& dirname) override {
        return base_env_->CreateDir(dirname);
    }

    virtual leveldb::Status DeleteDir(const std::string& dirname) override {
        return base_env_->DeleteDir(dirname);
    }

    virtual leveldb::Status GetFileSize(const std::string& fname, uint64_t* file_size) override {
        return base_env_->GetFileSize(fname, file_size);
    }

    virtual leveldb::Status RenameFile(const std::string& src, const std::string& target) override {
        auto it = file_fd_map_.find(src);
        if (it != file_fd_map_.end()) {
            uint32_t fd = it->second;
            file_fd_map_.erase(it);
            file_fd_map_[target] = fd;
        }
        return base_env_->RenameFile(src, target);
    }

    virtual leveldb::Status LockFile(const std::string& fname, leveldb::FileLock** lock) override {
        return base_env_->LockFile(fname, lock);
    }

    virtual leveldb::Status UnlockFile(leveldb::FileLock* lock) override {
        return base_env_->UnlockFile(lock);
    }

    virtual void Schedule(void (*function)(void* arg), void* arg) override {
        base_env_->Schedule(function, arg);
    }

    virtual void StartThread(void (*function)(void* arg), void* arg) override {
        base_env_->StartThread(function, arg);
    }

    virtual leveldb::Status GetTestDirectory(std::string* path) override {
        return base_env_->GetTestDirectory(path);
    }

    virtual leveldb::Status NewLogger(const std::string& fname, leveldb::Logger** result) override {
        return base_env_->NewLogger(fname, result);
    }

    virtual uint64_t NowMicros() override {
        return base_env_->NowMicros();
    }

    virtual void SleepForMicroseconds(int micros) override {
        base_env_->SleepForMicroseconds(micros);
    }

    // Helper để lấy hoặc tạo virtual FD cho file
    uint32_t GetOrCreateVirtualFD(const std::string& fname) {
        auto it = file_fd_map_.find(fname);
        if (it != file_fd_map_.end()) {
            return it->second;
        }
        uint32_t vfd = next_virtual_fd_.fetch_add(1, std::memory_order_relaxed);
        file_fd_map_[fname] = vfd;
        return vfd;
    }

    SecureRingBufferProducer* GetProducer() { return producer_; }
};

// WritableFile implementation sử dụng Ring Buffer
class RingBufferWritableFile : public leveldb::WritableFile {
private:
    RingBufferEnv* env_;
    uint32_t virtual_fd_;
    std::string filename_;

public:
    RingBufferWritableFile(RingBufferEnv* env, uint32_t vfd, const std::string& fname)
        : env_(env), virtual_fd_(vfd), filename_(fname) {}

    virtual ~RingBufferWritableFile() {
        // Đảm bảo flush khi đóng file
        Close();
    }

    virtual leveldb::Status Append(const leveldb::Slice& data) override {
        SecureRingBufferProducer* producer = env_->GetProducer();
        std::string_view data_view(data.data(), data.size());
        
        if (producer->Push(virtual_fd_, data_view)) {
            return leveldb::Status::OK();
        }
        return leveldb::Status::IOError("Ring buffer full");
    }

    virtual leveldb::Status Close() override {
        return Flush();
    }

    virtual leveldb::Status Flush() override {
        SecureRingBufferProducer* producer = env_->GetProducer();
        if (producer->Flush()) {
            return leveldb::Status::OK();
        }
        return leveldb::Status::IOError("Flush timeout - Possible Disk Failure");
    }

    virtual leveldb::Status Sync() override {
        return Flush();
    }
};

// SequentialFile implementation - Đọc dữ liệu tuần tự
// Vì Ring Buffer chủ yếu tối ưu cho ghi, phần đọc sẽ sử dụng base_env
class RingBufferSequentialFile : public leveldb::SequentialFile {
private:
    leveldb::SequentialFile* base_file_;

public:
    explicit RingBufferSequentialFile(leveldb::SequentialFile* base) : base_file_(base) {}
    
    virtual ~RingBufferSequentialFile() {
        delete base_file_;
    }

    virtual leveldb::Status Read(size_t n, leveldb::Slice* result, char* scratch) override {
        return base_file_->Read(n, result, scratch);
    }

    virtual leveldb::Status Skip(uint64_t n) override {
        return base_file_->Skip(n);
    }
};

// Implementation của NewWritableFile
inline leveldb::Status RingBufferEnv::NewWritableFile(const std::string& fname, leveldb::WritableFile** result) {
    uint32_t vfd = GetOrCreateVirtualFD(fname);
    *result = new RingBufferWritableFile(this, vfd, fname);
    return leveldb::Status::OK();
}

inline leveldb::Status RingBufferEnv::NewAppendableFile(const std::string& fname, leveldb::WritableFile** result) {
    return NewWritableFile(fname, result);
}

inline leveldb::Status RingBufferEnv::NewSequentialFile(const std::string& fname, leveldb::SequentialFile** result) {
    leveldb::SequentialFile* base_file;
    leveldb::Status s = base_env_->NewSequentialFile(fname, &base_file);
    if (s.ok()) {
        *result = new RingBufferSequentialFile(base_file);
    }
    return s;
}

inline leveldb::Status RingBufferEnv::NewRandomAccessFile(const std::string& fname, leveldb::RandomAccessFile** result) {
    // Random access sử dụng base_env vì không cần tối ưu
    return base_env_->NewRandomAccessFile(fname, result);
}
