#ifndef BLOCK_DEVICE_H
#define BLOCK_DEVICE_H

#include "types.h"
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>

// 前向声明
class BlockDevice;
class Page;

// Bio请求类型
enum class BioType {
    READ,
    WRITE,
    FLUSH,
    DISCARD
};

// Bio请求结构
struct Bio {
    BioType type;              // 请求类型
    sector_t sector;           // 起始扇区
    size_t size;              // 请求大小(字节)
    void* data;               // 数据缓冲区
    std::function<void(ErrorCode)> callback;  // 完成回调
    
    Bio(BioType t, sector_t s, size_t sz, void* d, 
        std::function<void(ErrorCode)> cb = nullptr)
        : type(t), sector(s), size(sz), data(d), callback(std::move(cb)) {}
};

// 块设备接口
class BlockDevice {
public:
    virtual ~BlockDevice() = default;
    
    // 获取设备信息
    virtual size_t get_size() const = 0;          // 设备总大小(字节)
    virtual u32 get_sector_size() const = 0;      // 扇区大小
    virtual u32 get_block_size() const = 0;       // 块大小
    virtual bool is_readonly() const = 0;         // 是否只读
    
    // 同步I/O接口
    virtual Result<size_t> read(sector_t sector, void* buffer, size_t size) = 0;
    virtual Result<size_t> write(sector_t sector, const void* buffer, size_t size) = 0;
    virtual Result<void> flush() = 0;             // 刷新缓存
    virtual Result<void> trim(sector_t sector, size_t size) = 0;  // TRIM/丢弃
    
    // 异步I/O接口
    virtual void submit_bio(std::unique_ptr<Bio> bio) = 0;
    
    // 设备标识
    virtual std::string get_name() const = 0;
    virtual u32 get_major() const = 0;            // 主设备号
    virtual u32 get_minor() const = 0;            // 次设备号
};

// 内存块设备实现(用于测试)
class MemoryBlockDevice : public BlockDevice {
private:
    std::vector<u8> data_;                        // 设备数据
    u32 sector_size_;                             // 扇区大小
    u32 block_size_;                              // 块大小
    bool readonly_;                               // 是否只读
    std::string name_;                            // 设备名称
    u32 major_, minor_;                           // 设备号
    
    mutable std::mutex mutex_;                    // 保护并发访问
    std::thread io_thread_;                       // I/O处理线程
    std::queue<std::unique_ptr<Bio>> bio_queue_;  // Bio请求队列
    std::condition_variable bio_cv_;              // 条件变量
    bool stop_io_thread_;                         // 停止I/O线程标志
    
    void io_thread_func();                        // I/O线程函数
    void process_bio(std::unique_ptr<Bio> bio);   // 处理Bio请求

public:
    MemoryBlockDevice(size_t size, u32 sector_size = SECTOR_SIZE, 
                      u32 block_size = BLOCK_SIZE, bool readonly = false,
                      const std::string& name = "memblk", 
                      u32 major = 8, u32 minor = 0);
    ~MemoryBlockDevice();
    
    // BlockDevice接口实现
    size_t get_size() const override;
    u32 get_sector_size() const override;
    u32 get_block_size() const override;
    bool is_readonly() const override;
    
    Result<size_t> read(sector_t sector, void* buffer, size_t size) override;
    Result<size_t> write(sector_t sector, const void* buffer, size_t size) override;
    Result<void> flush() override;
    Result<void> trim(sector_t sector, size_t size) override;
    
    void submit_bio(std::unique_ptr<Bio> bio) override;
    
    std::string get_name() const override;
    u32 get_major() const override;
    u32 get_minor() const override;
    
    // 特殊方法
    void load_from_file(const std::string& filename);  // 从文件加载
    void save_to_file(const std::string& filename);    // 保存到文件
};

// 文件块设备实现
class FileBlockDevice : public BlockDevice {
private:
    std::string filename_;                        // 文件名
    int fd_;                                      // 文件描述符
    size_t size_;                                 // 设备大小
    u32 sector_size_;                             // 扇区大小
    u32 block_size_;                              // 块大小
    bool readonly_;                               // 是否只读
    u32 major_, minor_;                           // 设备号
    
    mutable std::mutex mutex_;                    // 保护并发访问

public:
    FileBlockDevice(const std::string& filename, bool readonly = false,
                    u32 sector_size = SECTOR_SIZE, u32 block_size = BLOCK_SIZE,
                    u32 major = 8, u32 minor = 1);
    ~FileBlockDevice();
    
    // BlockDevice接口实现
    size_t get_size() const override;
    u32 get_sector_size() const override;
    u32 get_block_size() const override;
    bool is_readonly() const override;
    
    Result<size_t> read(sector_t sector, void* buffer, size_t size) override;
    Result<size_t> write(sector_t sector, const void* buffer, size_t size) override;
    Result<void> flush() override;
    Result<void> trim(sector_t sector, size_t size) override;
    
    void submit_bio(std::unique_ptr<Bio> bio) override;
    
    std::string get_name() const override;
    u32 get_major() const override;
    u32 get_minor() const override;
};

#endif // BLOCK_DEVICE_H