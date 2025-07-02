#ifndef TYPES_H
#define TYPES_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

// 基础类型定义
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using sector_t = u64;      // 扇区号类型
using block_t = u64;       // 块号类型
using inode_t = u32;       // inode号类型
using offset_t = u64;      // 偏移量类型
using fs_size_t = u64;     // 文件系统大小类型

// 常量定义
static constexpr u32 SECTOR_SIZE = 512;        // 扇区大小(字节)
static constexpr u32 PAGE_SIZE = 4096;         // 页大小(字节)
static constexpr u32 BLOCK_SIZE = 4096;        // 块大小(字节)
static constexpr u32 MAX_FILENAME_LEN = 255;   // 最大文件名长度
static constexpr u32 MAX_PATH_LEN = 4096;      // 最大路径长度

// 文件类型枚举
enum class FileType : u8 {
    REGULAR = 1,    // 普通文件
    DIRECTORY = 2,  // 目录
    SYMLINK = 3,    // 符号链接
    BLOCK = 4,      // 块设备
    CHAR = 5,       // 字符设备
    FIFO = 6,       // 命名管道
    SOCKET = 7      // 套接字
};

// 文件权限
struct FileMode {
    u16 mode;
    
    FileMode(u16 m = 0) : mode(m) {}
    
    bool is_readable() const { return mode & 0400; }
    bool is_writable() const { return mode & 0200; }
    bool is_executable() const { return mode & 0100; }
    FileType type() const { return static_cast<FileType>((mode >> 12) & 0xF); }
};

// 错误码定义
enum class ErrorCode : s32 {
    SUCCESS = 0,
    FS_ENOENT = -2,      // 文件或目录不存在
    FS_EIO = -5,         // I/O错误
    FS_ENOMEM = -12,     // 内存不足
    FS_EACCES = -13,     // 权限拒绝
    FS_EEXIST = -17,     // 文件已存在
    FS_ENOTDIR = -20,    // 不是目录
    FS_EISDIR = -21,     // 是目录
    FS_EINVAL = -22,     // 无效参数
    FS_ENOSPC = -28,     // 设备空间不足
    FS_EROFS = -30,      // 只读文件系统
};

// 结果类型模板
template<typename T>
class Result {
private:
    union {
        T value_;
        ErrorCode error_;
    };
    bool is_ok_;

public:
    Result(const T& value) : value_(value), is_ok_(true) {}
    Result(T&& value) : value_(std::move(value)), is_ok_(true) {}
    Result(ErrorCode error) : error_(error), is_ok_(false) {}
    
    // Copy constructor
    Result(const Result& other) : is_ok_(other.is_ok_) {
        if (is_ok_) {
            new (&value_) T(other.value_);
        } else {
            error_ = other.error_;
        }
    }
    
    // Move constructor
    Result(Result&& other) noexcept : is_ok_(other.is_ok_) {
        if (is_ok_) {
            new (&value_) T(std::move(other.value_));
        } else {
            error_ = other.error_;
        }
    }
    
    // Copy assignment
    Result& operator=(const Result& other) {
        if (this != &other) {
            if (is_ok_) {
                value_.~T();
            }
            is_ok_ = other.is_ok_;
            if (is_ok_) {
                new (&value_) T(other.value_);
            } else {
                error_ = other.error_;
            }
        }
        return *this;
    }
    
    // Move assignment
    Result& operator=(Result&& other) noexcept {
        if (this != &other) {
            if (is_ok_) {
                value_.~T();
            }
            is_ok_ = other.is_ok_;
            if (is_ok_) {
                new (&value_) T(std::move(other.value_));
            } else {
                error_ = other.error_;
            }
        }
        return *this;
    }
    
    ~Result() {
        if (is_ok_) {
            value_.~T();
        }
    }
    
    bool is_ok() const { return is_ok_; }
    bool is_err() const { return !is_ok_; }
    
    const T& unwrap() const {
        if (!is_ok_) throw std::runtime_error("unwrap failed");
        return value_;
    }
    
    T& unwrap() {
        if (!is_ok_) throw std::runtime_error("unwrap failed");
        return value_;
    }
    
    ErrorCode error() const {
        if (is_ok_) throw std::runtime_error("no error");
        return error_;
    }
};

// Result<void> 特化
template<>
class Result<void> {
private:
    ErrorCode error_;
    bool is_ok_;

public:
    Result() : is_ok_(true) {}
    Result(ErrorCode error) : error_(error), is_ok_(false) {}
    
    // Copy constructor
    Result(const Result& other) : error_(other.error_), is_ok_(other.is_ok_) {}
    
    // Move constructor
    Result(Result&& other) noexcept : error_(other.error_), is_ok_(other.is_ok_) {}
    
    // Copy assignment
    Result& operator=(const Result& other) {
        if (this != &other) {
            error_ = other.error_;
            is_ok_ = other.is_ok_;
        }
        return *this;
    }
    
    // Move assignment
    Result& operator=(Result&& other) noexcept {
        if (this != &other) {
            error_ = other.error_;
            is_ok_ = other.is_ok_;
        }
        return *this;
    }
    
    bool is_ok() const { return is_ok_; }
    bool is_err() const { return !is_ok_; }
    
    void unwrap() const {
        if (!is_ok_) throw std::runtime_error("unwrap failed");
    }
    
    ErrorCode error() const {
        if (is_ok_) throw std::runtime_error("no error");
        return error_;
    }
};

// 智能指针别名
template<typename T>
using UniquePtr = std::unique_ptr<T>;

template<typename T>
using SharedPtr = std::shared_ptr<T>;

template<typename T>
using WeakPtr = std::weak_ptr<T>;

#endif // TYPES_H