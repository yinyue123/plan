#include "../../include/vfs.h"
#include "../../include/page_cache.h"
#include <cstring>

// FileAttribute构造函数
FileAttribute::FileAttribute() 
    : mode(0644), uid(0), gid(0), size(0), blocks(0), nlink(1), blksize(BLOCK_SIZE) {
    auto now = std::chrono::system_clock::now();
    atime = mtime = ctime = now;
}

// Inode实现
Inode::Inode(inode_t ino, SharedPtr<SuperBlock> sb, SharedPtr<InodeOperations> ops)
    : ino_(ino), sb_(sb), ops_(ops), ref_count_(1) {
}

Inode::~Inode() {
    // 同步所有脏页面
    sync();
}

SharedPtr<BlockDevice> Inode::get_block_device() const {
    return sb_->get_device();
}

Result<size_t> Inode::read(offset_t pos, void* buffer, size_t size) {
    if (!ops_) {
        return ErrorCode::FS_EIO;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查读取范围
    if (pos >= attr_.size) {
        return 0;  // EOF
    }
    
    // 调整读取大小
    size_t actual_size = std::min(size, static_cast<size_t>(attr_.size - pos));
    
    // 使用页面缓存进行读取
    size_t bytes_read = 0;
    u8* buf = static_cast<u8*>(buffer);
    
    while (bytes_read < actual_size) {
        // 计算页面偏移量
        offset_t page_offset = (pos + bytes_read) & ~(PAGE_SIZE - 1);
        size_t page_pos = (pos + bytes_read) & (PAGE_SIZE - 1);
        size_t page_bytes = std::min(actual_size - bytes_read, PAGE_SIZE - page_pos);
        
        // 获取页面
        auto page_result = g_page_cache.read_page(shared_from_this(), page_offset);
        if (page_result.is_err()) {
            return page_result.error();
        }
        
        SharedPtr<Page> page = page_result.unwrap();
        
        // 复制数据
        std::memcpy(buf + bytes_read, page->get_data() + page_pos, page_bytes);
        bytes_read += page_bytes;
        
        // 释放页面引用
        page->put();
    }
    
    // 更新访问时间
    attr_.atime = std::chrono::system_clock::now();
    
    return bytes_read;
}

Result<size_t> Inode::write(offset_t pos, const void* buffer, size_t size) {
    if (!ops_) {
        return ErrorCode::FS_EIO;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查写权限
    if (!attr_.mode.is_writable()) {
        return ErrorCode::FS_EACCES;
    }
    
    const u8* buf = static_cast<const u8*>(buffer);
    size_t bytes_written = 0;
    
    while (bytes_written < size) {
        // 计算页面偏移量
        offset_t page_offset = (pos + bytes_written) & ~(PAGE_SIZE - 1);
        size_t page_pos = (pos + bytes_written) & (PAGE_SIZE - 1);
        size_t page_bytes = std::min(size - bytes_written, PAGE_SIZE - page_pos);
        
        // 获取或创建页面
        SharedPtr<Page> page = g_page_cache.find_or_create_page(shared_from_this(), page_offset);
        if (!page) {
            return ErrorCode::FS_ENOMEM;
        }
        
        // 如果是部分页面写入，需要先读取原数据
        if (page_pos != 0 || page_bytes != PAGE_SIZE) {
            if (!page->is_uptodate()) {
                auto read_result = g_page_cache.read_page(shared_from_this(), page_offset);
                if (read_result.is_err()) {
                    page->put();
                    return read_result.error();
                }
            }
        }
        
        // 写入数据
        std::memcpy(page->get_data() + page_pos, buf + bytes_written, page_bytes);
        bytes_written += page_bytes;
        
        // 标记页面为脏
        page->mark_dirty();
        
        // 释放页面引用
        page->put();
    }
    
    // 更新文件大小和修改时间
    if (pos + bytes_written > attr_.size) {
        attr_.size = pos + bytes_written;
    }
    attr_.mtime = attr_.ctime = std::chrono::system_clock::now();
    
    return bytes_written;
}

Result<std::vector<DirentEntry>> Inode::readdir() {
    if (!is_dir()) {
        return ErrorCode::FS_ENOTDIR;
    }
    
    if (!ops_) {
        return ErrorCode::FS_EIO;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    return ops_->readdir(shared_from_this());
}

Result<SharedPtr<Inode>> Inode::lookup(const std::string& name) {
    if (!is_dir()) {
        return ErrorCode::FS_ENOTDIR;
    }
    
    if (!ops_) {
        return ErrorCode::FS_EIO;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    return ops_->lookup(shared_from_this(), name);
}

Result<SharedPtr<Inode>> Inode::create(const std::string& name, FileMode mode) {
    if (!is_dir()) {
        return ErrorCode::FS_ENOTDIR;
    }
    
    if (!ops_) {
        return ErrorCode::FS_EIO;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查写权限
    if (!attr_.mode.is_writable()) {
        return ErrorCode::FS_EACCES;
    }
    
    auto result = ops_->create(shared_from_this(), name, mode);
    if (result.is_ok()) {
        // 更新目录的修改时间
        attr_.mtime = attr_.ctime = std::chrono::system_clock::now();
    }
    
    return result;
}

Result<void> Inode::unlink(const std::string& name) {
    if (!is_dir()) {
        return ErrorCode::FS_ENOTDIR;
    }
    
    if (!ops_) {
        return ErrorCode::FS_EIO;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查写权限
    if (!attr_.mode.is_writable()) {
        return ErrorCode::FS_EACCES;
    }
    
    auto result = ops_->unlink(shared_from_this(), name);
    if (result.is_ok()) {
        // 更新目录的修改时间
        attr_.mtime = attr_.ctime = std::chrono::system_clock::now();
    }
    
    return result;
}

Result<void> Inode::mkdir(const std::string& name, FileMode mode) {
    if (!is_dir()) {
        return ErrorCode::FS_ENOTDIR;
    }
    
    if (!ops_) {
        return ErrorCode::FS_EIO;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查写权限
    if (!attr_.mode.is_writable()) {
        return ErrorCode::FS_EACCES;
    }
    
    auto result = ops_->mkdir(shared_from_this(), name, mode);
    if (result.is_ok()) {
        // 更新目录的修改时间
        attr_.mtime = attr_.ctime = std::chrono::system_clock::now();
    }
    
    return result;
}

Result<void> Inode::rmdir(const std::string& name) {
    if (!is_dir()) {
        return ErrorCode::FS_ENOTDIR;
    }
    
    if (!ops_) {
        return ErrorCode::FS_EIO;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查写权限
    if (!attr_.mode.is_writable()) {
        return ErrorCode::FS_EACCES;
    }
    
    auto result = ops_->rmdir(shared_from_this(), name);
    if (result.is_ok()) {
        // 更新目录的修改时间
        attr_.mtime = attr_.ctime = std::chrono::system_clock::now();
    }
    
    return result;
}

Result<void> Inode::rename(const std::string& old_name, SharedPtr<Inode> new_dir, 
                          const std::string& new_name) {
    if (!is_dir() || !new_dir->is_dir()) {
        return ErrorCode::FS_ENOTDIR;
    }
    
    if (!ops_ || !new_dir->ops_) {
        return ErrorCode::FS_EIO;
    }
    
    // 锁定顺序：按inode号排序以避免死锁
    std::unique_lock<std::mutex> lock1, lock2;
    if (ino_ < new_dir->ino_) {
        lock1 = std::unique_lock<std::mutex>(mutex_);
        lock2 = std::unique_lock<std::mutex>(new_dir->mutex_);
    } else if (ino_ > new_dir->ino_) {
        lock2 = std::unique_lock<std::mutex>(new_dir->mutex_);
        lock1 = std::unique_lock<std::mutex>(mutex_);
    } else {
        // 同一目录
        lock1 = std::unique_lock<std::mutex>(mutex_);
    }
    
    // 检查权限
    if (!attr_.mode.is_writable() || !new_dir->attr_.mode.is_writable()) {
        return ErrorCode::FS_EACCES;
    }
    
    auto result = ops_->rename(shared_from_this(), old_name, new_dir, new_name);
    if (result.is_ok()) {
        // 更新两个目录的修改时间
        auto now = std::chrono::system_clock::now();
        attr_.mtime = attr_.ctime = now;
        new_dir->attr_.mtime = new_dir->attr_.ctime = now;
    }
    
    return result;
}

Result<FileAttribute> Inode::getattr() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return attr_;
}

Result<void> Inode::setattr(const FileAttribute& attr) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!ops_) {
        return ErrorCode::FS_EIO;
    }
    
    // 更新属性
    attr_ = attr;
    attr_.ctime = std::chrono::system_clock::now();
    
    // 通知文件系统
    return ops_->setattr(shared_from_this(), attr_);
}

Result<std::string> Inode::getxattr(const std::string& name) {
    if (!ops_) {
        return ErrorCode::FS_EIO;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    return ops_->getxattr(shared_from_this(), name);
}

Result<void> Inode::setxattr(const std::string& name, const std::string& value) {
    if (!ops_) {
        return ErrorCode::FS_EIO;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto result = ops_->setxattr(shared_from_this(), name, value);
    if (result.is_ok()) {
        attr_.ctime = std::chrono::system_clock::now();
    }
    
    return result;
}

Result<std::vector<std::string>> Inode::listxattr() {
    if (!ops_) {
        return ErrorCode::FS_EIO;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    return ops_->listxattr(shared_from_this());
}

Result<void> Inode::removexattr(const std::string& name) {
    if (!ops_) {
        return ErrorCode::FS_EIO;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto result = ops_->removexattr(shared_from_this(), name);
    if (result.is_ok()) {
        attr_.ctime = std::chrono::system_clock::now();
    }
    
    return result;
}

Result<void> Inode::sync() {
    // 同步页面缓存
    auto result = g_page_cache.sync_pages(shared_from_this());
    if (result.is_err()) {
        return result;
    }
    
    // 同步inode元数据
    if (sb_ && sb_->get_ops()) {
        return sb_->get_ops()->write_inode(shared_from_this());
    }
    
    return Result<void>();
}

Result<void> Inode::truncate(u64 size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (size == attr_.size) {
        return Result<void>();
    }
    
    if (size < attr_.size) {
        // 截断文件，需要释放超出部分的页面
        // offset_t start_page = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        
        // 使对应页面失效
        g_page_cache.invalidate_pages(shared_from_this());
    }
    
    // 更新文件大小
    attr_.size = size;
    attr_.mtime = attr_.ctime = std::chrono::system_clock::now();
    
    return Result<void>();
}