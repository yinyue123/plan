#include "../../include/syscall.h"

// FileDescriptorTable实现
FileDescriptorTable::FileDescriptorTable(size_t size) : files_(size) {
}

FileDescriptorTable::~FileDescriptorTable() {
}

Result<int> FileDescriptorTable::alloc_fd(SharedPtr<File> file) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (size_t i = 0; i < files_.size(); ++i) {
        if (!files_[i]) {
            files_[i] = file;
            return static_cast<int>(i);
        }
    }
    
    return ErrorCode::FS_ENOMEM; // 没有可用的文件描述符
}

Result<void> FileDescriptorTable::free_fd(int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_valid_fd(fd)) {
        return ErrorCode::FS_EINVAL;
    }
    
    files_[fd].reset();
    return Result<void>();
}

SharedPtr<File> FileDescriptorTable::get_file(int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_valid_fd(fd)) {
        return nullptr;
    }
    
    return files_[fd];
}

Result<int> FileDescriptorTable::dup_fd(int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_valid_fd(fd)) {
        return ErrorCode::FS_EINVAL;
    }
    
    auto file = files_[fd];
    if (!file) {
        return ErrorCode::FS_EINVAL;
    }
    
    // 查找下一个可用的文件描述符
    for (size_t i = 0; i < files_.size(); ++i) {
        if (!files_[i]) {
            files_[i] = file;
            return static_cast<int>(i);
        }
    }
    
    return ErrorCode::FS_ENOMEM;
}

Result<int> FileDescriptorTable::dup2_fd(int oldfd, int newfd) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_valid_fd(oldfd) || newfd < 0 || newfd >= static_cast<int>(files_.size())) {
        return ErrorCode::FS_EINVAL;
    }
    
    auto file = files_[oldfd];
    if (!file) {
        return ErrorCode::FS_EINVAL;
    }
    
    // 如果newfd已经打开，先关闭它
    if (files_[newfd]) {
        files_[newfd].reset();
    }
    
    files_[newfd] = file;
    return newfd;
}

int FileDescriptorTable::get_next_fd() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (size_t i = 0; i < files_.size(); ++i) {
        if (!files_[i]) {
            return static_cast<int>(i);
        }
    }
    
    return -1;
}

bool FileDescriptorTable::is_valid_fd(int fd) const {
    return fd >= 0 && fd < static_cast<int>(files_.size());
}

// ProcessFsContext实现
ProcessFsContext::ProcessFsContext() : umask(0022) {
    // 创建默认的根目录和当前目录
    // 在实际实现中，这些应该从VFS获取
    cwd = nullptr; // 将在VFS初始化后设置
    root = nullptr; // 将在VFS初始化后设置
}