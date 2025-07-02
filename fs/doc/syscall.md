# 系统调用子系统 (System Call)

## 概述

系统调用子系统是应用程序与文件系统之间的"翻译官"和"接待员"。它提供了标准的POSIX接口，让应用程序能够通过熟悉的函数（如open、read、write）来访问文件系统，而不需要了解底层的复杂实现。

想象一下：
- 系统调用就像酒店的"前台服务"
- 客人（应用程序）说"我要开房间"（open文件）
- 前台（系统调用）理解需求并办理手续（调用VFS接口）
- 提供房卡（文件描述符）给客人使用
- 管理所有房间的使用状态（文件描述符表）

## 数据结构关系图

```
应用程序层
    ↓ (open, read, write, close...)
┌─────────────────────────────────────────────────────────────┐
│                 系统调用接口层                              │
│  ┌───────────────┐  ┌───────────────┐  ┌─────────────────┐  │
│  │   文件操作    │  │   目录操作    │  │   属性操作      │  │
│  │ - open()      │  │ - mkdir()     │  │ - stat()        │  │
│  │ - read()      │  │ - readdir()   │  │ - chmod()       │  │
│  │ - write()     │  │ - opendir()   │  │ - chown()       │  │
│  │ - close()     │  │ - closedir()  │  │ - access()      │  │
│  │ - lseek()     │  │ - rmdir()     │  │ - truncate()    │  │
│  └───────────────┘  └───────────────┘  └─────────────────┘  │
└─────────────────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────────────────┐
│                文件描述符管理层                             │
│  ┌─────────────────────────────────────────────────────────┐│
│  │                 FdTable                                 ││
│  │  进程文件描述符表                                       ││
│  │  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐       ││
│  │  │fd:0 │ │fd:1 │ │fd:2 │ │fd:3 │ │fd:4 │ │fd:5 │ ...   ││
│  │  │stdin│ │stdout││stderr││File*││File*││File*│       ││
│  │  └─────┘ └─────┘ └─────┘ └─────┘ └─────┘ └─────┘       ││
│  │    ↓       ↓       ↓       ↓       ↓       ↓           ││
│  │   标准流           用户打开的文件                       ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────────────────┐
│                    VFS层                                    │
│  ┌─────────────────┐ ┌─────────────────┐ ┌───────────────┐  │
│  │      File       │ │     Inode       │ │    Dentry     │  │
│  │    文件对象     │ │    文件节点     │ │   目录项      │  │
│  └─────────────────┘ └─────────────────┘ └───────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## 头文件详解 (syscall.h)

### 文件操作标志

```cpp
// 文件打开标志 - 就像酒店房间的不同服务类型
#define O_RDONLY    0x0000      // 只读 - 只能看不能改
#define O_WRONLY    0x0001      // 只写 - 只能改不能看  
#define O_RDWR      0x0002      // 读写 - 既能看又能改
#define O_CREAT     0x0040      // 创建 - 房间不存在就新建
#define O_EXCL      0x0080      // 独占 - 房间必须是新的
#define O_TRUNC     0x0200      // 截断 - 清空原有内容
#define O_APPEND    0x0400      // 追加 - 只在末尾添加内容
#define O_NONBLOCK  0x0800      // 非阻塞 - 不等待，立即返回
#define O_SYNC      0x1000      // 同步 - 写入立即生效

// 文件访问权限测试
#define F_OK        0           // 文件存在性测试
#define R_OK        4           // 可读性测试
#define W_OK        2           // 可写性测试  
#define X_OK        1           // 可执行性测试

// 文件定位模式
#define SEEK_SET    0           // 从文件开头计算偏移
#define SEEK_CUR    1           // 从当前位置计算偏移
#define SEEK_END    2           // 从文件末尾计算偏移
```

### 文件状态结构

```cpp
struct stat {
    dev_t st_dev;           // 设备ID
    ino_t st_ino;           // inode号 - 文件的身份证号
    mode_t st_mode;         // 文件类型和权限
    nlink_t st_nlink;       // 硬链接数
    uid_t st_uid;           // 用户ID
    gid_t st_gid;           // 组ID
    dev_t st_rdev;          // 设备文件的设备ID
    off_t st_size;          // 文件大小
    time_t st_atime;        // 最后访问时间
    time_t st_mtime;        // 最后修改时间
    time_t st_ctime;        // 状态改变时间
    blksize_t st_blksize;   // 块大小
    blkcnt_t st_blocks;     // 分配的块数
};
```

### 目录操作结构

```cpp
struct dirent {
    ino_t d_ino;            // inode号
    off_t d_off;            // 偏移量
    unsigned short d_reclen; // 记录长度
    unsigned char d_type;    // 文件类型
    char d_name[256];       // 文件名
};

// 目录句柄
struct DIR {
    int fd;                 // 对应的文件描述符
    off_t pos;              // 当前读取位置
    struct dirent current;  // 当前目录项
    SharedPtr<Inode> inode; // 目录的inode
    std::vector<DirentEntry> entries;  // 缓存的目录项
    size_t entry_index;     // 当前索引
};
```

### FdTable 类 - 文件描述符表

每个进程都有自己的文件描述符表，就像每个客人都有自己的房卡管理册：

```cpp
class FdTable {
private:
    static constexpr int MAX_FDS = 1024;    // 最大文件描述符数
    
    std::array<SharedPtr<File>, MAX_FDS> files_;  // 文件对象数组
    std::bitset<MAX_FDS> used_;                   // 使用位图
    mutable std::shared_mutex mutex_;             // 读写锁
    
    // 标准输入输出
    SharedPtr<File> stdin_;
    SharedPtr<File> stdout_;
    SharedPtr<File> stderr_;
    
public:
    FdTable();
    ~FdTable();
    
    // 分配和释放文件描述符
    Result<int> allocate_fd(SharedPtr<File> file);   // 分配新的fd
    Result<void> free_fd(int fd);                     // 释放fd
    
    // 获取文件对象
    Result<SharedPtr<File>> get_file(int fd);        // 根据fd获取文件
    
    // 标准流管理
    void set_stdin(SharedPtr<File> file);
    void set_stdout(SharedPtr<File> file);
    void set_stderr(SharedPtr<File> file);
    
    // 复制文件描述符
    Result<int> duplicate_fd(int old_fd);             // dup()
    Result<int> duplicate_fd2(int old_fd, int new_fd); // dup2()
    
    // 获取统计信息
    size_t get_open_count() const;                    // 已打开文件数
    std::vector<int> get_all_fds() const;            // 所有已使用的fd
};
```

### SystemCall 类 - 系统调用管理器

这是系统调用的"总调度室"：

```cpp
class SystemCall {
private:
    static thread_local std::unique_ptr<FdTable> current_process_fds_;  // 当前进程的fd表
    static SharedPtr<VFS> vfs_;                                         // VFS实例
    static std::string current_directory_;                              // 当前工作目录
    static mutable std::mutex global_mutex_;                           // 全局锁
    
public:
    // 初始化系统调用子系统
    static Result<void> initialize(SharedPtr<VFS> vfs);
    static void cleanup();
    
    // 文件操作系统调用
    static int open(const char* pathname, int flags, mode_t mode = 0);
    static int close(int fd);
    static ssize_t read(int fd, void* buf, size_t count);
    static ssize_t write(int fd, const void* buf, size_t count);
    static off_t lseek(int fd, off_t offset, int whence);
    static int fsync(int fd);
    
    // 目录操作
    static int mkdir(const char* pathname, mode_t mode);
    static int rmdir(const char* pathname);
    static DIR* opendir(const char* name);
    static struct dirent* readdir(DIR* dirp);
    static int closedir(DIR* dirp);
    
    // 文件管理
    static int unlink(const char* pathname);
    static int rename(const char* oldpath, const char* newpath);
    static int link(const char* oldpath, const char* newpath);
    
    // 文件属性
    static int stat(const char* pathname, struct stat* statbuf);
    static int fstat(int fd, struct stat* statbuf);
    static int chmod(const char* pathname, mode_t mode);
    static int access(const char* pathname, int mode);
    
    // 工作目录
    static int chdir(const char* path);
    static char* getcwd(char* buf, size_t size);
};
```

## 源文件实现详解

### fd_table.cpp - 文件描述符表实现

#### 文件描述符分配

```cpp
Result<int> FdTable::allocate_fd(SharedPtr<File> file) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // 从fd 3开始分配（0,1,2是标准流）
    for (int fd = 3; fd < MAX_FDS; ++fd) {
        if (!used_[fd]) {
            files_[fd] = file;
            used_.set(fd);
            return fd;
        }
    }
    
    return ErrorCode::FS_EMFILE;  // 文件描述符耗尽
}

Result<void> FdTable::free_fd(int fd) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    if (fd < 0 || fd >= MAX_FDS || !used_[fd]) {
        return ErrorCode::FS_EBADF;  // 无效的文件描述符
    }
    
    files_[fd].reset();
    used_.reset(fd);
    return Result<void>();
}

Result<SharedPtr<File>> FdTable::get_file(int fd) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (fd < 0 || fd >= MAX_FDS || !used_[fd]) {
        return ErrorCode::FS_EBADF;
    }
    
    return files_[fd];
}
```

#### 实际操作例子

**例子1：进程启动时的fd表初始化**

```cpp
// 操作前状态：
// used_ = [0,0,0,0,0,0...] (全部未使用)
// files_ = [null,null,null...] (全部为空)

FdTable fd_table;

// 操作后状态：
// used_ = [1,1,1,0,0,0...] (前3个已使用)
// files_[0] = stdin_文件对象
// files_[1] = stdout_文件对象  
// files_[2] = stderr_文件对象
```

**例子2：打开新文件时分配fd**

```cpp
// 操作前状态：
// used_ = [1,1,1,0,0,0...]
// 下一个可用fd = 3

auto file = std::make_shared<File>(inode, dentry, O_RDWR);
auto fd_result = fd_table.allocate_fd(file);

// 操作后状态：
// used_ = [1,1,1,1,0,0...]
// files_[3] = file对象
// fd_result.unwrap() = 3
```

### syscall.cpp - 系统调用实现

#### open系统调用

```cpp
int SystemCall::open(const char* pathname, int flags, mode_t mode) {
    try {
        // 1. 解析路径
        std::string path_str(pathname);
        if (path_str[0] != '/') {
            // 相对路径，转换为绝对路径
            path_str = current_directory_ + "/" + path_str;
        }
        
        // 2. 解析路径到dentry
        auto dentry_result = vfs_->resolve_path(path_str);
        SharedPtr<Inode> inode;
        SharedPtr<Dentry> dentry;
        
        if (dentry_result.is_ok()) {
            // 文件已存在
            dentry = dentry_result.unwrap();
            inode = dentry->get_inode();
            
            // 检查O_CREAT | O_EXCL标志
            if ((flags & O_CREAT) && (flags & O_EXCL)) {
                errno = EEXIST;
                return -1;  // 文件已存在但要求独占创建
            }
            
            // 检查O_TRUNC标志
            if (flags & O_TRUNC) {
                auto truncate_result = inode->truncate(0);
                if (truncate_result.is_err()) {
                    errno = EIO;
                    return -1;
                }
            }
        } else {
            // 文件不存在
            if (!(flags & O_CREAT)) {
                errno = ENOENT;
                return -1;  // 文件不存在且不创建
            }
            
            // 创建新文件
            auto create_result = vfs_->create_file(path_str, FileMode(mode));
            if (create_result.is_err()) {
                errno = EIO;
                return -1;
            }
            
            auto create_info = create_result.unwrap();
            inode = create_info.first;
            dentry = create_info.second;
        }
        
        // 3. 创建File对象
        auto file = std::make_shared<File>(inode, dentry, flags);
        
        // 4. 分配文件描述符
        auto fd_result = current_process_fds_->allocate_fd(file);
        if (fd_result.is_err()) {
            errno = EMFILE;
            return -1;
        }
        
        return fd_result.unwrap();
        
    } catch (const std::exception& e) {
        errno = EIO;
        return -1;
    }
}
```

#### 实际操作例子

**例子3：打开已存在文件**

```cpp
// 操作前状态：
// 文件系统中存在 "/home/user/test.txt"
// 当前进程fd表：[stdin, stdout, stderr, ...]

int fd = SystemCall::open("/home/user/test.txt", O_RDWR);

// 操作过程：
// 1. 解析路径 "/home/user/test.txt"
// 2. 通过VFS找到对应的inode和dentry
// 3. 创建File对象，flags=O_RDWR
// 4. 分配fd=3
// 5. fd_table.files_[3] = File对象

// 操作后状态：
// 返回值：fd = 3
// fd表：[stdin, stdout, stderr, File("/home/user/test.txt"), ...]
```

**例子4：创建新文件**

```cpp
// 操作前状态：
// 文件 "/tmp/newfile.txt" 不存在

int fd = SystemCall::open("/tmp/newfile.txt", O_CREAT | O_WRONLY, 0644);

// 操作过程：
// 1. 解析路径，发现文件不存在
// 2. 因为有O_CREAT标志，调用VFS创建文件
// 3. 创建成功后获得新的inode和dentry
// 4. 创建File对象，flags=O_WRONLY
// 5. 分配fd=4

// 操作后状态：
// 文件系统中新建了 "/tmp/newfile.txt"，权限644
// 返回值：fd = 4
// fd表：[stdin, stdout, stderr, File1, File("/tmp/newfile.txt"), ...]
```

#### read/write系统调用

```cpp
ssize_t SystemCall::read(int fd, void* buf, size_t count) {
    // 1. 获取文件对象
    auto file_result = current_process_fds_->get_file(fd);
    if (file_result.is_err()) {
        errno = EBADF;
        return -1;
    }
    
    auto file = file_result.unwrap();
    
    // 2. 检查读取权限
    if (!file->can_read()) {
        errno = EACCES;
        return -1;
    }
    
    // 3. 执行读取
    auto read_result = file->read(buf, count);
    if (read_result.is_err()) {
        errno = EIO;
        return -1;
    }
    
    return static_cast<ssize_t>(read_result.unwrap());
}

ssize_t SystemCall::write(int fd, const void* buf, size_t count) {
    auto file_result = current_process_fds_->get_file(fd);
    if (file_result.is_err()) {
        errno = EBADF;
        return -1;
    }
    
    auto file = file_result.unwrap();
    
    if (!file->can_write()) {
        errno = EACCES;
        return -1;
    }
    
    auto write_result = file->write(buf, count);
    if (write_result.is_err()) {
        errno = EIO;
        return -1;
    }
    
    return static_cast<ssize_t>(write_result.unwrap());
}
```

#### 实际操作例子

**例子5：文件读写操作**

```cpp
// 操作前状态：
// fd=3 指向文件 "Hello, World!"，当前位置=0

char buffer[10];
ssize_t bytes_read = SystemCall::read(3, buffer, 5);

// 操作过程：
// 1. 从fd表获取fd=3对应的File对象
// 2. 检查文件可读性
// 3. 调用file->read(buffer, 5)
// 4. File对象从当前位置读取5字节
// 5. 更新文件位置指针

// 操作后状态：
// buffer = "Hello"
// bytes_read = 5
// 文件位置从0移动到5

// 继续写入
const char* data = " Linux";
ssize_t bytes_written = SystemCall::write(3, data, 6);

// 操作后状态：
// 文件内容变为 "Hello Linux"
// bytes_written = 6
// 文件位置从5移动到11
```

#### 目录操作实现

```cpp
DIR* SystemCall::opendir(const char* name) {
    try {
        // 1. 解析目录路径
        auto dentry_result = vfs_->resolve_path(name);
        if (dentry_result.is_err()) {
            errno = ENOENT;
            return nullptr;
        }
        
        auto dentry = dentry_result.unwrap();
        auto inode = dentry->get_inode();
        
        // 2. 检查是否为目录
        if (inode->get_attr().mode.type() != FileType::DIRECTORY) {
            errno = ENOTDIR;
            return nullptr;
        }
        
        // 3. 读取目录内容
        auto entries_result = inode->readdir();
        if (entries_result.is_err()) {
            errno = EIO;
            return nullptr;
        }
        
        // 4. 创建DIR结构
        auto dir = std::make_unique<DIR>();
        dir->fd = -1;  // 目录不使用fd
        dir->pos = 0;
        dir->inode = inode;
        dir->entries = entries_result.unwrap();
        dir->entry_index = 0;
        
        return dir.release();
        
    } catch (const std::exception& e) {
        errno = EIO;
        return nullptr;
    }
}

struct dirent* SystemCall::readdir(DIR* dirp) {
    if (!dirp || dirp->entry_index >= dirp->entries.size()) {
        return nullptr;  // 到达目录末尾
    }
    
    // 获取当前目录项
    const auto& entry = dirp->entries[dirp->entry_index];
    
    // 填充dirent结构
    dirp->current.d_ino = entry.ino;
    dirp->current.d_off = dirp->entry_index;
    dirp->current.d_reclen = sizeof(struct dirent);
    dirp->current.d_type = vfs_type_to_dt(entry.type);
    
    // 复制文件名
    size_t name_len = std::min(entry.name.length(), sizeof(dirp->current.d_name) - 1);
    std::memcpy(dirp->current.d_name, entry.name.c_str(), name_len);
    dirp->current.d_name[name_len] = '\0';
    
    // 移动到下一项
    dirp->entry_index++;
    
    return &dirp->current;
}
```

#### 实际操作例子

**例子6：目录遍历**

```cpp
// 操作前状态：
// 目录 "/home/user" 包含：
// - "." (inode: 100)
// - ".." (inode: 50)  
// - "documents" (inode: 101, 目录)
// - "photo.jpg" (inode: 102, 文件)

DIR* dir = SystemCall::opendir("/home/user");

// 操作过程：
// 1. 解析路径找到目录inode
// 2. 调用inode->readdir()获取所有目录项
// 3. 创建DIR结构，缓存目录项列表
// 4. entry_index = 0

// 操作后状态：
// dir->entries = [
//   {".", 100, DIRECTORY},
//   {"..", 50, DIRECTORY},
//   {"documents", 101, DIRECTORY},
//   {"photo.jpg", 102, REGULAR}
// ]
// dir->entry_index = 0

// 遍历目录
struct dirent* entry;
while ((entry = SystemCall::readdir(dir)) != nullptr) {
    printf("文件名: %s, inode: %lu, 类型: %d\n", 
           entry->d_name, entry->d_ino, entry->d_type);
}

// 输出：
// 文件名: ., inode: 100, 类型: 4
// 文件名: .., inode: 50, 类型: 4
// 文件名: documents, inode: 101, 类型: 4
// 文件名: photo.jpg, inode: 102, 类型: 8
```

#### 文件属性操作

```cpp
int SystemCall::stat(const char* pathname, struct stat* statbuf) {
    try {
        // 1. 解析路径
        auto dentry_result = vfs_->resolve_path(pathname);
        if (dentry_result.is_err()) {
            errno = ENOENT;
            return -1;
        }
        
        auto dentry = dentry_result.unwrap();
        auto inode = dentry->get_inode();
        auto attr = inode->get_attr();
        
        // 2. 填充stat结构
        std::memset(statbuf, 0, sizeof(struct stat));
        statbuf->st_ino = inode->get_ino();
        statbuf->st_mode = attr.mode.mode;
        statbuf->st_nlink = attr.nlink;
        statbuf->st_uid = attr.uid;
        statbuf->st_gid = attr.gid;
        statbuf->st_size = attr.size;
        statbuf->st_blocks = attr.blocks;
        statbuf->st_blksize = attr.blksize;
        
        // 转换时间格式
        statbuf->st_atime = std::chrono::system_clock::to_time_t(attr.atime);
        statbuf->st_mtime = std::chrono::system_clock::to_time_t(attr.mtime);
        statbuf->st_ctime = std::chrono::system_clock::to_time_t(attr.ctime);
        
        return 0;
        
    } catch (const std::exception& e) {
        errno = EIO;
        return -1;
    }
}
```

## 完整使用场景和例子

### 场景1：标准的文件操作流程

```cpp
// 就像在办公室处理文档的完整流程

// 1. 创建并打开文件
int fd = open("/tmp/work.txt", O_CREAT | O_RDWR, 0644);
if (fd == -1) {
    perror("无法创建文件");
    return -1;
}

printf("文件已打开，文件描述符: %d\n", fd);

// 2. 写入数据
const char* content = "今天的工作报告：\n1. 完成了系统调用模块\n2. 测试了文件操作\n";
ssize_t written = write(fd, content, strlen(content));
printf("写入了 %zd 字节\n", written);

// 3. 定位到文件开头
off_t pos = lseek(fd, 0, SEEK_SET);
printf("当前位置: %ld\n", pos);

// 4. 读取数据
char buffer[200];
ssize_t read_bytes = read(fd, buffer, sizeof(buffer) - 1);
buffer[read_bytes] = '\0';
printf("读取内容:\n%s\n", buffer);

// 5. 获取文件信息
struct stat file_stat;
if (fstat(fd, &file_stat) == 0) {
    printf("文件信息:\n");
    printf("  大小: %ld 字节\n", file_stat.st_size);
    printf("  权限: %o\n", file_stat.st_mode & 0777);
    printf("  inode: %lu\n", file_stat.st_ino);
}

// 6. 关闭文件
close(fd);
printf("文件已关闭\n");
```

### 场景2：目录操作和文件管理

```cpp
// 就像整理文件柜

// 1. 创建目录结构
if (mkdir("/tmp/project", 0755) == 0) {
    printf("创建项目目录成功\n");
}

if (mkdir("/tmp/project/docs", 0755) == 0) {
    printf("创建文档目录成功\n");
}

// 2. 在目录中创建文件
int readme_fd = open("/tmp/project/README.md", O_CREAT | O_WRONLY, 0644);
if (readme_fd != -1) {
    const char* readme_content = "# Project Documentation\n\nThis is a sample project.";
    write(readme_fd, readme_content, strlen(readme_content));
    close(readme_fd);
    printf("创建README文件成功\n");
}

// 3. 遍历目录
printf("\n项目目录内容:\n");
DIR* dir = opendir("/tmp/project");
if (dir) {
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // 获取完整路径
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "/tmp/project/%s", entry->d_name);
        
        // 获取文件详细信息
        struct stat entry_stat;
        if (stat(full_path, &entry_stat) == 0) {
            char type_char = S_ISDIR(entry_stat.st_mode) ? 'd' : '-';
            char perms[10];
            mode_to_string(entry_stat.st_mode, perms);
            
            printf("%c%s %3ld %ld %s\n", 
                   type_char, perms, entry_stat.st_nlink, 
                   entry_stat.st_size, entry->d_name);
        }
    }
    closedir(dir);
}

// 4. 文件操作
printf("\n测试文件访问权限:\n");
if (access("/tmp/project/README.md", F_OK) == 0) {
    printf("README.md 存在\n");
}
if (access("/tmp/project/README.md", R_OK) == 0) {
    printf("README.md 可读\n");
}
if (access("/tmp/project/README.md", W_OK) == 0) {
    printf("README.md 可写\n");
}
```

### 场景3：高级文件操作

```cpp
// 就像专业的文件管理操作

// 1. 文件复制
int copy_file(const char* src, const char* dst) {
    int src_fd = open(src, O_RDONLY);
    if (src_fd == -1) {
        perror("打开源文件失败");
        return -1;
    }
    
    int dst_fd = open(dst, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (dst_fd == -1) {
        perror("创建目标文件失败");
        close(src_fd);
        return -1;
    }
    
    char buffer[4096];
    ssize_t bytes;
    while ((bytes = read(src_fd, buffer, sizeof(buffer))) > 0) {
        if (write(dst_fd, buffer, bytes) != bytes) {
            perror("写入失败");
            break;
        }
    }
    
    close(src_fd);
    close(dst_fd);
    
    if (bytes == -1) {
        perror("读取失败");
        return -1;
    }
    
    printf("文件复制完成: %s -> %s\n", src, dst);
    return 0;
}

// 2. 文件链接操作
// 创建硬链接
if (link("/tmp/project/README.md", "/tmp/project/README_link.md") == 0) {
    printf("创建硬链接成功\n");
    
    // 检查链接数
    struct stat stat_buf;
    stat("/tmp/project/README.md", &stat_buf);
    printf("README.md 硬链接数: %ld\n", stat_buf.st_nlink);
}

// 3. 文件重命名
if (rename("/tmp/project/README_link.md", "/tmp/project/README_backup.md") == 0) {
    printf("文件重命名成功\n");
}

// 4. 权限修改
if (chmod("/tmp/project/README.md", 0755) == 0) {
    printf("修改文件权限成功\n");
    
    struct stat stat_buf;
    stat("/tmp/project/README.md", &stat_buf);
    printf("新权限: %o\n", stat_buf.st_mode & 0777);
}
```

### 场景4：工作目录管理

```cpp
// 就像在不同办公室之间切换工作

// 1. 获取当前工作目录
char current_dir[PATH_MAX];
if (getcwd(current_dir, sizeof(current_dir))) {
    printf("当前工作目录: %s\n", current_dir);
}

// 2. 切换到项目目录
if (chdir("/tmp/project") == 0) {
    printf("切换到项目目录成功\n");
    
    // 验证切换
    if (getcwd(current_dir, sizeof(current_dir))) {
        printf("新的工作目录: %s\n", current_dir);
    }
    
    // 3. 使用相对路径操作文件
    int fd = open("README.md", O_RDONLY);  // 相对路径
    if (fd != -1) {
        printf("使用相对路径打开文件成功\n");
        
        // 读取文件内容
        char buffer[100];
        ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
        buffer[bytes] = '\0';
        printf("文件内容片段: %.50s...\n", buffer);
        
        close(fd);
    }
}

// 4. 递归遍历目录树
void traverse_directory(const char* path, int depth = 0) {
    DIR* dir = opendir(path);
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // 打印缩进
        for (int i = 0; i < depth; i++) {
            printf("  ");
        }
        
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        struct stat entry_stat;
        if (stat(full_path, &entry_stat) == 0) {
            if (S_ISDIR(entry_stat.st_mode)) {
                printf("📁 %s/\n", entry->d_name);
                traverse_directory(full_path, depth + 1);  // 递归遍历
            } else {
                printf("📄 %s (%ld bytes)\n", entry->d_name, entry_stat.st_size);
            }
        }
    }
    
    closedir(dir);
}

printf("\n项目目录树:\n");
traverse_directory("/tmp/project");
```

### 场景5：错误处理和资源管理

```cpp
// 就像专业的错误处理和清理工作

class FileManager {
private:
    std::vector<int> open_fds_;
    std::vector<DIR*> open_dirs_;
    
public:
    ~FileManager() {
        // 自动清理资源
        cleanup();
    }
    
    int safe_open(const char* path, int flags, mode_t mode = 0) {
        int fd = open(path, flags, mode);
        if (fd != -1) {
            open_fds_.push_back(fd);
            printf("打开文件: %s (fd: %d)\n", path, fd);
        } else {
            perror("打开文件失败");
        }
        return fd;
    }
    
    DIR* safe_opendir(const char* path) {
        DIR* dir = opendir(path);
        if (dir) {
            open_dirs_.push_back(dir);
            printf("打开目录: %s\n", path);
        } else {
            perror("打开目录失败");
        }
        return dir;
    }
    
    void cleanup() {
        // 关闭所有打开的文件
        for (int fd : open_fds_) {
            close(fd);
            printf("关闭文件描述符: %d\n", fd);
        }
        open_fds_.clear();
        
        // 关闭所有打开的目录
        for (DIR* dir : open_dirs_) {
            closedir(dir);
            printf("关闭目录\n");
        }
        open_dirs_.clear();
    }
    
    // 批量文件操作
    bool batch_create_files(const std::vector<std::string>& filenames) {
        bool all_success = true;
        
        for (const auto& filename : filenames) {
            int fd = safe_open(filename.c_str(), O_CREAT | O_WRONLY, 0644);
            if (fd == -1) {
                all_success = false;
                continue;
            }
            
            // 写入默认内容
            std::string content = "# " + filename + "\n\nCreated automatically.\n";
            if (write(fd, content.c_str(), content.length()) == -1) {
                perror("写入文件失败");
                all_success = false;
            }
        }
        
        return all_success;
    }
};

// 使用示例
FileManager fm;

// 批量创建文件
std::vector<std::string> files = {
    "/tmp/project/docs/design.md",
    "/tmp/project/docs/api.md", 
    "/tmp/project/docs/user_guide.md"
};

if (fm.batch_create_files(files)) {
    printf("所有文件创建成功\n");
} else {
    printf("部分文件创建失败\n");
}

// 程序结束时自动清理资源
```

## 系统调用的设计优势

1. **标准接口**：提供POSIX兼容的标准接口，应用程序无需修改即可使用
2. **资源管理**：自动管理文件描述符，防止资源泄漏
3. **错误处理**：完善的错误码机制，便于调试和错误处理
4. **性能优化**：缓存机制和批量操作提高性能
5. **安全性**：权限检查和访问控制保护系统安全

系统调用子系统就像文件系统的"前台服务"，为应用程序提供友好、标准、高效的文件服务接口！