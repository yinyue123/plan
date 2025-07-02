# 块设备子系统 (Block Device)

## 概述

块设备子系统是文件系统的底层存储抽象层，就像是文件系统的"仓库管理员"。它负责管理存储设备，提供统一的块读写接口。

想象一下：
- 块设备就像一个巨大的图书馆
- 每本书都有固定的位置（块号）
- 你可以精确地找到并取出任何一本书（读取块）
- 也可以把书放回指定位置（写入块）

## 数据结构关系图

```
文件系统层
    ↓
┌─────────────────────────────────────┐
│         块设备抽象层                │
│  ┌─────────────────────────────────┐│
│  │       BlockDevice               ││
│  │  - device_id: 设备ID            ││
│  │  - size_: 设备大小              ││
│  │  - sector_size_: 扇区大小       ││
│  │  - block_size_: 块大小          ││
│  │  - readonly_: 是否只读          ││
│  └─────────────────────────────────┘│
└─────────────────────────────────────┘
    ↓
┌─────────────────────────────────────┐
│           具体实现                  │
│  ┌─────────────────┐ ┌─────────────┐│
│  │ MemoryBlockDevice│ │FileBlockDevice││
│  │  - data_: 内存块 │ │ - fd_: 文件描述││
│  │  - mutex_: 锁    │ │ - path_: 路径  ││
│  └─────────────────┘ └─────────────┘│
└─────────────────────────────────────┘
    ↓
┌─────────────────────────────────────┐
│            I/O请求                  │
│  ┌─────────────────────────────────┐│
│  │             Bio                 ││
│  │  - type_: READ/WRITE            ││
│  │  - sector_: 扇区号              ││
│  │  - size_: 数据大小              ││
│  │  - buffer_: 数据缓冲区          ││
│  │  - callback_: 完成回调          ││
│  └─────────────────────────────────┘│
└─────────────────────────────────────┘
```

## 头文件详解 (block_device.h)

### 核心类型定义

```cpp
// 扇区号类型 - 就像图书馆的书架号
using sector_t = u64;

// I/O操作类型
enum class BioType {
    READ,    // 读取 - 从书架上取书
    WRITE,   // 写入 - 把书放到书架上
    FLUSH,   // 刷新 - 整理书架
    DISCARD  // 丢弃 - 清空书架
};

// 错误码
enum class ErrorCode : s32 {
    SUCCESS = 0,        // 成功
    FS_EIO = -5,       // I/O错误
    FS_EROFS = -30,    // 只读文件系统
    // ...
};
```

### BlockDevice 基类

这是所有块设备的"总管"，定义了设备必须提供的基本操作：

```cpp
class BlockDevice {
protected:
    device_id_t device_id_;  // 设备身份证
    fs_size_t size_;        // 设备总容量（字节）
    u32 sector_size_;       // 扇区大小（通常512字节）
    u32 block_size_;        // 块大小（通常4096字节）
    bool readonly_;         // 是否只读
    std::string name_;      // 设备名称
    
public:
    // 同步读取 - 立即返回结果
    virtual Result<size_t> read(offset_t offset, void* buffer, size_t size) = 0;
    
    // 同步写入 - 立即返回结果  
    virtual Result<size_t> write(offset_t offset, const void* buffer, size_t size) = 0;
    
    // 异步I/O - 提交请求后立即返回，完成时调用回调
    virtual void submit_bio(std::unique_ptr<Bio> bio) = 0;
};
```

### Bio I/O请求类

Bio就像是"图书借阅单"，记录了你要做什么操作：

```cpp
class Bio {
private:
    BioType type_;           // 操作类型：借书还是还书？
    sector_t sector_;        // 目标扇区：第几个书架？
    size_t size_;           // 数据大小：要几本书？
    void* buffer_;          // 数据缓冲区：书放在哪个篮子里？
    BioCallback callback_;   // 完成回调：借书完成后通知我
    
public:
    Bio(BioType type, sector_t sector, size_t size, void* buffer, BioCallback callback);
};
```

## 源文件实现详解 (block_device.cpp)

### MemoryBlockDevice - 内存块设备

这就像一个"虚拟硬盘"，数据存在内存里：

```cpp
class MemoryBlockDevice : public BlockDevice {
private:
    std::vector<u8> data_;      // 数据存储区 - 虚拟硬盘空间
    mutable std::mutex mutex_;  // 并发访问锁 - 确保数据安全
    
public:
    MemoryBlockDevice(fs_size_t size, u32 sector_size = 512, u32 block_size = 4096);
};
```

#### 实际操作例子

**例子1：创建一个64MB的内存块设备**

```cpp
// 操作前：无设备
auto device = std::make_shared<MemoryBlockDevice>(64 * 1024 * 1024);

// 操作后：
// - data_.size() = 67108864 (64MB)
// - sector_size_ = 512
// - block_size_ = 4096  
// - readonly_ = false
```

**例子2：读取数据**

```cpp
// 操作前状态：
// data_[0-4095] = 全部是0

char buffer[1024];
auto result = device->read(0, buffer, 1024);

// 操作后：
// - buffer中包含data_[0-1023]的内容（全是0）
// - result.unwrap() = 1024 (读取的字节数)
```

**例子3：写入数据**

```cpp
// 操作前状态：
// data_[1000-1003] = {0, 0, 0, 0}

const char data[] = {0xAB, 0xCD, 0xEF, 0x12};
auto result = device->write(1000, data, 4);

// 操作后：
// data_[1000-1003] = {0xAB, 0xCD, 0xEF, 0x12}
// result.unwrap() = 4 (写入的字节数)
```

### FileBlockDevice - 文件块设备

这是把普通文件当作块设备使用，就像把文件夹当作硬盘：

```cpp
class FileBlockDevice : public BlockDevice {
private:
    int fd_;           // 文件描述符 - 文件的"门牌号"
    std::string path_; // 文件路径
    
public:
    FileBlockDevice(const std::string& path, bool create_if_not_exists = true);
};
```

#### 实际操作例子

**例子1：创建文件块设备**

```cpp
// 操作前：/tmp/test.img 不存在
auto device = std::make_shared<FileBlockDevice>("/tmp/test.img", true);

// 操作后：
// - 创建了 /tmp/test.img 文件
// - fd_ = 3 (文件描述符)
// - path_ = "/tmp/test.img"
```

**例子2：异步I/O操作**

```cpp
// 创建异步读取请求
bool completed = false;
std::string result_data;

auto bio = std::make_unique<Bio>(
    BioType::READ,           // 读取操作
    0,                       // 从第0个扇区开始
    1024,                    // 读取1024字节
    buffer,                  // 存储到buffer中
    [&](ErrorCode err) {     // 完成后的回调
        if (err == ErrorCode::SUCCESS) {
            result_data = "读取成功";
        }
        completed = true;
    }
);

// 提交请求
device->submit_bio(std::move(bio));

// 等待完成
while (!completed) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
```

## 使用场景和例子

### 场景1：文件系统初始化

```cpp
// 就像开一家图书馆

// 1. 准备硬盘空间（64MB）
auto storage = std::make_shared<MemoryBlockDevice>(64 * 1024 * 1024);

// 2. 检查设备信息
std::cout << "存储容量: " << storage->get_size() / (1024*1024) << "MB" << std::endl;
std::cout << "块大小: " << storage->get_block_size() << "字节" << std::endl;

// 输出：
// 存储容量: 64MB
// 块大小: 4096字节
```

### 场景2：数据读写操作

```cpp
// 就像在图书馆借书还书

// 准备数据
std::vector<u8> write_data(4096, 0xAB);  // 4KB数据，全部填充0xAB
std::vector<u8> read_data(4096, 0);      // 4KB空缓冲区

// 写入数据（还书）
auto write_result = storage->write(0, write_data.data(), write_data.size());
if (write_result.is_ok()) {
    std::cout << "写入了 " << write_result.unwrap() << " 字节" << std::endl;
}

// 读取数据（借书）  
auto read_result = storage->read(0, read_data.data(), read_data.size());
if (read_result.is_ok()) {
    std::cout << "读取了 " << read_result.unwrap() << " 字节" << std::endl;
    
    // 验证数据是否一致
    bool match = (write_data == read_data);
    std::cout << "数据验证: " << (match ? "成功" : "失败") << std::endl;
}
```

### 场景3：错误处理

```cpp
// 就像图书馆的安全检查

// 创建只读设备
auto readonly_storage = std::make_shared<MemoryBlockDevice>(1024, 512, 4096, true);

// 尝试写入只读设备
std::vector<u8> data(512, 0xFF);
auto result = readonly_storage->write(0, data.data(), data.size());

if (result.is_err()) {
    if (result.error() == ErrorCode::FS_EROFS) {
        std::cout << "错误：尝试写入只读设备！" << std::endl;
    }
}
```

### 场景4：边界检查

```cpp
// 就像检查书架是否存在

auto small_device = std::make_shared<MemoryBlockDevice>(1024);  // 只有1KB

// 尝试读取超出设备大小的数据
std::vector<u8> buffer(512);
auto result = small_device->read(2048, buffer.data(), buffer.size());  // 从2KB位置读取

if (result.is_err()) {
    std::cout << "错误：读取位置超出设备范围！" << std::endl;
}
```

## 关键设计特点

1. **抽象性**：通过基类定义统一接口，不同的存储介质（内存、文件、真实硬盘）都可以用相同方式操作
2. **异步支持**：Bio机制支持异步I/O，提高性能
3. **错误处理**：完善的错误码体系，便于调试和错误处理
4. **线程安全**：使用mutex保护并发访问
5. **灵活性**：支持不同的扇区大小和块大小配置

块设备子系统就像是文件系统的地基，为上层提供稳定可靠的存储服务！