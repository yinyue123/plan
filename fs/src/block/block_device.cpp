#include "../../include/block_device.h"
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

// MemoryBlockDevice实现

MemoryBlockDevice::MemoryBlockDevice(size_t size, u32 sector_size, 
                                     u32 block_size, bool readonly,
                                     const std::string& name, 
                                     u32 major, u32 minor)
    : data_(size, 0), sector_size_(sector_size), block_size_(block_size),
      readonly_(readonly), name_(name), major_(major), minor_(minor),
      stop_io_thread_(false) {
    
    // 启动I/O处理线程
    io_thread_ = std::thread(&MemoryBlockDevice::io_thread_func, this);
}

MemoryBlockDevice::~MemoryBlockDevice() {
    // 停止I/O线程
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_io_thread_ = true;
    }
    bio_cv_.notify_all();
    
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
}

size_t MemoryBlockDevice::get_size() const {
    return data_.size();
}

u32 MemoryBlockDevice::get_sector_size() const {
    return sector_size_;
}

u32 MemoryBlockDevice::get_block_size() const {
    return block_size_;
}

bool MemoryBlockDevice::is_readonly() const {
    return readonly_;
}

Result<size_t> MemoryBlockDevice::read(sector_t sector, void* buffer, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 计算字节偏移量
    offset_t offset = sector * sector_size_;
    
    // 检查边界
    if (offset >= data_.size()) {
        return ErrorCode::FS_EINVAL;
    }
    
    // 调整读取大小
    size_t actual_size = std::min(size, static_cast<size_t>(data_.size() - offset));
    
    // 复制数据
    std::memcpy(buffer, data_.data() + offset, actual_size);
    
    return actual_size;
}

Result<size_t> MemoryBlockDevice::write(sector_t sector, const void* buffer, size_t size) {
    if (readonly_) {
        return ErrorCode::FS_EROFS;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 计算字节偏移量
    offset_t offset = sector * sector_size_;
    
    // 检查边界
    if (offset >= data_.size()) {
        return ErrorCode::FS_EINVAL;
    }
    
    // 调整写入大小
    size_t actual_size = std::min(size, static_cast<size_t>(data_.size() - offset));
    
    // 复制数据
    std::memcpy(data_.data() + offset, buffer, actual_size);
    
    return actual_size;
}

Result<void> MemoryBlockDevice::flush() {
    // 内存设备无需刷新
    return Result<void>();
}

Result<void> MemoryBlockDevice::trim(sector_t sector, size_t size) {
    if (readonly_) {
        return ErrorCode::FS_EROFS;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 计算字节偏移量
    offset_t offset = sector * sector_size_;
    
    // 检查边界
    if (offset >= data_.size()) {
        return ErrorCode::FS_EINVAL;
    }
    
    // 调整大小并清零
    size_t actual_size = std::min(size, static_cast<size_t>(data_.size() - offset));
    std::memset(data_.data() + offset, 0, actual_size);
    
    return Result<void>();
}

void MemoryBlockDevice::submit_bio(std::unique_ptr<Bio> bio) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        bio_queue_.push(std::move(bio));
    }
    bio_cv_.notify_one();
}

std::string MemoryBlockDevice::get_name() const {
    return name_;
}

u32 MemoryBlockDevice::get_major() const {
    return major_;
}

u32 MemoryBlockDevice::get_minor() const {
    return minor_;
}

void MemoryBlockDevice::load_from_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开文件: " + filename);
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 读取文件内容到设备
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    size_t read_size = std::min(file_size, data_.size());
    file.read(reinterpret_cast<char*>(data_.data()), read_size);
}

void MemoryBlockDevice::save_to_file(const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("无法创建文件: " + filename);
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 写入设备内容到文件
    file.write(reinterpret_cast<const char*>(data_.data()), data_.size());
}

void MemoryBlockDevice::io_thread_func() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 等待Bio请求或停止信号
        bio_cv_.wait(lock, [this] { 
            return !bio_queue_.empty() || stop_io_thread_; 
        });
        
        if (stop_io_thread_) {
            break;
        }
        
        // 处理所有排队的Bio请求
        while (!bio_queue_.empty()) {
            auto bio = std::move(bio_queue_.front());
            bio_queue_.pop();
            lock.unlock();
            
            process_bio(std::move(bio));
            
            lock.lock();
        }
    }
}

void MemoryBlockDevice::process_bio(std::unique_ptr<Bio> bio) {
    ErrorCode result = ErrorCode::SUCCESS;
    
    try {
        switch (bio->type) {
            case BioType::READ: {
                auto res = read(bio->sector, bio->data, bio->size);
                if (res.is_err()) {
                    result = res.error();
                }
                break;
            }
            
            case BioType::WRITE: {
                auto res = write(bio->sector, bio->data, bio->size);
                if (res.is_err()) {
                    result = res.error();
                }
                break;
            }
            
            case BioType::FLUSH: {
                auto res = flush();
                if (res.is_err()) {
                    result = res.error();
                }
                break;
            }
            
            case BioType::DISCARD: {
                auto res = trim(bio->sector, bio->size);
                if (res.is_err()) {
                    result = res.error();
                }
                break;
            }
        }
    } catch (...) {
        result = ErrorCode::FS_EIO;
    }
    
    // 调用完成回调
    if (bio->callback) {
        bio->callback(result);
    }
}

// FileBlockDevice实现

FileBlockDevice::FileBlockDevice(const std::string& filename, bool readonly,
                                 u32 sector_size, u32 block_size,
                                 u32 major, u32 minor)
    : filename_(filename), fd_(-1), size_(0), 
      sector_size_(sector_size), block_size_(block_size),
      readonly_(readonly), major_(major), minor_(minor) {
    
    // 打开文件
    int flags = readonly ? O_RDONLY : O_RDWR;
    fd_ = open(filename.c_str(), flags);
    
    if (fd_ < 0) {
        throw std::runtime_error("无法打开文件: " + filename);
    }
    
    // 获取文件大小
    struct stat st;
    if (fstat(fd_, &st) < 0) {
        close(fd_);
        throw std::runtime_error("无法获取文件信息: " + filename);
    }
    
    size_ = st.st_size;
}

FileBlockDevice::~FileBlockDevice() {
    if (fd_ >= 0) {
        close(fd_);
    }
}

size_t FileBlockDevice::get_size() const {
    return size_;
}

u32 FileBlockDevice::get_sector_size() const {
    return sector_size_;
}

u32 FileBlockDevice::get_block_size() const {
    return block_size_;
}

bool FileBlockDevice::is_readonly() const {
    return readonly_;
}

Result<size_t> FileBlockDevice::read(sector_t sector, void* buffer, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 计算字节偏移量
    offset_t offset = sector * sector_size_;
    
    // 检查边界
    if (offset >= size_) {
        return ErrorCode::FS_EINVAL;
    }
    
    // 调整读取大小
    size_t actual_size = std::min(size, static_cast<size_t>(size_ - offset));
    
    // 定位并读取
    if (lseek(fd_, static_cast<off_t>(offset), SEEK_SET) != static_cast<off_t>(offset)) {
        return ErrorCode::FS_EIO;
    }
    
    ssize_t bytes_read = ::read(fd_, buffer, actual_size);
    if (bytes_read < 0) {
        return ErrorCode::FS_EIO;
    }
    
    return static_cast<size_t>(bytes_read);
}

Result<size_t> FileBlockDevice::write(sector_t sector, const void* buffer, size_t size) {
    if (readonly_) {
        return ErrorCode::FS_EROFS;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 计算字节偏移量
    offset_t offset = sector * sector_size_;
    
    // 检查边界
    if (offset >= size_) {
        return ErrorCode::FS_EINVAL;
    }
    
    // 调整写入大小
    size_t actual_size = std::min(size, static_cast<size_t>(size_ - offset));
    
    // 定位并写入
    if (lseek(fd_, static_cast<off_t>(offset), SEEK_SET) != static_cast<off_t>(offset)) {
        return ErrorCode::FS_EIO;
    }
    
    ssize_t bytes_written = ::write(fd_, buffer, actual_size);
    if (bytes_written < 0) {
        return ErrorCode::FS_EIO;
    }
    
    return static_cast<size_t>(bytes_written);
}

Result<void> FileBlockDevice::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (fsync(fd_) < 0) {
        return ErrorCode::FS_EIO;
    }
    
    return Result<void>();
}

Result<void> FileBlockDevice::trim(sector_t /* sector */, size_t /* size */) {
    // 文件设备暂不支持TRIM
    return Result<void>();
}

void FileBlockDevice::submit_bio(std::unique_ptr<Bio> bio) {
    // 简单实现：直接在当前线程处理
    ErrorCode result = ErrorCode::SUCCESS;
    
    try {
        switch (bio->type) {
            case BioType::READ: {
                auto res = read(bio->sector, bio->data, bio->size);
                if (res.is_err()) {
                    result = res.error();
                }
                break;
            }
            
            case BioType::WRITE: {
                auto res = write(bio->sector, bio->data, bio->size);
                if (res.is_err()) {
                    result = res.error();
                }
                break;
            }
            
            case BioType::FLUSH: {
                auto res = flush();
                if (res.is_err()) {
                    result = res.error();
                }
                break;
            }
            
            case BioType::DISCARD: {
                auto res = trim(bio->sector, bio->size);
                if (res.is_err()) {
                    result = res.error();
                }
                break;
            }
        }
    } catch (...) {
        result = ErrorCode::FS_EIO;
    }
    
    // 调用完成回调
    if (bio->callback) {
        bio->callback(result);
    }
}

std::string FileBlockDevice::get_name() const {
    return filename_;
}

u32 FileBlockDevice::get_major() const {
    return major_;
}

u32 FileBlockDevice::get_minor() const {
    return minor_;
}