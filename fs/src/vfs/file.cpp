#include "../../include/vfs.h"

// File实现
File::File(SharedPtr<Dentry> dentry, u32 flags)
    : dentry_(dentry), flags_(flags), pos_(0), ref_count_(1) {
}

File::~File() {
}

Result<size_t> File::read(void* buffer, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto inode = dentry_->get_inode();
    if (!inode) {
        return ErrorCode::FS_EIO;
    }
    
    auto result = inode->read(pos_, buffer, size);
    if (result.is_ok()) {
        pos_ += result.unwrap();
    }
    
    return result;
}

Result<size_t> File::write(const void* buffer, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto inode = dentry_->get_inode();
    if (!inode) {
        return ErrorCode::FS_EIO;
    }
    
    auto result = inode->write(pos_, buffer, size);
    if (result.is_ok()) {
        pos_ += result.unwrap();
    }
    
    return result;
}

Result<offset_t> File::seek(offset_t offset, int whence) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    switch (whence) {
        case 0: // SEEK_SET
            pos_ = offset;
            break;
        case 1: // SEEK_CUR
            pos_ += offset;
            break;
        case 2: // SEEK_END
            pos_ = dentry_->get_inode()->get_size() + offset;
            break;
        default:
            return ErrorCode::FS_EINVAL;
    }
    
    return pos_;
}

Result<void> File::fsync() {
    auto inode = dentry_->get_inode();
    if (!inode) {
        return ErrorCode::FS_EIO;
    }
    
    return inode->sync();
}

Result<void> File::truncate(u64 size) {
    auto inode = dentry_->get_inode();
    if (!inode) {
        return ErrorCode::FS_EIO;
    }
    
    return inode->truncate(size);
}

Result<std::vector<DirentEntry>> File::readdir() {
    auto inode = dentry_->get_inode();
    if (!inode) {
        return ErrorCode::FS_EIO;
    }
    
    return inode->readdir();
}

Result<FileAttribute> File::fstat() {
    auto inode = dentry_->get_inode();
    if (!inode) {
        return ErrorCode::FS_EIO;
    }
    
    return inode->getattr();
}