// SPDX-License-Identifier: BSD-2-Clause
/*
 * Custom LevelDB Environment for OP-TEE
 * - All data stored in RAM only (no disk)
 * - All operations logged via ocall_logger
 */

#ifndef ENV_TEE_RAM_H_
#define ENV_TEE_RAM_H_

#include <leveldb/env.h>
#include <leveldb/slice.h>
#include <leveldb/status.h>
#include <map>
#include <string>
#include <vector>
#include <memory>

extern "C" {
#include <tee_internal_api.h>
}

// Include ocall_logger for logging
extern "C" {
    void ocall_log_print(const char* fmt, ...);
}

// Macro for logging
#define ocall_log ocall_log_print

namespace leveldb {

// Simple in-memory file implementation
class RamFile {
public:
    std::vector<char> data;
    size_t size() const { return data.size(); }
    
    void append(const char* buf, size_t n) {
        data.insert(data.end(), buf, buf + n);
    }
    
    void clear() {
        data.clear();
    }
};

// WritableFile that writes to RAM
class RamWritableFile : public WritableFile {
public:
    explicit RamWritableFile(const std::string& fname, std::shared_ptr<RamFile> file)
        : fname_(fname), file_(file) {
        ocall_log("RamWritableFile created: %s", fname.c_str());
    }
    
    ~RamWritableFile() override {
        ocall_log("RamWritableFile closed: %s (size=%zu)", fname_.c_str(), file_->size());
    }
    
    Status Append(const Slice& data) override {
        ocall_log("RamWritableFile::Append: %s, size=%zu", fname_.c_str(), data.size());
        file_->append(data.data(), data.size());
        return Status::OK();
    }
    
    Status Close() override {
        ocall_log("RamWritableFile::Close: %s", fname_.c_str());
        return Status::OK();
    }
    
    Status Flush() override {
        ocall_log("RamWritableFile::Flush: %s", fname_.c_str());
        return Status::OK();
    }
    
    Status Sync() override {
        ocall_log("RamWritableFile::Sync: %s", fname_.c_str());
        return Status::OK();
    }
    
private:
    std::string fname_;
    std::shared_ptr<RamFile> file_;
};

// SequentialFile that reads from RAM
class RamSequentialFile : public SequentialFile {
public:
    explicit RamSequentialFile(const std::string& fname, std::shared_ptr<RamFile> file)
        : fname_(fname), file_(file), pos_(0) {
        ocall_log("RamSequentialFile created: %s (size=%zu)", fname.c_str(), file->size());
    }
    
    ~RamSequentialFile() override {
        ocall_log("RamSequentialFile closed: %s", fname_.c_str());
    }
    
    Status Read(size_t n, Slice* result, char* scratch) override {
        size_t available = file_->size() - pos_;
        size_t to_read = (n < available) ? n : available;
        
        if (to_read > 0) {
            memcpy(scratch, &file_->data[pos_], to_read);
            pos_ += to_read;
        }
        
        *result = Slice(scratch, to_read);
        ocall_log("RamSequentialFile::Read: %s, requested=%zu, read=%zu", 
                  fname_.c_str(), n, to_read);
        return Status::OK();
    }
    
    Status Skip(uint64_t n) override {
        if (pos_ + n > file_->size()) {
            pos_ = file_->size();
        } else {
            pos_ += n;
        }
        ocall_log("RamSequentialFile::Skip: %s, bytes=%llu", fname_.c_str(), n);
        return Status::OK();
    }
    
private:
    std::string fname_;
    std::shared_ptr<RamFile> file_;
    size_t pos_;
};

// RandomAccessFile that reads from RAM
class RamRandomAccessFile : public RandomAccessFile {
public:
    explicit RamRandomAccessFile(const std::string& fname, std::shared_ptr<RamFile> file)
        : fname_(fname), file_(file) {
        ocall_log("RamRandomAccessFile created: %s (size=%zu)", fname.c_str(), file->size());
    }
    
    ~RamRandomAccessFile() override {
        ocall_log("RamRandomAccessFile closed: %s", fname_.c_str());
    }
    
    Status Read(uint64_t offset, size_t n, Slice* result, char* scratch) const override {
        if (offset >= file_->size()) {
            *result = Slice();
            return Status::OK();
        }
        
        size_t available = file_->size() - offset;
        size_t to_read = (n < available) ? n : available;
        
        if (to_read > 0) {
            memcpy(scratch, &file_->data[offset], to_read);
        }
        
        *result = Slice(scratch, to_read);
        ocall_log("RamRandomAccessFile::Read: %s, offset=%llu, requested=%zu, read=%zu",
                  fname_.c_str(), offset, n, to_read);
        return Status::OK();
    }
    
private:
    std::string fname_;
    std::shared_ptr<RamFile> file_;
};

// Custom Environment that uses RAM storage
class EnvTeeRam : public Env {
public:
    EnvTeeRam() {
        ocall_log("=== EnvTeeRam created ===");
    }
    
    ~EnvTeeRam() override {
        ocall_log("=== EnvTeeRam destroyed (total files: %zu) ===", files_.size());
    }
    
    Status NewSequentialFile(const std::string& fname, SequentialFile** result) override {
        ocall_log("EnvTeeRam::NewSequentialFile: %s", fname.c_str());
        
        auto it = files_.find(fname);
        if (it == files_.end()) {
            *result = nullptr;
            return Status::NotFound(fname);
        }
        
        *result = new RamSequentialFile(fname, it->second);
        return Status::OK();
    }
    
    Status NewRandomAccessFile(const std::string& fname, RandomAccessFile** result) override {
        ocall_log("EnvTeeRam::NewRandomAccessFile: %s", fname.c_str());
        
        auto it = files_.find(fname);
        if (it == files_.end()) {
            *result = nullptr;
            return Status::NotFound(fname);
        }
        
        *result = new RamRandomAccessFile(fname, it->second);
        return Status::OK();
    }
    
    Status NewWritableFile(const std::string& fname, WritableFile** result) override {
        ocall_log("EnvTeeRam::NewWritableFile: %s", fname.c_str());
        
        auto file = std::make_shared<RamFile>();
        files_[fname] = file;
        *result = new RamWritableFile(fname, file);
        return Status::OK();
    }
    
    Status NewAppendableFile(const std::string& fname, WritableFile** result) override {
        ocall_log("EnvTeeRam::NewAppendableFile: %s", fname.c_str());
        
        auto it = files_.find(fname);
        std::shared_ptr<RamFile> file;
        
        if (it == files_.end()) {
            file = std::make_shared<RamFile>();
            files_[fname] = file;
        } else {
            file = it->second;
        }
        
        *result = new RamWritableFile(fname, file);
        return Status::OK();
    }
    
    bool FileExists(const std::string& fname) override {
        bool exists = files_.find(fname) != files_.end();
        ocall_log("EnvTeeRam::FileExists: %s = %d", fname.c_str(), exists);
        return exists;
    }
    
    Status GetChildren(const std::string& dir, std::vector<std::string>* result) override {
        ocall_log("EnvTeeRam::GetChildren: %s", dir.c_str());
        result->clear();
        
        // Simple implementation: return all files that start with dir/
        std::string prefix = dir;
        if (!prefix.empty() && prefix.back() != '/') {
            prefix += '/';
        }
        
        for (const auto& kv : files_) {
            if (kv.first.find(prefix) == 0) {
                std::string child = kv.first.substr(prefix.size());
                // Only return direct children (no subdirectories)
                if (child.find('/') == std::string::npos) {
                    result->push_back(child);
                }
            }
        }
        
        ocall_log("EnvTeeRam::GetChildren: %s, found %zu files", dir.c_str(), result->size());
        return Status::OK();
    }
    
    Status RemoveFile(const std::string& fname) override {
        ocall_log("EnvTeeRam::RemoveFile: %s", fname.c_str());
        
        auto it = files_.find(fname);
        if (it == files_.end()) {
            return Status::NotFound(fname);
        }
        
        files_.erase(it);
        return Status::OK();
    }
    
    Status CreateDir(const std::string& dirname) override {
        ocall_log("EnvTeeRam::CreateDir: %s (no-op for RAM)", dirname.c_str());
        return Status::OK();
    }
    
    Status RemoveDir(const std::string& dirname) override {
        ocall_log("EnvTeeRam::RemoveDir: %s (no-op for RAM)", dirname.c_str());
        return Status::OK();
    }
    
    Status GetFileSize(const std::string& fname, uint64_t* file_size) override {
        auto it = files_.find(fname);
        if (it == files_.end()) {
            return Status::NotFound(fname);
        }
        
        *file_size = it->second->size();
        ocall_log("EnvTeeRam::GetFileSize: %s = %llu", fname.c_str(), *file_size);
        return Status::OK();
    }
    
    Status RenameFile(const std::string& src, const std::string& target) override {
        ocall_log("EnvTeeRam::RenameFile: %s -> %s", src.c_str(), target.c_str());
        
        auto it = files_.find(src);
        if (it == files_.end()) {
            return Status::NotFound(src);
        }
        
        files_[target] = it->second;
        files_.erase(it);
        return Status::OK();
    }
    
    Status LockFile(const std::string& fname, FileLock** lock) override {
        ocall_log("EnvTeeRam::LockFile: %s (no-op)", fname.c_str());
        *lock = nullptr;
        return Status::OK();
    }
    
    Status UnlockFile(FileLock* lock) override {
        ocall_log("EnvTeeRam::UnlockFile (no-op)");
        return Status::OK();
    }
    
    void Schedule(void (*function)(void* arg), void* arg) override {
        ocall_log("EnvTeeRam::Schedule (executing immediately)");
        (*function)(arg);
    }
    
    void StartThread(void (*function)(void* arg), void* arg) override {
        ocall_log("EnvTeeRam::StartThread (not supported, executing immediately)");
        (*function)(arg);
    }
    
    Status GetTestDirectory(std::string* path) override {
        *path = "/tmp/leveldb_test";
        ocall_log("EnvTeeRam::GetTestDirectory: %s", path->c_str());
        return Status::OK();
    }
    
    Status NewLogger(const std::string& fname, Logger** result) override {
        ocall_log("EnvTeeRam::NewLogger: %s (not implemented)", fname.c_str());
        *result = nullptr;
        return Status::NotSupported("Logger not implemented");
    }
    
    uint64_t NowMicros() override {
        // Simple implementation - return monotonic counter
        static uint64_t counter = 0;
        return ++counter;
    }
    
    void SleepForMicroseconds(int micros) override {
        ocall_log("EnvTeeRam::SleepForMicroseconds: %d (no-op)", micros);
    }
    
private:
    std::map<std::string, std::shared_ptr<RamFile>> files_;
};

} // namespace leveldb

#endif // ENV_TEE_RAM_H_

