# EXT4文件系统 (EXT4 File System)

## 概述

EXT4是Linux系统中最常用的文件系统，就像是一个"超级图书馆管理系统"。它不仅能存储文件，还能高效地组织、索引和管理海量数据，支持大文件、目录索引、日志等先进功能。

想象一下：
- EXT4就像一个现代化的智能图书馆
- 有完善的索引系统（inode表）
- 支持大型藏书（大文件支持）
- 有安全日志（journal）记录所有操作
- 能快速找到任何一本书（高效目录索引）

## 数据结构关系图

```
EXT4文件系统布局
┌─────────────────────────────────────────────────────────────┐
│                    物理存储布局                             │
│ ┌─────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────────┐ │
│ │Boot │ │   Group 0   │ │   Group 1   │ │    Group N...   │ │
│ │Block│ │             │ │             │ │                 │ │
│ └─────┘ └─────────────┘ └─────────────┘ └─────────────────┘ │
└─────────────────────────────────────────────────────────────┘

每个块组(Block Group)的结构：
┌─────────────────────────────────────────────────────────────┐
│ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────┐ │
│ │ Super Block │ │Group Descriptor││ Block Bitmap│ │ Inode   │ │
│ │   超级块    │ │   组描述符    │ │  块位图     │ │ Bitmap  │ │
│ └─────────────┘ └─────────────┘ └─────────────┘ └─────────┘ │
│ ┌─────────────┐ ┌───────────────────────────────────────────┐ │
│ │ Inode Table │ │          Data Blocks                     │ │
│ │  inode表    │ │          数据块                          │ │
│ └─────────────┘ └───────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘

Inode结构和文件数据关系：
┌─────────────────────────────────────────────────────────────┐
│                     Inode                                   │
│ ┌─────────────────┐ ┌─────────────────────────────────────┐ │
│ │    基本属性     │ │           块指针数组                │ │
│ │ - 文件大小      │ │ [0] → 直接块 (Direct Block)         │ │
│ │ - 权限模式      │ │ [1] → 直接块                        │ │
│ │ │ 用户ID       │ │ [2] → 直接块                        │ │
│ │ - 时间戳        │ │ ...                                 │ │
│ │ - 链接数        │ │ [11] → 直接块                       │ │
│ └─────────────────┘ │ [12] → 一级间接块                   │ │
│                     │ [13] → 二级间接块                   │ │
│                     │ [14] → 三级间接块                   │ │
│                     └─────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────────────────────────┐
│                   数据块组织                                │
│ 小文件:  [直接块0][直接块1][直接块2]...                     │
│                                                             │
│ 大文件:  [直接块们] → [一级间接] → [块指针] → [数据块]      │
│                    → [二级间接] → [指针块] → [指针] → [数据] │
│                    → [三级间接] → [指针块] → [指针] → [指针] │
└─────────────────────────────────────────────────────────────┘
```

## 头文件详解 (ext4.h)

### EXT4常量定义

```cpp
// EXT4魔数 - 文件系统的"身份证"
static constexpr u16 EXT4_SUPER_MAGIC = 0xEF53;

// 块组大小相关
static constexpr u32 EXT4_BLOCKS_PER_GROUP = 32768;    // 每组32K个块
static constexpr u32 EXT4_INODES_PER_GROUP = 8192;     // 每组8K个inode

// inode中的块指针数量
static constexpr u32 EXT4_N_BLOCKS = 15;               // 总共15个块指针
static constexpr u32 EXT4_NDIR_BLOCKS = 12;            // 12个直接块指针
static constexpr u32 EXT4_IND_BLOCK = 12;              // 一级间接块指针
static constexpr u32 EXT4_DIND_BLOCK = 13;             // 二级间接块指针  
static constexpr u32 EXT4_TIND_BLOCK = 14;             // 三级间接块指针

// 目录项类型
enum EXT4_FT : u8 {
    EXT4_FT_UNKNOWN = 0,    // 未知类型
    EXT4_FT_REG_FILE = 1,   // 普通文件
    EXT4_FT_DIR = 2,        // 目录
    EXT4_FT_CHRDEV = 3,     // 字符设备
    EXT4_FT_BLKDEV = 4,     // 块设备
    EXT4_FT_FIFO = 5,       // 命名管道
    EXT4_FT_SOCK = 6,       // 套接字
    EXT4_FT_SYMLINK = 7     // 符号链接
};
```

### EXT4超级块结构

超级块就像图书馆的"总登记册"，记录整个文件系统的基本信息：

```cpp
struct Ext4SuperBlock {
    u32 s_inodes_count;         // inode总数
    u32 s_blocks_count_lo;      // 块总数(低32位)
    u32 s_r_blocks_count_lo;    // 保留块数(低32位)
    u32 s_free_blocks_count_lo; // 空闲块数(低32位)
    u32 s_free_inodes_count;    // 空闲inode数
    u32 s_first_data_block;     // 第一个数据块号
    u32 s_log_block_size;       // 块大小(log2(块大小/1024))
    s32 s_log_frag_size;        // 片段大小
    u32 s_blocks_per_group;     // 每组块数
    u32 s_frags_per_group;      // 每组片段数
    u32 s_inodes_per_group;     // 每组inode数
    u32 s_mtime;                // 挂载时间
    u32 s_wtime;                // 写入时间
    u16 s_mnt_count;            // 挂载次数
    s16 s_max_mnt_count;        // 最大挂载次数
    u16 s_magic;                // 魔数(0xEF53)
    u16 s_state;                // 文件系统状态
    u16 s_errors;               // 错误处理方式
    u16 s_minor_rev_level;      // 次版本号
    
    // ... 还有很多字段
    
    // 辅助方法
    u32 get_block_size() const {
        return 1024 << s_log_block_size;  // 计算实际块大小
    }
    
    u64 get_blocks_count() const {
        return s_blocks_count_lo;  // 简化版，不考虑64位
    }
} __attribute__((packed));
```

### EXT4 inode结构

inode就像每本书的"详细登记卡"：

```cpp
struct Ext4Inode {
    u16 i_mode;                 // 文件类型和权限
    u16 i_uid;                  // 用户ID(低16位)
    u32 i_size_lo;              // 文件大小(低32位)
    u32 i_atime;                // 访问时间
    u32 i_ctime;                // 创建时间
    u32 i_mtime;                // 修改时间
    u32 i_dtime;                // 删除时间
    u16 i_gid;                  // 组ID(低16位)
    u16 i_links_count;          // 硬链接数
    u32 i_blocks_lo;            // 块数(低32位)
    u32 i_flags;                // 文件标志
    
    union {
        struct {
            u32 l_i_version;    // Linux版本
        } linux1;
    } osd1;                     // 操作系统相关数据1
    
    u32 i_block[EXT4_N_BLOCKS]; // 块指针数组(15个)
    u32 i_generation;           // 文件版本
    u32 i_file_acl_lo;          // 扩展属性块
    u32 i_size_high;            // 文件大小(高32位)
    
    // ... 更多字段
    
    union {
        struct {
            u16 l_i_uid_high;   // 用户ID(高16位)
            u16 l_i_gid_high;   // 组ID(高16位)
            u32 l_i_reserved2;
        } linux2;
    } osd2;                     // 操作系统相关数据2
    
    // 辅助方法
    bool is_reg() const { return (i_mode & 0xF000) == 0x8000; }  // 是否为普通文件
    bool is_dir() const { return (i_mode & 0xF000) == 0x4000; }  // 是否为目录
    bool is_lnk() const { return (i_mode & 0xF000) == 0xA000; }  // 是否为符号链接
    
    u64 get_size() const {
        return static_cast<u64>(i_size_high) << 32 | i_size_lo;
    }
    
    u32 get_uid() const {
        return static_cast<u32>(osd2.linux2.l_i_uid_high) << 16 | i_uid;
    }
} __attribute__((packed));
```

### EXT4目录项结构

目录项就像"目录索引卡"：

```cpp
struct Ext4DirEntry {
    u32 inode;                  // inode号
    u16 rec_len;                // 记录长度
    u8 name_len;                // 文件名长度
    u8 file_type;               // 文件类型
    char name[];                // 文件名(变长)
    
    // 获取下一个目录项
    Ext4DirEntry* next() {
        return reinterpret_cast<Ext4DirEntry*>(
            reinterpret_cast<u8*>(this) + rec_len
        );
    }
    
    // 获取文件名字符串
    std::string get_name() const {
        return std::string(name, name_len);
    }
} __attribute__((packed));
```

### EXT4文件系统类

这是EXT4的"大管家"，实现了具体的文件系统操作：

```cpp
class Ext4FileSystem : public FileSystem, public InodeOperations {
private:
    SharedPtr<BlockDevice> device_;     // 存储设备
    Ext4SuperBlock sb_;                 // 超级块
    
    // 块组管理
    std::vector<Ext4GroupDesc> groups_; // 块组描述符
    
    // 缓存
    std::unordered_map<u32, SharedPtr<Inode>> inode_cache_;  // inode缓存
    
    mutable std::shared_mutex mutex_;
    
public:
    Ext4FileSystem();
    
    // FileSystem接口实现
    Result<void> mount(SharedPtr<BlockDevice> device) override;
    Result<void> unmount() override;
    Result<void> format(SharedPtr<BlockDevice> device) override;
    SharedPtr<Inode> get_root_inode() override;
    
    // InodeOperations接口实现
    Result<size_t> read(SharedPtr<Inode> inode, offset_t pos, void* buffer, size_t size) override;
    Result<size_t> write(SharedPtr<Inode> inode, offset_t pos, const void* buffer, size_t size) override;
    Result<std::vector<DirentEntry>> readdir(SharedPtr<Inode> inode) override;
    Result<SharedPtr<Inode>> lookup(SharedPtr<Inode> dir, const std::string& name) override;
    Result<SharedPtr<Inode>> create(SharedPtr<Inode> dir, const std::string& name, FileMode mode) override;
    // ... 更多操作
};
```

## 源文件实现详解

### ext4.cpp - 主要文件系统操作

#### 文件系统格式化

格式化就像"建造图书馆"的过程：

```cpp
Result<void> Ext4FileSystem::format(SharedPtr<BlockDevice> device) {
    device_ = device;
    
    // 1. 计算文件系统参数
    u64 total_blocks = device->get_size() / BLOCK_SIZE;
    u32 groups_count = (total_blocks + EXT4_BLOCKS_PER_GROUP - 1) / EXT4_BLOCKS_PER_GROUP;
    
    // 2. 初始化超级块
    std::memset(&sb_, 0, sizeof(sb_));
    sb_.s_magic = EXT4_SUPER_MAGIC;                    // 设置魔数
    sb_.s_blocks_count_lo = total_blocks;              // 总块数
    sb_.s_inodes_count = groups_count * EXT4_INODES_PER_GROUP;  // 总inode数
    sb_.s_free_blocks_count_lo = total_blocks - groups_count * 10;  // 空闲块数(估算)
    sb_.s_free_inodes_count = sb_.s_inodes_count - 1;  // 空闲inode数(减去根目录)
    sb_.s_log_block_size = 2;                          // 4KB块大小
    sb_.s_blocks_per_group = EXT4_BLOCKS_PER_GROUP;
    sb_.s_inodes_per_group = EXT4_INODES_PER_GROUP;
    // ... 设置更多字段
    
    // 3. 写入超级块到设备
    auto write_result = device->write(1024, &sb_, sizeof(sb_));  // 超级块在1024字节偏移处
    if (write_result.is_err()) {
        return write_result.error();
    }
    
    // 4. 初始化块组
    for (u32 i = 0; i < groups_count; i++) {
        auto init_result = init_block_group(i);
        if (init_result.is_err()) {
            return init_result.error();
        }
    }
    
    // 5. 创建根目录
    auto root_result = create_root_directory();
    return root_result;
}
```

#### 实际操作例子

**例子1：格式化64MB设备**

```cpp
// 操作前状态：
// 设备：64MB原始存储空间
// 内容：全部为0

auto device = std::make_shared<MemoryBlockDevice>(64 * 1024 * 1024);
auto fs = std::make_shared<Ext4FileSystem>();
auto format_result = fs->format(device);

// 操作后状态：
// 超级块: 位置1024，包含文件系统元信息
// sb_.s_magic = 0xEF53
// sb_.s_blocks_count_lo = 16384 (64MB / 4KB)
// sb_.s_inodes_count = 8192 
// 
// 块组0: [超级块][组描述符][位图][inode表][数据块]
// 根目录inode: inode编号2，包含"."和".."项
```

#### 文件读取实现

```cpp
Result<size_t> Ext4FileSystem::read(SharedPtr<Inode> inode, offset_t pos, void* buffer, size_t size) {
    // 获取EXT4 inode
    auto ext4_inode_result = get_ext4_inode(inode->get_ino());
    if (ext4_inode_result.is_err()) {
        return ext4_inode_result.error();
    }
    
    auto ext4_inode = ext4_inode_result.unwrap();
    u64 file_size = ext4_inode.get_size();
    
    // 检查读取范围
    if (pos >= file_size) {
        return 0;  // 超出文件末尾
    }
    
    size_t actual_size = std::min(size, static_cast<size_t>(file_size - pos));
    size_t bytes_read = 0;
    
    // 按块读取
    while (bytes_read < actual_size) {
        // 计算当前位置对应的块号
        u32 block_index = (pos + bytes_read) / BLOCK_SIZE;
        u32 block_offset = (pos + bytes_read) % BLOCK_SIZE;
        
        // 获取物理块号
        auto block_num_result = get_block_number(ext4_inode, block_index);
        if (block_num_result.is_err()) {
            return block_num_result.error();
        }
        
        u32 physical_block = block_num_result.unwrap();
        if (physical_block == 0) {
            // 稀疏文件，填充零
            size_t zero_size = std::min(actual_size - bytes_read, BLOCK_SIZE - block_offset);
            std::memset(static_cast<u8*>(buffer) + bytes_read, 0, zero_size);
            bytes_read += zero_size;
            continue;
        }
        
        // 读取物理块
        std::vector<u8> block_data(BLOCK_SIZE);
        auto read_result = device_->read(physical_block * BLOCK_SIZE, block_data.data(), BLOCK_SIZE);
        if (read_result.is_err()) {
            return read_result.error();
        }
        
        // 复制所需部分
        size_t copy_size = std::min(actual_size - bytes_read, BLOCK_SIZE - block_offset);
        std::memcpy(static_cast<u8*>(buffer) + bytes_read, 
                   block_data.data() + block_offset, copy_size);
        
        bytes_read += copy_size;
    }
    
    return bytes_read;
}
```

#### 块号映射实现

这是EXT4的核心算法，将文件内的逻辑块号转换为物理块号：

```cpp
Result<u32> Ext4FileSystem::get_block_number(const Ext4Inode& inode, u32 block_index) {
    // 1. 直接块(0-11)
    if (block_index < EXT4_NDIR_BLOCKS) {
        return inode.i_block[block_index];  // 直接返回物理块号
    }
    
    u32 addrs_per_block = BLOCK_SIZE / sizeof(u32);  // 每块能存储多少个地址
    
    // 2. 一级间接块
    block_index -= EXT4_NDIR_BLOCKS;
    if (block_index < addrs_per_block) {
        u32 indirect_block = inode.i_block[EXT4_IND_BLOCK];
        if (indirect_block == 0) return 0;  // 未分配
        
        return read_indirect_block(indirect_block, block_index);
    }
    
    // 3. 二级间接块
    block_index -= addrs_per_block;
    if (block_index < addrs_per_block * addrs_per_block) {
        u32 double_indirect = inode.i_block[EXT4_DIND_BLOCK];
        if (double_indirect == 0) return 0;
        
        u32 first_level_index = block_index / addrs_per_block;
        u32 second_level_index = block_index % addrs_per_block;
        
        // 先读取一级间接块地址
        auto first_level_result = read_indirect_block(double_indirect, first_level_index);
        if (first_level_result.is_err() || first_level_result.unwrap() == 0) {
            return 0;
        }
        
        // 再读取二级间接块地址
        return read_indirect_block(first_level_result.unwrap(), second_level_index);
    }
    
    // 4. 三级间接块 - 类似二级，但多一层
    // ... 实现省略
    
    return ErrorCode::FS_EINVAL;  // 块索引超出范围
}
```

#### 实际操作例子

**例子2：读取大文件的块映射**

```cpp
// 假设有一个5MB的文件(约1280个4KB块)
// inode.i_block数组布局：

// 操作前：文件结构
// 文件大小: 5MB (1280块)
// i_block[0-11]: 直接块，指向前12个数据块
// i_block[12]: 一级间接块，指向包含1024个地址的块
// i_block[13]: 二级间接块(未使用)

// 读取操作：读取第500个块的数据
u32 block_index = 500;

// 映射过程：
// 1. block_index >= 12，不是直接块
// 2. block_index = 500 - 12 = 488
// 3. 488 < 1024，在一级间接块范围内
// 4. 读取i_block[12]指向的间接块
// 5. 从间接块的第488个位置读取物理块号
// 6. 得到实际存储数据的物理块号

auto block_result = fs->get_block_number(ext4_inode, 500);
// 返回：物理块号，例如3000
```

### ext4_inode.cpp - inode操作实现

#### inode分配

```cpp
Result<u32> Ext4FileSystem::allocate_inode() {
    // 1. 查找有空闲inode的块组
    for (u32 group = 0; group < groups_.size(); group++) {
        if (groups_[group].bg_free_inodes_count > 0) {
            // 2. 读取inode位图
            auto bitmap_result = read_inode_bitmap(group);
            if (bitmap_result.is_err()) continue;
            
            auto bitmap = bitmap_result.unwrap();
            
            // 3. 查找空闲位
            for (u32 bit = 0; bit < EXT4_INODES_PER_GROUP; bit++) {
                if (!test_bit(bitmap.get(), bit)) {
                    // 找到空闲inode
                    u32 inode_num = group * EXT4_INODES_PER_GROUP + bit + 1;
                    
                    // 4. 标记为已使用
                    set_bit(bitmap.get(), bit);
                    
                    // 5. 更新位图到磁盘
                    write_inode_bitmap(group, bitmap.get());
                    
                    // 6. 更新组描述符
                    groups_[group].bg_free_inodes_count--;
                    write_group_descriptor(group);
                    
                    // 7. 更新超级块
                    sb_.s_free_inodes_count--;
                    write_super_block();
                    
                    return inode_num;
                }
            }
        }
    }
    
    return ErrorCode::FS_ENOSPC;  // 没有空闲inode
}
```

#### 实际操作例子

**例子3：创建新文件时的inode分配**

```cpp
// 操作前状态：
// 块组0: bg_free_inodes_count = 8191 (已用inode #2 给根目录)
// inode位图: [1,1,0,0,0,0...] (inode 1,2已用)
// sb_.s_free_inodes_count = 8191

auto inode_result = fs->allocate_inode();

// 操作后状态：
// 分配了inode #3
// inode位图: [1,1,1,0,0,0...] (inode 1,2,3已用)
// 块组0: bg_free_inodes_count = 8190
// sb_.s_free_inodes_count = 8190
// 返回: inode_result.unwrap() = 3
```

### ext4_dir.cpp - 目录操作实现

#### 目录遍历

```cpp
Result<std::vector<DirentEntry>> Ext4FileSystem::readdir(SharedPtr<Inode> inode) {
    auto ext4_inode_result = get_ext4_inode(inode->get_ino());
    if (ext4_inode_result.is_err()) {
        return ext4_inode_result.error();
    }
    
    auto ext4_inode = ext4_inode_result.unwrap();
    if (!ext4_inode.is_dir()) {
        return ErrorCode::FS_ENOTDIR;
    }
    
    std::vector<DirentEntry> entries;
    u64 dir_size = ext4_inode.get_size();
    offset_t pos = 0;
    
    // 遍历目录的所有块
    while (pos < dir_size) {
        // 读取当前块
        std::vector<u8> block_data(BLOCK_SIZE);
        auto read_result = read(inode, pos, block_data.data(), BLOCK_SIZE);
        if (read_result.is_err()) {
            break;
        }
        
        // 解析目录项
        u32 offset = 0;
        while (offset < BLOCK_SIZE && pos + offset < dir_size) {
            auto* dir_entry = reinterpret_cast<Ext4DirEntry*>(block_data.data() + offset);
            
            // 检查目录项有效性
            if (dir_entry->rec_len == 0 || dir_entry->rec_len > BLOCK_SIZE - offset) {
                break;
            }
            
            if (dir_entry->inode != 0 && dir_entry->name_len > 0) {
                // 转换为VFS目录项
                DirentEntry entry(
                    dir_entry->inode,
                    dir_entry->get_name(),
                    ext4_ft_to_vfs_type(dir_entry->file_type)
                );
                entries.push_back(entry);
            }
            
            offset += dir_entry->rec_len;
        }
        
        pos += BLOCK_SIZE;
    }
    
    return entries;
}
```

#### 在目录中查找文件

```cpp
Result<SharedPtr<Inode>> Ext4FileSystem::lookup(SharedPtr<Inode> dir, const std::string& name) {
    // 1. 获取目录的所有项
    auto entries_result = readdir(dir);
    if (entries_result.is_err()) {
        return entries_result.error();
    }
    
    auto entries = entries_result.unwrap();
    
    // 2. 查找匹配的文件名
    for (const auto& entry : entries) {
        if (entry.name == name) {
            // 3. 加载对应的inode
            auto inode_result = load_inode(entry.ino);
            if (inode_result.is_ok()) {
                return inode_result.unwrap();
            }
        }
    }
    
    return ErrorCode::FS_ENOENT;  // 文件不存在
}
```

#### 实际操作例子

**例子4：在目录中查找文件**

```cpp
// 操作前状态：
// 根目录包含：
// - "." (inode: 2, 当前目录)
// - ".." (inode: 2, 父目录)  
// - "test.txt" (inode: 3, 普通文件)
// - "docs" (inode: 4, 目录)

auto root_inode = fs->get_root_inode();
auto lookup_result = fs->lookup(root_inode, "test.txt");

// 查找过程：
// 1. 调用readdir(root_inode)获取所有目录项
// 2. 遍历目录项，比较文件名
// 3. 找到"test.txt"，inode号为3
// 4. 调用load_inode(3)加载文件的inode
// 5. 返回加载的inode对象

// 操作后：
// lookup_result.unwrap()->get_ino() = 3
// 可以通过返回的inode进行文件操作
```

#### 创建目录项

```cpp
Result<void> Ext4FileSystem::add_dir_entry(SharedPtr<Inode> dir, const std::string& name, 
                                           u32 inode_num, u8 file_type) {
    auto ext4_inode_result = get_ext4_inode(dir->get_ino());
    if (ext4_inode_result.is_err()) {
        return ext4_inode_result.error();
    }
    
    auto ext4_inode = ext4_inode_result.unwrap();
    u64 dir_size = ext4_inode.get_size();
    
    // 计算新目录项需要的空间
    u32 entry_len = sizeof(Ext4DirEntry) + name.length();
    entry_len = (entry_len + 3) & ~3;  // 4字节对齐
    
    // 查找目录的最后一个块，看是否有足够空间
    if (dir_size > 0) {
        u32 last_block_offset = (dir_size - 1) / BLOCK_SIZE * BLOCK_SIZE;
        std::vector<u8> block_data(BLOCK_SIZE);
        
        auto read_result = read(dir, last_block_offset, block_data.data(), BLOCK_SIZE);
        if (read_result.is_ok()) {
            // 检查最后一个块是否有空间
            u32 used_space = calculate_used_space_in_block(block_data.data());
            if (BLOCK_SIZE - used_space >= entry_len) {
                // 有空间，在当前块添加
                return add_entry_to_block(dir, last_block_offset, block_data.data(), 
                                        name, inode_num, file_type);
            }
        }
    }
    
    // 需要分配新块
    auto new_block_result = allocate_block();
    if (new_block_result.is_err()) {
        return new_block_result.error();
    }
    
    // 在新块中添加目录项
    std::vector<u8> new_block_data(BLOCK_SIZE, 0);
    auto add_result = create_dir_entry_in_block(new_block_data.data(), name, inode_num, file_type);
    if (add_result.is_err()) {
        return add_result.error();
    }
    
    // 写入新块
    u32 new_block_offset = dir_size;
    auto write_result = write(dir, new_block_offset, new_block_data.data(), BLOCK_SIZE);
    if (write_result.is_err()) {
        return write_result.error();
    }
    
    // 更新目录大小
    ext4_inode.i_size_lo += BLOCK_SIZE;
    return update_inode(dir->get_ino(), ext4_inode);
}
```

## 完整使用场景和例子

### 场景1：创建文件系统并使用

```cpp
// 就像建造并使用一个图书馆

// 1. 创建存储设备（64MB）
auto device = std::make_shared<MemoryBlockDevice>(64 * 1024 * 1024);

// 2. 创建EXT4文件系统
auto fs = std::make_shared<Ext4FileSystem>();

// 3. 格式化设备
auto format_result = fs->format(device);
if (format_result.is_ok()) {
    std::cout << "文件系统格式化成功" << std::endl;
    
    // 4. 挂载文件系统
    auto mount_result = fs->mount(device);
    if (mount_result.is_ok()) {
        std::cout << "文件系统挂载成功" << std::endl;
        
        // 5. 获取根目录
        auto root = fs->get_root_inode();
        std::cout << "根目录inode: " << root->get_ino() << std::endl;
        
        // 6. 列出根目录内容
        auto entries_result = fs->readdir(root);
        if (entries_result.is_ok()) {
            auto entries = entries_result.unwrap();
            std::cout << "根目录内容:" << std::endl;
            for (const auto& entry : entries) {
                std::cout << "  " << entry.name << " (inode: " << entry.ino << ")" << std::endl;
            }
        }
    }
}
```

### 场景2：文件创建和读写

```cpp
// 就像在图书馆里添加新书并编辑内容

auto root = fs->get_root_inode();

// 1. 创建新文件
auto create_result = fs->create(root, "myfile.txt", FileMode(0644));
if (create_result.is_ok()) {
    auto file_inode = create_result.unwrap();
    std::cout << "创建文件成功，inode: " << file_inode->get_ino() << std::endl;
    
    // 2. 写入数据
    const char* content = "Hello, EXT4 World!\nThis is my first file.";
    auto write_result = fs->write(file_inode, 0, content, strlen(content));
    if (write_result.is_ok()) {
        std::cout << "写入了 " << write_result.unwrap() << " 字节" << std::endl;
        
        // 3. 读取数据验证
        char buffer[100];
        auto read_result = fs->read(file_inode, 0, buffer, sizeof(buffer));
        if (read_result.is_ok()) {
            buffer[read_result.unwrap()] = '\0';
            std::cout << "文件内容: " << buffer << std::endl;
        }
    }
}
```

### 场景3：目录操作

```cpp
// 就像在图书馆里创建分类区域

auto root = fs->get_root_inode();

// 1. 创建目录
auto mkdir_result = fs->mkdir(root, "documents", FileMode(0755));
if (mkdir_result.is_ok()) {
    std::cout << "创建目录成功" << std::endl;
    
    // 2. 查找目录
    auto lookup_result = fs->lookup(root, "documents");
    if (lookup_result.is_ok()) {
        auto docs_dir = lookup_result.unwrap();
        
        // 3. 在目录中创建文件
        auto file_result = fs->create(docs_dir, "readme.md", FileMode(0644));
        if (file_result.is_ok()) {
            auto readme_inode = file_result.unwrap();
            
            // 4. 写入README内容
            const char* readme_content = "# Documents Directory\n\nThis directory contains important documents.";
            fs->write(readme_inode, 0, readme_content, strlen(readme_content));
            
            std::cout << "在documents目录中创建了readme.md" << std::endl;
        }
        
        // 5. 列出目录内容
        auto dir_entries = fs->readdir(docs_dir);
        if (dir_entries.is_ok()) {
            std::cout << "documents目录内容:" << std::endl;
            for (const auto& entry : dir_entries.unwrap()) {
                std::cout << "  " << entry.name 
                         << " (类型: " << (int)entry.type << ")" << std::endl;
            }
        }
    }
}
```

### 场景4：大文件处理

```cpp
// 就像处理多卷的百科全书

auto root = fs->get_root_inode();

// 1. 创建大文件（测试间接块）
auto big_file_result = fs->create(root, "bigfile.dat", FileMode(0644));
if (big_file_result.is_ok()) {
    auto big_file = big_file_result.unwrap();
    
    // 2. 写入大量数据（超过直接块容量）
    const size_t chunk_size = 4096;  // 4KB块
    const size_t total_chunks = 20;  // 总共80KB，超过直接块限制
    
    std::vector<u8> chunk_data(chunk_size);
    for (size_t i = 0; i < total_chunks; i++) {
        // 为每个块生成不同的内容
        std::fill(chunk_data.begin(), chunk_data.end(), static_cast<u8>(i));
        
        auto write_result = fs->write(big_file, i * chunk_size, chunk_data.data(), chunk_size);
        if (write_result.is_ok()) {
            std::cout << "写入块 " << i << std::endl;
        }
    }
    
    // 3. 验证随机位置读取
    std::vector<u8> read_buffer(chunk_size);
    size_t test_chunk = 15;  // 读取第15个块
    
    auto read_result = fs->read(big_file, test_chunk * chunk_size, 
                               read_buffer.data(), chunk_size);
    if (read_result.is_ok()) {
        bool correct = std::all_of(read_buffer.begin(), read_buffer.end(),
                                  [test_chunk](u8 val) { return val == test_chunk; });
        std::cout << "第" << test_chunk << "块验证: " 
                 << (correct ? "成功" : "失败") << std::endl;
    }
}
```

### 场景5：文件系统统计和健康检查

```cpp
// 就像检查图书馆的使用情况

// 1. 获取文件系统统计信息
std::cout << "=== EXT4文件系统统计 ===" << std::endl;
std::cout << "魔数: 0x" << std::hex << fs->get_super_block().s_magic << std::endl;
std::cout << "块大小: " << fs->get_super_block().get_block_size() << " 字节" << std::endl;
std::cout << "总块数: " << fs->get_super_block().get_blocks_count() << std::endl;
std::cout << "空闲块数: " << fs->get_super_block().s_free_blocks_count_lo << std::endl;
std::cout << "总inode数: " << fs->get_super_block().s_inodes_count << std::endl;
std::cout << "空闲inode数: " << fs->get_super_block().s_free_inodes_count << std::endl;

// 2. 计算使用率
float block_usage = (float)(fs->get_super_block().get_blocks_count() - 
                           fs->get_super_block().s_free_blocks_count_lo) / 
                           fs->get_super_block().get_blocks_count() * 100;
std::cout << "块使用率: " << std::fixed << std::setprecision(2) << block_usage << "%" << std::endl;

// 3. 遍历根目录，统计文件
auto root = fs->get_root_inode();
auto entries_result = fs->readdir(root);
if (entries_result.is_ok()) {
    auto entries = entries_result.unwrap();
    int file_count = 0, dir_count = 0;
    
    for (const auto& entry : entries) {
        if (entry.name != "." && entry.name != "..") {
            if (entry.type == FileType::REGULAR) {
                file_count++;
            } else if (entry.type == FileType::DIRECTORY) {
                dir_count++;
            }
        }
    }
    
    std::cout << "根目录统计: " << file_count << " 个文件, " 
             << dir_count << " 个目录" << std::endl;
}
```

## EXT4的核心优势

1. **高效存储**：多级间接块支持大文件，稀疏文件节省空间
2. **快速访问**：inode直接块提供小文件的快速访问
3. **可扩展性**：块组结构支持大容量存储
4. **数据完整性**：位图管理确保空间分配的一致性
5. **兼容性**：与Linux标准EXT4格式兼容

EXT4就像一个现代化的智能图书馆，不仅能存储大量信息，还能快速、准确地找到你需要的任何内容！