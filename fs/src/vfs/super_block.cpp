#include "../../include/vfs.h"

// SuperBlock实现
SuperBlock::SuperBlock(SharedPtr<BlockDevice> device, SharedPtr<FileSystem> fs_type, u32 flags)
    : device_(device), fs_type_(fs_type), flags_(flags) {
    device_name_ = device->get_name();
}

SuperBlock::~SuperBlock() {
}

Result<SharedPtr<Inode>> SuperBlock::get_inode(inode_t ino) {
    std::lock_guard<std::mutex> lock(inode_cache_mutex_);
    
    auto iter = inode_cache_.find(ino);
    if (iter != inode_cache_.end()) {
        if (auto inode = iter->second.lock()) {
            return inode;
        } else {
            inode_cache_.erase(iter);
        }
    }
    
    if (!ops_) {
        return ErrorCode::FS_EIO;
    }
    
    auto result = ops_->read_inode(ino);
    if (result.is_ok()) {
        cache_inode(result.unwrap());
    }
    
    return result;
}

void SuperBlock::cache_inode(SharedPtr<Inode> inode) {
    std::lock_guard<std::mutex> lock(inode_cache_mutex_);
    inode_cache_[inode->get_ino()] = inode;
}

void SuperBlock::evict_inode(inode_t ino) {
    std::lock_guard<std::mutex> lock(inode_cache_mutex_);
    inode_cache_.erase(ino);
}

Result<void> SuperBlock::sync() {
    if (!ops_) {
        return ErrorCode::FS_EIO;
    }
    return ops_->sync();
}