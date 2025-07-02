# 虚拟文件系统 (VFS - Virtual File System)

## 概述

VFS是文件系统的"大总管"，就像一个统一的"文件管理局"。它为所有文件系统（EXT4、NTFS、FAT32等）提供统一的接口，让应用程序不用关心底层文件系统的具体实现。

想象一下：
- VFS就像一个"万能翻译官"
- 不管你说的是中文、英文还是日文（不同文件系统）
- 翻译官都能理解并转换成统一的"标准语言"
- 这样大家都能互相交流（应用程序都能访问文件）

## 数据结构关系图

```
应用程序层 (open, read, write...)
    ↓
┌─────────────────────────────────────────────────────────────┐
│                    VFS抽象层                                │
│  ┌───────────────┐  ┌───────────────┐  ┌─────────────────┐  │
│  │   SuperBlock  │  │     Inode     │  │     Dentry      │  │
│  │   文件系统    │  │   文件节点    │  │   目录项缓存    │  │
│  │   超级块      │  │   (文件/目录) │  │   (路径解析)    │  │
│  └───────────────┘  └───────────────┘  └─────────────────┘  │
│           ↓                 ↓                    ↓          │
│  ┌───────────────┐  ┌───────────────┐  ┌─────────────────┐  │
│  │     File      │  │  InodeOps     │  │    VfsMount     │  │
│  │   文件句柄    │  │  文件操作接口 │  │   挂载点管理    │  │
│  │   (打开的文件)│  │              │  │                 │  │
│  └───────────────┘  └───────────────┘  └─────────────────┘  │
└─────────────────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────────────────┐
│                具体文件系统实现                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │    EXT4     │  │    NTFS     │  │       FAT32         │  │
│  │             │  │             │  │                     │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
    ↓
块设备层 (Block Device)
```

## 头文件详解 (vfs.h)

### 核心数据结构

#### FileAttribute - 文件属性

就像每个人的身份证，记录文件的基本信息：

```cpp
struct FileAttribute {
    FileMode mode;              // 文件权限和类型 (像身份证的职业栏)
    u32 uid;                   // 用户ID (谁拥有这个文件)
    u32 gid;                   // 组ID (属于哪个用户组)
    u64 size;                  // 文件大小 (文件有多大)
    u64 blocks;                // 占用的块数 (占用多少存储空间)
    std::chrono::system_clock::time_point atime;  // 最后访问时间
    std::chrono::system_clock::time_point mtime;  // 最后修改时间
    std::chrono::system_clock::time_point ctime;  // 状态改变时间
    u32 nlink;                 // 硬链接数 (有多少个名字指向这个文件)
    u32 blksize;               // 块大小
};
```

#### DirentEntry - 目录项

就像电话簿中的一条记录：

```cpp
struct DirentEntry {
    inode_t ino;                        // inode号 (身份证号码)
    std::string name;                   // 文件名 (姓名)
    FileType type;                      // 文件类型 (职业：文件/目录/链接等)
    
    DirentEntry(inode_t i, const std::string& n, FileType t)
        : ino(i), name(n), type(t) {}
};
```

### Inode 类 - 文件节点

Inode就像文件的"身份证"，记录文件的所有重要信息：

```cpp
class Inode : public std::enable_shared_from_this<Inode> {
private:
    inode_t ino_;                       // inode号 (唯一身份证号)
    SharedPtr<SuperBlock> sb_;          // 所属文件系统
    SharedPtr<InodeOperations> ops_;    // 文件操作接口
    FileAttribute attr_;                // 文件属性
    mutable std::mutex mutex_;          // 并发保护锁
    std::atomic<u32> ref_count_;        // 引用计数 (有多少人在使用)
    
    // 缓存的页面(用于文件内容)
    std::unordered_map<offset_t, SharedPtr<Page>> pages_;

public:
    Inode(inode_t ino, SharedPtr<SuperBlock> sb, SharedPtr<InodeOperations> ops);
    
    // 文件内容操作
    Result<size_t> read(offset_t pos, void* buffer, size_t size);    // 读文件
    Result<size_t> write(offset_t pos, const void* buffer, size_t size);  // 写文件
    
    // 目录操作
    Result<std::vector<DirentEntry>> readdir();                     // 列出目录内容
    Result<SharedPtr<Inode>> lookup(const std::string& name);       // 查找文件
    Result<SharedPtr<Inode>> create(const std::string& name, FileMode mode);  // 创建文件
    Result<void> unlink(const std::string& name);                   // 删除文件
    Result<void> mkdir(const std::string& name, FileMode mode);     // 创建目录
    Result<void> rmdir(const std::string& name);                    // 删除目录
    
    // 属性操作
    FileAttribute get_attr() const { return attr_; }
    Result<void> set_attr(const FileAttribute& attr);
    Result<void> truncate(u64 size);                                // 截断文件
};
```

### SuperBlock 类 - 文件系统超级块

SuperBlock就像文件系统的"户口本"，记录整个文件系统的基本信息：

```cpp
class SuperBlock {
private:
    SharedPtr<BlockDevice> device_;     // 存储设备
    SharedPtr<FileSystem> fs_;          // 文件系统类型
    SharedPtr<Inode> root_inode_;       // 根目录inode
    
    // 文件系统统计信息
    u64 total_blocks_;                  // 总块数
    u64 free_blocks_;                   // 空闲块数
    u64 total_inodes_;                  // 总inode数
    u64 free_inodes_;                   // 空闲inode数
    
    mutable std::shared_mutex mutex_;   // 读写锁
    
public:
    SuperBlock(SharedPtr<BlockDevice> device, SharedPtr<FileSystem> fs);
    
    // 基本信息
    SharedPtr<BlockDevice> get_device() const { return device_; }
    SharedPtr<FileSystem> get_ops() const { return fs_; }
    SharedPtr<Inode> get_root() const { return root_inode_; }
    
    // 统计信息
    u64 get_total_blocks() const { return total_blocks_; }
    u64 get_free_blocks() const { return free_blocks_; }
    u64 get_total_inodes() const { return total_inodes_; }
    u64 get_free_inodes() const { return free_inodes_; }
};
```

### Dentry 类 - 目录项缓存

Dentry就像"路径记忆"，记住怎么从路径找到文件：

```cpp
class Dentry {
private:
    std::string name_;                  // 目录/文件名
    SharedPtr<Inode> inode_;           // 对应的inode
    SharedPtr<Dentry> parent_;         // 父目录
    std::unordered_map<std::string, SharedPtr<Dentry>> children_;  // 子目录/文件
    
    mutable std::mutex mutex_;
    std::atomic<u32> ref_count_;
    
public:
    Dentry(const std::string& name, SharedPtr<Inode> inode, SharedPtr<Dentry> parent = nullptr);
    
    // 路径解析
    SharedPtr<Dentry> lookup_child(const std::string& name);
    void add_child(SharedPtr<Dentry> child);
    void remove_child(const std::string& name);
    
    // 获取完整路径
    std::string get_path() const;
    
    // 访问器
    const std::string& get_name() const { return name_; }
    SharedPtr<Inode> get_inode() const { return inode_; }
    SharedPtr<Dentry> get_parent() const { return parent_; }
};
```

### File 类 - 打开的文件

File就像"借书卡"，记录你打开文件的状态：

```cpp
class File {
private:
    SharedPtr<Inode> inode_;           // 对应的文件
    SharedPtr<Dentry> dentry_;         // 对应的目录项
    u32 flags_;                        // 打开标志 (只读/读写/追加等)
    offset_t pos_;                     // 当前读写位置
    
    mutable std::mutex mutex_;
    
public:
    File(SharedPtr<Inode> inode, SharedPtr<Dentry> dentry, u32 flags);
    
    // 文件操作
    Result<size_t> read(void* buffer, size_t size);
    Result<size_t> write(const void* buffer, size_t size);
    Result<offset_t> seek(offset_t offset, int whence);
    Result<void> sync();
    
    // 获取信息
    offset_t get_pos() const { return pos_; }
    u32 get_flags() const { return flags_; }
    SharedPtr<Inode> get_inode() const { return inode_; }
};
```

## 源文件实现详解

### inode.cpp - 文件节点实现

#### 文件读取操作

```cpp
Result<size_t> Inode::read(offset_t pos, void* buffer, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查读取范围
    if (pos >= attr_.size) {
        return 0;  // 超出文件末尾
    }
    
    // 调整读取大小
    size_t actual_size = std::min(size, static_cast<size_t>(attr_.size - pos));
    size_t bytes_read = 0;
    
    // 按页读取
    while (bytes_read < actual_size) {
        offset_t page_offset = (pos + bytes_read) & ~(PAGE_SIZE - 1);  // 页面对齐
        offset_t page_pos = (pos + bytes_read) % PAGE_SIZE;            // 页内偏移
        
        // 从页面缓存获取页面
        auto page_result = g_page_cache.read_page(shared_from_this(), page_offset);
        if (page_result.is_err()) {
            return page_result.error();
        }
        
        auto page = page_result.unwrap();
        size_t copy_size = std::min(actual_size - bytes_read, PAGE_SIZE - page_pos);
        
        // 复制数据
        std::memcpy(static_cast<u8*>(buffer) + bytes_read, 
                   page->get_data() + page_pos, copy_size);
        
        bytes_read += copy_size;
        page->put();  // 释放页面引用
    }
    
    // 更新访问时间
    attr_.atime = std::chrono::system_clock::now();
    
    return bytes_read;
}
```

#### 实际操作例子

**例子1：读取文件内容**

```cpp
// 操作前状态：
// 文件内容: "Hello, World! This is a test file."
// 文件大小: 35字节
// 当前位置: 0

char buffer[20];
auto result = inode->read(7, buffer, 13);  // 从位置7开始读取13字节

// 操作后状态：
// buffer = "World! This i"
// result.unwrap() = 13
// 文件访问时间已更新
```

#### 目录操作实现

```cpp
Result<std::vector<DirentEntry>> Inode::readdir() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查是否为目录
    if (attr_.mode.type() != FileType::DIRECTORY) {
        return ErrorCode::FS_ENOTDIR;
    }
    
    // 调用具体文件系统的实现
    return ops_->readdir(shared_from_this());
}

Result<SharedPtr<Inode>> Inode::lookup(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (attr_.mode.type() != FileType::DIRECTORY) {
        return ErrorCode::FS_ENOTDIR;
    }
    
    return ops_->lookup(shared_from_this(), name);
}
```

### dentry.cpp - 目录项缓存实现

#### 路径解析

```cpp
SharedPtr<Dentry> Dentry::lookup_child(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = children_.find(name);
    if (it != children_.end()) {
        return it->second;  // 缓存命中
    }
    
    // 缓存未命中，从inode查找
    if (inode_) {
        auto inode_result = inode_->lookup(name);
        if (inode_result.is_ok()) {
            auto child_inode = inode_result.unwrap();
            auto child_dentry = std::make_shared<Dentry>(name, child_inode, shared_from_this());
            children_[name] = child_dentry;  // 加入缓存
            return child_dentry;
        }
    }
    
    return nullptr;  // 未找到
}

std::string Dentry::get_path() const {
    if (!parent_) {
        return "/";  // 根目录
    }
    
    std::string parent_path = parent_->get_path();
    if (parent_path == "/") {
        return "/" + name_;
    } else {
        return parent_path + "/" + name_;
    }
}
```

#### 实际操作例子

**例子2：路径解析过程**

```cpp
// 解析路径 "/home/user/document.txt"

// 1. 从根目录开始
auto root_dentry = get_root_dentry();  // "/"

// 2. 查找 "home"
auto home_dentry = root_dentry->lookup_child("home");
// 操作后：
// root_dentry->children_["home"] = home_dentry
// home_dentry->get_path() = "/home"

// 3. 查找 "user" 
auto user_dentry = home_dentry->lookup_child("user");
// 操作后：
// home_dentry->children_["user"] = user_dentry  
// user_dentry->get_path() = "/home/user"

// 4. 查找 "document.txt"
auto file_dentry = user_dentry->lookup_child("document.txt");
// 操作后：
// user_dentry->children_["document.txt"] = file_dentry
// file_dentry->get_path() = "/home/user/document.txt"
```

### file.cpp - 文件操作实现

#### 文件读写

```cpp
Result<size_t> File::read(void* buffer, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查文件是否可读
    if (!(flags_ & O_RDONLY) && !(flags_ & O_RDWR)) {
        return ErrorCode::FS_EACCES;
    }
    
    // 从inode读取数据
    auto result = inode_->read(pos_, buffer, size);
    if (result.is_ok()) {
        pos_ += result.unwrap();  // 更新文件位置
    }
    
    return result;
}

Result<size_t> File::write(const void* buffer, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查文件是否可写
    if (!(flags_ & O_WRONLY) && !(flags_ & O_RDWR)) {
        return ErrorCode::FS_EACCES;
    }
    
    // 如果是追加模式，移动到文件末尾
    if (flags_ & O_APPEND) {
        pos_ = inode_->get_attr().size;
    }
    
    // 写入数据到inode
    auto result = inode_->write(pos_, buffer, size);
    if (result.is_ok()) {
        pos_ += result.unwrap();  // 更新文件位置
    }
    
    return result;
}
```

#### 实际操作例子

**例子3：文件操作序列**

```cpp
// 打开文件进行读写
auto file = std::make_shared<File>(inode, dentry, O_RDWR);

// 操作前状态：
// file->pos_ = 0
// 文件内容: "Hello"

// 1. 读取3字节
char read_buf[10];
auto read_result = file->read(read_buf, 3);

// 操作后状态：
// read_buf = "Hel"
// file->pos_ = 3
// read_result.unwrap() = 3

// 2. 写入5字节
const char* write_data = " World";
auto write_result = file->write(write_data, 6);

// 操作后状态：
// file->pos_ = 9
// 文件内容: "Hel World"
// write_result.unwrap() = 6

// 3. 定位到文件开头
auto seek_result = file->seek(0, SEEK_SET);

// 操作后状态：
// file->pos_ = 0
// seek_result.unwrap() = 0
```

### super_block.cpp - 文件系统超级块

```cpp
SuperBlock::SuperBlock(SharedPtr<BlockDevice> device, SharedPtr<FileSystem> fs)
    : device_(device), fs_(fs), total_blocks_(0), free_blocks_(0),
      total_inodes_(0), free_inodes_(0) {
    
    // 读取文件系统超级块信息
    auto mount_result = fs_->mount(device);
    if (mount_result.is_ok()) {
        // 获取根目录inode
        root_inode_ = fs_->get_root_inode();
        
        // 获取文件系统统计信息
        auto stat = fs_->get_stat();
        total_blocks_ = stat.total_blocks;
        free_blocks_ = stat.free_blocks;
        total_inodes_ = stat.total_inodes;
        free_inodes_ = stat.free_inodes;
    }
}
```

## 完整使用场景和例子

### 场景1：创建和访问文件

```cpp
// 就像在办公室创建和使用文档

// 1. 挂载文件系统
auto device = std::make_shared<MemoryBlockDevice>(64 * 1024 * 1024);
auto fs = std::make_shared<Ext4FileSystem>();
auto sb = std::make_shared<SuperBlock>(device, fs);

// 2. 获取根目录
auto root = sb->get_root();

// 3. 创建文件
auto create_result = root->create("test.txt", FileMode(0644));
if (create_result.is_ok()) {
    auto file_inode = create_result.unwrap();
    
    // 4. 写入内容
    const char* content = "Hello, VFS World!";
    auto write_result = file_inode->write(0, content, strlen(content));
    
    std::cout << "写入了 " << write_result.unwrap() << " 字节" << std::endl;
}

// 5. 查找并读取文件
auto lookup_result = root->lookup("test.txt");
if (lookup_result.is_ok()) {
    auto file_inode = lookup_result.unwrap();
    
    char buffer[100];
    auto read_result = file_inode->read(0, buffer, sizeof(buffer));
    buffer[read_result.unwrap()] = '\0';
    
    std::cout << "文件内容: " << buffer << std::endl;
}
```

### 场景2：目录树操作

```cpp
// 就像创建文件夹结构

auto root = sb->get_root();

// 1. 创建目录结构: /docs/projects/
auto docs_result = root->mkdir("docs", FileMode(0755));
if (docs_result.is_ok()) {
    auto docs_inode = root->lookup("docs").unwrap();
    auto projects_result = docs_inode->mkdir("projects", FileMode(0755));
    
    // 2. 在projects目录下创建文件
    if (projects_result.is_ok()) {
        auto projects_inode = docs_inode->lookup("projects").unwrap();
        auto file_result = projects_inode->create("readme.md", FileMode(0644));
        
        if (file_result.is_ok()) {
            auto readme_inode = file_result.unwrap();
            const char* content = "# Project Documentation\n\nThis is a readme file.";
            readme_inode->write(0, content, strlen(content));
        }
    }
}

// 3. 遍历目录
auto entries_result = root->readdir();
if (entries_result.is_ok()) {
    auto entries = entries_result.unwrap();
    
    std::cout << "根目录内容:" << std::endl;
    for (const auto& entry : entries) {
        std::cout << "  " << entry.name << " (inode: " << entry.ino 
                  << ", type: " << (int)entry.type << ")" << std::endl;
    }
}
```

### 场景3：文件属性操作

```cpp
// 就像查看和修改文件属性

auto file_inode = root->lookup("test.txt").unwrap();

// 1. 获取文件属性
auto attr = file_inode->get_attr();

std::cout << "文件信息:" << std::endl;
std::cout << "  大小: " << attr.size << " 字节" << std::endl;
std::cout << "  权限: " << std::oct << attr.mode.mode << std::endl;
std::cout << "  用户ID: " << attr.uid << std::endl;
std::cout << "  硬链接数: " << attr.nlink << std::endl;

// 2. 修改文件大小
auto truncate_result = file_inode->truncate(10);
if (truncate_result.is_ok()) {
    auto new_attr = file_inode->get_attr();
    std::cout << "截断后大小: " << new_attr.size << " 字节" << std::endl;
}
```

### 场景4：Dentry缓存效果

```cpp
// 就像智能路径记忆

auto root_dentry = std::make_shared<Dentry>("/", root, nullptr);

// 第一次访问路径 - 建立缓存
auto start_time = std::chrono::high_resolution_clock::now();
auto file_dentry1 = resolve_path(root_dentry, "/docs/projects/readme.md");
auto time1 = std::chrono::high_resolution_clock::now() - start_time;

// 第二次访问相同路径 - 使用缓存
start_time = std::chrono::high_resolution_clock::now();
auto file_dentry2 = resolve_path(root_dentry, "/docs/projects/readme.md");
auto time2 = std::chrono::high_resolution_clock::now() - start_time;

std::cout << "第一次路径解析: " << time1.count() << "ns" << std::endl;
std::cout << "第二次路径解析: " << time2.count() << "ns" << std::endl;
std::cout << "缓存效果: " << (file_dentry1 == file_dentry2 ? "命中" : "未命中") << std::endl;
```

### 场景5：文件系统统计

```cpp
// 就像查看硬盘使用情况

std::cout << "文件系统统计:" << std::endl;
std::cout << "  总块数: " << sb->get_total_blocks() << std::endl;
std::cout << "  空闲块数: " << sb->get_free_blocks() << std::endl;
std::cout << "  使用率: " << (double)(sb->get_total_blocks() - sb->get_free_blocks()) 
             / sb->get_total_blocks() * 100 << "%" << std::endl;

std::cout << "  总inode数: " << sb->get_total_inodes() << std::endl;
std::cout << "  空闲inode数: " << sb->get_free_inodes() << std::endl;
```

## VFS的设计优势

1. **统一接口**：所有文件系统都通过相同的接口访问
2. **路径缓存**：Dentry缓存加速路径解析
3. **分层设计**：清晰的抽象层次，便于扩展
4. **并发安全**：支持多线程并发访问
5. **灵活挂载**：支持多个文件系统同时存在

VFS就像文件系统世界的"联合国"，让不同的文件系统能够和谐共存，为应用程序提供统一的服务！