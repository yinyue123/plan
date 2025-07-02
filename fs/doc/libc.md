# LibC库接口 (LibC Interface)

## 概述

LibC库接口是应用程序与文件系统交互的"最后一道桥梁"。它提供了我们日常编程中最熟悉的函数接口，如`fopen`、`printf`、`iostream`等，让程序员能够用最自然的方式操作文件，而不需要直接处理底层的系统调用。

想象一下：
- LibC就像一个"贴心的助手"
- 你说"我要打开一个文件写日记"（fopen）
- 助手帮你处理所有细节（调用open系统调用）
- 你只需要专注于写内容（fprintf, fwrite）
- 甚至可以用更方便的方式（C++的ofstream）

## 数据结构关系图

```
应用程序代码
    ↓ (fopen, printf, cout...)
┌─────────────────────────────────────────────────────────────┐
│                    LibC接口层                               │
│  ┌─────────────────┐ ┌─────────────────┐ ┌───────────────┐  │
│  │   C标准库接口   │ │   C++流接口     │ │  文件系统接口 │  │
│  │ - fopen/fclose  │ │ - ifstream      │ │ - exists()    │  │
│  │ - fread/fwrite  │ │ - ofstream      │ │ - is_directory│  │
│  │ - fprintf/fscanf│ │ - iostream      │ │ - file_size() │  │
│  │ - fseek/ftell   │ │ - stringstream  │ │ - copy_file() │  │
│  └─────────────────┘ └─────────────────┘ └───────────────┘  │
└─────────────────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────────────────┐
│                    缓冲管理层                               │
│  ┌─────────────────────────────────────────────────────────┐│
│  │                   FILE结构                              ││
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐││
│  │  │    缓冲区   │ │   文件状态  │ │      流控制         │││
│  │  │ - buffer_   │ │ - fd_       │ │ - mode_             │││
│  │  │ - buf_size_ │ │ - eof_      │ │ - position_         │││
│  │  │ - buf_pos_  │ │ - error_    │ │ - flags_            │││
│  │  └─────────────┘ └─────────────┘ └─────────────────────┘││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────────────────┐
│                  C++流缓冲层                                │
│  ┌─────────────────────────────────────────────────────────┐│
│  │                   FileBuf                               ││
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐││
│  │  │   输入缓冲  │ │   输出缓冲  │ │    文件操作         │││
│  │  │ - eback()   │ │ - pbase()   │ │ - fd_               │││
│  │  │ - gptr()    │ │ - pptr()    │ │ - overflow()        │││
│  │  │ - egptr()   │ │ - epptr()   │ │ - underflow()       │││
│  │  └─────────────┘ └─────────────┘ └─────────────────────┘││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
    ↓
系统调用层 (open, read, write, close...)
```

## 头文件详解 (libc.h)

### C标准库函数声明

#### 基础文件操作

```cpp
// 文件打开/关闭 - 就像开门关门
FILE* fopen(const char* pathname, const char* mode);     // 打开文件
FILE* fdopen(int fd, const char* mode);                  // 从fd创建FILE*
FILE* freopen(const char* pathname, const char* mode, FILE* stream);  // 重新打开
int fclose(FILE* stream);                                // 关闭文件
int fflush(FILE* stream);                                // 刷新缓冲区

// 文件模式字符串含义：
// "r"  - 只读，文件必须存在
// "w"  - 只写，截断文件或创建新文件
// "a"  - 追加，在文件末尾写入
// "r+" - 读写，文件必须存在
// "w+" - 读写，截断文件或创建新文件
// "a+" - 读追加，可读可在末尾写入
```

#### 文件I/O操作

```cpp
// 字符I/O - 一个字符一个字符操作
int fgetc(FILE* stream);                                 // 读一个字符
int fputc(int c, FILE* stream);                         // 写一个字符
int getc(FILE* stream);                                 // 读字符(可能是宏)
int putc(int c, FILE* stream);                          // 写字符(可能是宏)
int ungetc(int c, FILE* stream);                        // 退回一个字符

// 字符串I/O - 一行一行操作
char* fgets(char* s, int size, FILE* stream);          // 读一行
int fputs(const char* s, FILE* stream);                // 写字符串

// 二进制I/O - 批量数据操作
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);   // 读数据块
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream); // 写数据块

// 格式化I/O - 带格式的读写
int fprintf(FILE* stream, const char* format, ...);    // 格式化写入
int fscanf(FILE* stream, const char* format, ...);     // 格式化读取
int vfprintf(FILE* stream, const char* format, va_list ap);  // 可变参数版本
```

#### 文件定位操作

```cpp
// 文件位置控制 - 就像书签
int fseek(FILE* stream, long offset, int whence);      // 设置文件位置
long ftell(FILE* stream);                              // 获取当前位置
void rewind(FILE* stream);                             // 回到文件开头
int fgetpos(FILE* stream, fpos_t* pos);                // 获取位置(高级版)
int fsetpos(FILE* stream, const fpos_t* pos);          // 设置位置(高级版)

// whence参数：
// SEEK_SET - 从文件开头计算
// SEEK_CUR - 从当前位置计算  
// SEEK_END - 从文件末尾计算
```

#### 文件状态检查

```cpp
// 状态检查函数 - 就像健康检查
int feof(FILE* stream);                                 // 是否到达文件末尾
int ferror(FILE* stream);                               // 是否有错误发生
void clearerr(FILE* stream);                           // 清除错误标志
int fileno(FILE* stream);                               // 获取文件描述符
```

### C++流类定义

#### FileBuf类 - 流缓冲区

```cpp
namespace fs_libc {

class FileBuf : public std::streambuf {
private:
    int fd_;                    // 文件描述符
    char* buffer_;              // 缓冲区
    size_t buffer_size_;        // 缓冲区大小
    bool owns_fd_;              // 是否拥有fd（需要关闭）
    
public:
    FileBuf(int fd, bool owns_fd = false);              // 从fd构造
    FileBuf(const char* filename, std::ios_base::openmode mode);  // 从文件名构造
    ~FileBuf();
    
    bool is_open() const { return fd_ >= 0; }
    int get_fd() const { return fd_; }
    
protected:
    // 输入操作 - 从文件读到缓冲区
    int_type underflow() override;                      // 填充输入缓冲区
    int_type uflow() override;                          // 读取并移动指针
    
    // 输出操作 - 从缓冲区写到文件
    int_type overflow(int_type c = traits_type::eof()) override;  // 刷新输出缓冲区
    int sync() override;                                // 同步到文件
    
    // 定位操作
    pos_type seekoff(off_type off, std::ios_base::seekdir way,
                     std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;
    pos_type seekpos(pos_type sp,
                     std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;
};

}
```

#### 流类定义

```cpp
// 输入文件流 - 只能读取
class ifstream : public std::istream {
private:
    FileBuf buf_;
    
public:
    ifstream() : std::istream(&buf_), buf_(-1) {}
    explicit ifstream(const char* filename, std::ios_base::openmode mode = std::ios_base::in)
        : std::istream(&buf_), buf_(filename, mode | std::ios_base::in) {
        if (!buf_.is_open()) {
            setstate(std::ios_base::failbit);
        }
    }
    
    bool is_open() const { return buf_.is_open(); }
    void open(const char* filename, std::ios_base::openmode mode = std::ios_base::in);
    void close();
};

// 输出文件流 - 只能写入
class ofstream : public std::ostream {
private:
    FileBuf buf_;
    
public:
    ofstream() : std::ostream(&buf_), buf_(-1) {}
    explicit ofstream(const char* filename, std::ios_base::openmode mode = std::ios_base::out)
        : std::ostream(&buf_), buf_(filename, mode | std::ios_base::out) {
        if (!buf_.is_open()) {
            setstate(std::ios_base::failbit);
        }
    }
    
    bool is_open() const { return buf_.is_open(); }
    void open(const char* filename, std::ios_base::openmode mode = std::ios_base::out);
    void close();
};

// 双向文件流 - 可读可写
class fstream : public std::iostream {
private:
    FileBuf buf_;
    
public:
    fstream() : std::iostream(&buf_), buf_(-1) {}
    explicit fstream(const char* filename, 
                    std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
        : std::iostream(&buf_), buf_(filename, mode) {
        if (!buf_.is_open()) {
            setstate(std::ios_base::failbit);
        }
    }
};
```

#### 目录迭代器

```cpp
// 目录遍历器 - 就像文件夹浏览器
class directory_iterator {
private:
    DIR* dir_;                  // 目录句柄
    struct dirent* current_;    // 当前目录项
    std::string path_;          // 目录路径
    
public:
    directory_iterator() : dir_(nullptr), current_(nullptr) {}
    explicit directory_iterator(const std::string& path);
    ~directory_iterator();
    
    // 移动语义支持
    directory_iterator(directory_iterator&& other) noexcept;
    directory_iterator& operator=(directory_iterator&& other) noexcept;
    
    // 迭代器接口
    const struct dirent& operator*() const { return *current_; }
    const struct dirent* operator->() const { return current_; }
    
    directory_iterator& operator++();                   // 前置++
    directory_iterator operator++(int);                 // 后置++
    
    bool operator==(const directory_iterator& other) const;
    bool operator!=(const directory_iterator& other) const;
    
    static directory_iterator end() { return directory_iterator(); }
};
```

#### 文件系统操作函数

```cpp
// 高级文件系统操作 - 就像文件管理器功能
bool exists(const std::string& path);                  // 文件/目录是否存在
bool is_regular_file(const std::string& path);         // 是否为普通文件
bool is_directory(const std::string& path);            // 是否为目录
bool is_symlink(const std::string& path);              // 是否为符号链接
std::uintmax_t file_size(const std::string& path);     // 获取文件大小

bool create_directory(const std::string& path);        // 创建目录
bool create_directories(const std::string& path);      // 递归创建目录
bool remove(const std::string& path);                  // 删除文件/目录
std::uintmax_t remove_all(const std::string& path);    // 递归删除
void rename(const std::string& old_path, const std::string& new_path);  // 重命名
void copy_file(const std::string& from, const std::string& to);         // 复制文件

std::string current_path();                            // 获取当前目录
void current_path(const std::string& path);           // 设置当前目录
std::string absolute(const std::string& path);        // 获取绝对路径
```

## 源文件实现详解

### libc.cpp - C标准库实现

#### FILE结构定义

```cpp
// FILE结构 - 就像文件的"工作台"
struct FILE {
    int fd;                     // 文件描述符
    char* buffer;               // 缓冲区
    size_t buffer_size;         // 缓冲区大小
    size_t buffer_pos;          // 缓冲区当前位置
    size_t buffer_end;          // 缓冲区数据结束位置
    int flags;                  // 文件标志
    int error;                  // 错误状态
    int eof;                    // 文件末尾标志
    
    // 模式信息
    bool readable;              // 是否可读
    bool writable;              // 是否可写
    bool append_mode;           // 是否为追加模式
    
    FILE(int file_descriptor) : fd(file_descriptor), buffer(nullptr), 
         buffer_size(BUFSIZ), buffer_pos(0), buffer_end(0),
         flags(0), error(0), eof(0), readable(false), writable(false), append_mode(false) {
        buffer = new char[buffer_size];
    }
    
    ~FILE() {
        delete[] buffer;
    }
};
```

#### fopen实现

```cpp
FILE* fopen(const char* pathname, const char* mode) {
    // 1. 解析模式字符串
    int flags = 0;
    bool readable = false, writable = false, append_mode = false;
    
    switch (mode[0]) {
        case 'r':
            flags = O_RDONLY;
            readable = true;
            if (mode[1] == '+') {
                flags = O_RDWR;
                writable = true;
            }
            break;
            
        case 'w':
            flags = O_WRONLY | O_CREAT | O_TRUNC;
            writable = true;
            if (mode[1] == '+') {
                flags = O_RDWR | O_CREAT | O_TRUNC;
                readable = true;
            }
            break;
            
        case 'a':
            flags = O_WRONLY | O_CREAT | O_APPEND;
            writable = true;
            append_mode = true;
            if (mode[1] == '+') {
                flags = O_RDWR | O_CREAT | O_APPEND;
                readable = true;
            }
            break;
            
        default:
            return nullptr;  // 无效模式
    }
    
    // 2. 调用系统调用打开文件
    int fd = ::open(pathname, flags, 0644);
    if (fd == -1) {
        return nullptr;
    }
    
    // 3. 创建FILE结构
    FILE* file = new FILE(fd);
    file->readable = readable;
    file->writable = writable;
    file->append_mode = append_mode;
    
    return file;
}
```

#### 实际操作例子

**例子1：fopen的不同模式**

```cpp
// 模式 "r" - 只读模式
FILE* read_file = fopen("/tmp/data.txt", "r");
// 内部操作：
// flags = O_RDONLY
// fd = open("/tmp/data.txt", O_RDONLY)
// file->readable = true, writable = false

// 模式 "w" - 写入模式
FILE* write_file = fopen("/tmp/output.txt", "w");
// 内部操作：
// flags = O_WRONLY | O_CREAT | O_TRUNC
// 如果文件存在则清空，不存在则创建
// file->writable = true, readable = false

// 模式 "a+" - 追加读写模式
FILE* append_file = fopen("/tmp/log.txt", "a+");
// 内部操作：
// flags = O_RDWR | O_CREAT | O_APPEND
// file->readable = true, writable = true, append_mode = true
```

#### fread/fwrite实现

```cpp
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (!stream || !stream->readable || stream->error) {
        return 0;
    }
    
    size_t total_bytes = size * nmemb;
    size_t bytes_read = 0;
    char* dest = static_cast<char*>(ptr);
    
    while (bytes_read < total_bytes) {
        // 1. 检查缓冲区是否有数据
        if (stream->buffer_pos >= stream->buffer_end) {
            // 缓冲区空了，从文件读取
            ssize_t read_result = ::read(stream->fd, stream->buffer, stream->buffer_size);
            if (read_result <= 0) {
                if (read_result == 0) {
                    stream->eof = 1;  // 到达文件末尾
                } else {
                    stream->error = 1;  // 读取错误
                }
                break;
            }
            stream->buffer_pos = 0;
            stream->buffer_end = read_result;
        }
        
        // 2. 从缓冲区复制数据
        size_t available = stream->buffer_end - stream->buffer_pos;
        size_t to_copy = std::min(total_bytes - bytes_read, available);
        
        std::memcpy(dest + bytes_read, stream->buffer + stream->buffer_pos, to_copy);
        stream->buffer_pos += to_copy;
        bytes_read += to_copy;
    }
    
    return bytes_read / size;  // 返回读取的完整元素数
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (!stream || !stream->writable || stream->error) {
        return 0;
    }
    
    size_t total_bytes = size * nmemb;
    const char* src = static_cast<const char*>(ptr);
    
    // 如果是追加模式，确保在文件末尾
    if (stream->append_mode) {
        ::lseek(stream->fd, 0, SEEK_END);
    }
    
    // 直接写入（简化实现，实际可能需要缓冲）
    ssize_t written = ::write(stream->fd, src, total_bytes);
    if (written < 0) {
        stream->error = 1;
        return 0;
    }
    
    return written / size;
}
```

#### 实际操作例子

**例子2：缓冲区读取过程**

```cpp
// 假设文件内容："Hello, World! This is a test file."
FILE* file = fopen("test.txt", "r");

// 第一次fread - 缓冲区为空，需要从文件读取
char buffer1[5];
size_t read1 = fread(buffer1, 1, 5, file);

// 内部过程：
// 1. 检查stream->buffer，发现为空(buffer_pos >= buffer_end)
// 2. 调用::read(fd, stream->buffer, BUFSIZ)从文件读取数据
// 3. 假设读取了整个文件内容到缓冲区
// 4. stream->buffer_end = 35, stream->buffer_pos = 0
// 5. 从缓冲区复制5字节到buffer1
// 6. stream->buffer_pos = 5

// 结果：buffer1 = "Hello", read1 = 5

// 第二次fread - 缓冲区还有数据
char buffer2[7];
size_t read2 = fread(buffer2, 1, 7, file);

// 内部过程：
// 1. 检查缓冲区，还有数据(buffer_pos=5, buffer_end=35)
// 2. 直接从缓冲区复制7字节
// 3. stream->buffer_pos = 12

// 结果：buffer2 = ", World", read2 = 7
```

#### fprintf实现

```cpp
int fprintf(FILE* stream, const char* format, ...) {
    if (!stream || !stream->writable) {
        return -1;
    }
    
    va_list args;
    va_start(args, format);
    
    // 计算需要的缓冲区大小
    va_list args_copy;
    va_copy(args_copy, args);
    int needed_size = vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);
    
    if (needed_size < 0) {
        va_end(args);
        return -1;
    }
    
    // 分配临时缓冲区
    std::vector<char> temp_buffer(needed_size + 1);
    int result = vsnprintf(temp_buffer.data(), temp_buffer.size(), format, args);
    va_end(args);
    
    if (result < 0) {
        return -1;
    }
    
    // 写入数据
    size_t written = fwrite(temp_buffer.data(), 1, result, stream);
    return written == static_cast<size_t>(result) ? result : -1;
}
```

### cpp_streams.cpp - C++流实现

#### FileBuf的underflow实现

```cpp
std::streambuf::int_type FileBuf::underflow() {
    if (fd_ < 0) return traits_type::eof();
    
    // 读取数据到缓冲区
    ssize_t n = ::read(fd_, buffer_, buffer_size_);
    if (n <= 0) return traits_type::eof();
    
    // 设置输入缓冲区指针
    setg(buffer_,           // eback() - 缓冲区开始
         buffer_,           // gptr()  - 当前读取位置
         buffer_ + n);      // egptr() - 缓冲区结束
    
    return traits_type::to_int_type(*gptr());
}

std::streambuf::int_type FileBuf::overflow(int_type c) {
    if (fd_ < 0) return traits_type::eof();
    
    if (c != traits_type::eof()) {
        char ch = traits_type::to_char_type(c);
        if (::write(fd_, &ch, 1) != 1) {
            return traits_type::eof();
        }
    }
    return c;
}
```

#### 实际操作例子

**例子3：C++流的使用**

```cpp
// 写入文件
{
    fs_libc::ofstream ofs("output.txt");
    if (ofs.is_open()) {
        ofs << "Hello, C++ streams!" << std::endl;
        ofs << "Number: " << 42 << std::endl;
        ofs << "Float: " << 3.14159 << std::endl;
    }
    // 自动调用析构函数关闭文件
}

// 内部过程：
// 1. 构造ofstream，内部创建FileBuf
// 2. FileBuf调用open()系统调用
// 3. operator<<调用FileBuf::overflow()
// 4. overflow()调用::write()写入数据
// 5. 析构时自动关闭文件

// 读取文件
{
    fs_libc::ifstream ifs("output.txt");
    if (ifs.is_open()) {
        std::string line;
        while (std::getline(ifs, line)) {
            std::cout << "读取: " << line << std::endl;
        }
    }
}

// 内部过程：
// 1. 构造ifstream和FileBuf
// 2. getline()调用FileBuf::underflow()
// 3. underflow()调用::read()从文件读取
// 4. 数据通过streambuf机制传递给getline()
```

#### 文件系统操作实现

```cpp
bool exists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0;
}

bool is_regular_file(const std::string& path) {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) {
        return false;
    }
    return S_ISREG(st.st_mode);
}

bool is_directory(const std::string& path) {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

std::uintmax_t file_size(const std::string& path) {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) {
        return static_cast<std::uintmax_t>(-1);
    }
    return static_cast<std::uintmax_t>(st.st_size);
}

void copy_file(const std::string& from, const std::string& to) {
    int src_fd = ::open(from.c_str(), O_RDONLY);
    if (src_fd == -1) {
        throw std::runtime_error("Cannot open source file");
    }
    
    int dst_fd = ::open(to.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd == -1) {
        ::close(src_fd);
        throw std::runtime_error("Cannot create destination file");
    }
    
    char buffer[4096];
    ssize_t bytes;
    while ((bytes = ::read(src_fd, buffer, sizeof(buffer))) > 0) {
        if (::write(dst_fd, buffer, bytes) != bytes) {
            ::close(src_fd);
            ::close(dst_fd);
            throw std::runtime_error("Write error");
        }
    }
    
    ::close(src_fd);
    ::close(dst_fd);
}
```

## 完整使用场景和例子

### 场景1：传统C风格文件操作

```cpp
// 就像用传统方式写日记

void write_diary_entry() {
    // 1. 打开日记文件（追加模式）
    FILE* diary = fopen("/tmp/my_diary.txt", "a");
    if (!diary) {
        perror("无法打开日记文件");
        return;
    }
    
    // 2. 获取当前时间
    time_t now = time(nullptr);
    char* time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0';  // 移除换行
    
    // 3. 写入日记头部
    fprintf(diary, "\n=== %s ===\n", time_str);
    
    // 4. 写入日记内容
    const char* entries[] = {
        "今天学习了文件系统编程",
        "实现了LibC接口层",
        "理解了缓冲区机制",
        "感觉收获很大！"
    };
    
    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        fprintf(diary, "%zu. %s\n", i + 1, entries[i]);
    }
    
    // 5. 写入分隔线
    fputs("\n" + std::string(40, '-') + "\n", diary);
    
    // 6. 确保数据写入磁盘
    fflush(diary);
    
    // 7. 关闭文件
    fclose(diary);
    
    printf("日记写入完成\n");
}

void read_diary() {
    FILE* diary = fopen("/tmp/my_diary.txt", "r");
    if (!diary) {
        perror("无法打开日记文件");
        return;
    }
    
    printf("=== 我的日记 ===\n");
    
    char line[256];
    while (fgets(line, sizeof(line), diary)) {
        printf("%s", line);  // fgets保留换行符
    }
    
    if (ferror(diary)) {
        fprintf(stderr, "读取日记时发生错误\n");
    }
    
    fclose(diary);
}
```

### 场景2：C++现代风格操作

```cpp
// 就像用现代工具写报告

class ReportGenerator {
private:
    std::string base_path_;
    
public:
    ReportGenerator(const std::string& base_path) : base_path_(base_path) {
        // 确保目录存在
        if (!fs_libc::exists(base_path_)) {
            fs_libc::create_directories(base_path_);
        }
    }
    
    void generate_daily_report() {
        // 1. 生成文件名
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::ostringstream filename;
        filename << base_path_ << "/report_" 
                << std::put_time(&tm, "%Y%m%d") << ".md";
        
        // 2. 写入报告
        {
            fs_libc::ofstream report(filename.str());
            if (!report.is_open()) {
                throw std::runtime_error("无法创建报告文件");
            }
            
            // 写入Markdown格式的报告
            report << "# Daily Report\n\n";
            report << "**Date:** " << std::put_time(&tm, "%Y-%m-%d") << "\n\n";
            
            report << "## System Status\n\n";
            report << "- File system: ✅ Normal\n";
            report << "- Cache hit rate: " << 95.5 << "%\n";
            report << "- Storage usage: " << 75 << "%\n\n";
            
            report << "## Performance Metrics\n\n";
            std::vector<std::pair<std::string, double>> metrics = {
                {"Read operations/sec", 1250.5},
                {"Write operations/sec", 890.3},
                {"Average latency (ms)", 2.8}
            };
            
            for (const auto& metric : metrics) {
                report << "- **" << metric.first << "**: " 
                      << std::fixed << std::setprecision(1) 
                      << metric.second << "\n";
            }
            
            report << "\n## Summary\n\n";
            report << "All systems operating normally. "
                   << "Performance within expected parameters.\n";
        }  // 自动关闭文件
        
        std::cout << "报告已生成: " << filename.str() << std::endl;
    }
    
    void list_reports() {
        std::cout << "\n=== 报告列表 ===\n";
        
        try {
            for (fs_libc::directory_iterator it(base_path_); 
                 it != fs_libc::directory_iterator::end(); ++it) {
                
                std::string name = it->d_name;
                if (name.starts_with("report_") && name.ends_with(".md")) {
                    std::string full_path = base_path_ + "/" + name;
                    auto size = fs_libc::file_size(full_path);
                    
                    std::cout << "📄 " << name 
                             << " (" << size << " bytes)" << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "遍历目录时出错: " << e.what() << std::endl;
        }
    }
    
    void read_report(const std::string& date) {
        std::string filename = base_path_ + "/report_" + date + ".md";
        
        if (!fs_libc::exists(filename)) {
            std::cout << "报告文件不存在: " << filename << std::endl;
            return;
        }
        
        fs_libc::ifstream report(filename);
        if (!report.is_open()) {
            std::cout << "无法打开报告文件" << std::endl;
            return;
        }
        
        std::cout << "\n=== " << date << " 报告内容 ===\n";
        
        std::string line;
        while (std::getline(report, line)) {
            std::cout << line << std::endl;
        }
    }
};

// 使用示例
void demo_report_system() {
    ReportGenerator generator("/tmp/reports");
    
    // 生成今日报告
    generator.generate_daily_report();
    
    // 列出所有报告
    generator.list_reports();
    
    // 读取特定日期的报告
    generator.read_report("20241201");
}
```

### 场景3：数据处理和格式转换

```cpp
// 就像数据分析师处理数据

class DataProcessor {
public:
    struct Record {
        int id;
        std::string name;
        double value;
        std::chrono::system_clock::time_point timestamp;
    };
    
    void export_to_csv(const std::vector<Record>& records, const std::string& filename) {
        FILE* csv_file = fopen(filename.c_str(), "w");
        if (!csv_file) {
            throw std::runtime_error("Cannot create CSV file");
        }
        
        // 写入CSV头部
        fprintf(csv_file, "ID,Name,Value,Timestamp\n");
        
        // 写入数据记录
        for (const auto& record : records) {
            auto time_t = std::chrono::system_clock::to_time_t(record.timestamp);
            auto tm = *std::localtime(&time_t);
            
            fprintf(csv_file, "%d,\"%s\",%.2f,%04d-%02d-%02d %02d:%02d:%02d\n",
                   record.id,
                   record.name.c_str(),
                   record.value,
                   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                   tm.tm_hour, tm.tm_min, tm.tm_sec);
        }
        
        fclose(csv_file);
        printf("CSV文件已导出: %s\n", filename.c_str());
    }
    
    std::vector<Record> import_from_csv(const std::string& filename) {
        std::vector<Record> records;
        
        fs_libc::ifstream csv_file(filename);
        if (!csv_file.is_open()) {
            throw std::runtime_error("Cannot open CSV file");
        }
        
        std::string line;
        std::getline(csv_file, line);  // 跳过头部
        
        while (std::getline(csv_file, line)) {
            if (line.empty()) continue;
            
            // 简单的CSV解析（实际项目中应使用专业的CSV库）
            std::istringstream ss(line);
            std::string token;
            
            Record record;
            
            // 解析ID
            std::getline(ss, token, ',');
            record.id = std::stoi(token);
            
            // 解析名称（去除引号）
            std::getline(ss, token, ',');
            if (token.front() == '"' && token.back() == '"') {
                token = token.substr(1, token.length() - 2);
            }
            record.name = token;
            
            // 解析数值
            std::getline(ss, token, ',');
            record.value = std::stod(token);
            
            // 解析时间戳（简化实现）
            std::getline(ss, token);
            record.timestamp = std::chrono::system_clock::now();
            
            records.push_back(record);
        }
        
        printf("从CSV文件导入了 %zu 条记录\n", records.size());
        return records;
    }
    
    void export_to_json(const std::vector<Record>& records, const std::string& filename) {
        fs_libc::ofstream json_file(filename);
        if (!json_file.is_open()) {
            throw std::runtime_error("Cannot create JSON file");
        }
        
        json_file << "{\n";
        json_file << "  \"records\": [\n";
        
        for (size_t i = 0; i < records.size(); ++i) {
            const auto& record = records[i];
            auto time_t = std::chrono::system_clock::to_time_t(record.timestamp);
            
            json_file << "    {\n";
            json_file << "      \"id\": " << record.id << ",\n";
            json_file << "      \"name\": \"" << record.name << "\",\n";
            json_file << "      \"value\": " << std::fixed << std::setprecision(2) 
                     << record.value << ",\n";
            json_file << "      \"timestamp\": " << time_t << "\n";
            json_file << "    }";
            
            if (i < records.size() - 1) {
                json_file << ",";
            }
            json_file << "\n";
        }
        
        json_file << "  ],\n";
        json_file << "  \"count\": " << records.size() << "\n";
        json_file << "}\n";
        
        std::cout << "JSON文件已导出: " << filename << std::endl;
    }
};

// 使用示例
void demo_data_processing() {
    DataProcessor processor;
    
    // 准备测试数据
    std::vector<DataProcessor::Record> records = {
        {1, "Temperature Sensor", 23.5, std::chrono::system_clock::now()},
        {2, "Humidity Sensor", 65.2, std::chrono::system_clock::now()},
        {3, "Pressure Sensor", 1013.25, std::chrono::system_clock::now()}
    };
    
    // 导出到不同格式
    processor.export_to_csv(records, "/tmp/sensor_data.csv");
    processor.export_to_json(records, "/tmp/sensor_data.json");
    
    // 导入并验证
    auto imported_records = processor.import_from_csv("/tmp/sensor_data.csv");
    std::cout << "导入验证: " << imported_records.size() << " 条记录\n";
}
```

### 场景4：文件系统管理工具

```cpp
// 就像文件管理器的功能

class FileManager {
public:
    void analyze_directory(const std::string& path) {
        if (!fs_libc::exists(path)) {
            std::cout << "路径不存在: " << path << std::endl;
            return;
        }
        
        if (!fs_libc::is_directory(path)) {
            std::cout << "不是目录: " << path << std::endl;
            return;
        }
        
        size_t file_count = 0;
        size_t dir_count = 0;
        std::uintmax_t total_size = 0;
        std::map<std::string, size_t> extensions;
        
        try {
            for (fs_libc::directory_iterator it(path); 
                 it != fs_libc::directory_iterator::end(); ++it) {
                
                std::string name = it->d_name;
                if (name == "." || name == "..") continue;
                
                std::string full_path = path + "/" + name;
                
                if (fs_libc::is_directory(full_path)) {
                    dir_count++;
                } else if (fs_libc::is_regular_file(full_path)) {
                    file_count++;
                    auto size = fs_libc::file_size(full_path);
                    total_size += size;
                    
                    // 统计文件扩展名
                    auto dot_pos = name.find_last_of('.');
                    if (dot_pos != std::string::npos) {
                        std::string ext = name.substr(dot_pos);
                        extensions[ext]++;
                    } else {
                        extensions["(无扩展名)"]++;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "分析目录时出错: " << e.what() << std::endl;
            return;
        }
        
        // 输出分析结果
        std::cout << "\n=== 目录分析: " << path << " ===\n";
        std::cout << "📁 目录数量: " << dir_count << std::endl;
        std::cout << "📄 文件数量: " << file_count << std::endl;
        std::cout << "💾 总大小: " << format_size(total_size) << std::endl;
        
        if (!extensions.empty()) {
            std::cout << "\n文件类型分布:\n";
            for (const auto& [ext, count] : extensions) {
                std::cout << "  " << ext << ": " << count << " 个文件\n";
            }
        }
    }
    
    void backup_directory(const std::string& source, const std::string& backup_dir) {
        // 创建备份目录
        std::string timestamp = get_timestamp();
        std::string backup_path = backup_dir + "/backup_" + timestamp;
        
        if (!fs_libc::create_directories(backup_path)) {
            throw std::runtime_error("无法创建备份目录");
        }
        
        std::cout << "开始备份: " << source << " -> " << backup_path << std::endl;
        
        size_t files_copied = 0;
        std::uintmax_t bytes_copied = 0;
        
        backup_recursive(source, backup_path, files_copied, bytes_copied);
        
        std::cout << "备份完成!\n";
        std::cout << "复制文件: " << files_copied << " 个\n";
        std::cout << "复制数据: " << format_size(bytes_copied) << std::endl;
    }
    
    void cleanup_temp_files(const std::string& path) {
        std::vector<std::string> temp_patterns = {
            ".tmp", ".temp", ".bak", ".old", "~"
        };
        
        size_t deleted_count = 0;
        std::uintmax_t freed_space = 0;
        
        try {
            for (fs_libc::directory_iterator it(path); 
                 it != fs_libc::directory_iterator::end(); ++it) {
                
                std::string name = it->d_name;
                if (name == "." || name == "..") continue;
                
                std::string full_path = path + "/" + name;
                
                if (fs_libc::is_regular_file(full_path)) {
                    bool is_temp = false;
                    
                    // 检查是否为临时文件
                    for (const auto& pattern : temp_patterns) {
                        if (name.ends_with(pattern)) {
                            is_temp = true;
                            break;
                        }
                    }
                    
                    if (is_temp) {
                        auto size = fs_libc::file_size(full_path);
                        if (fs_libc::remove(full_path)) {
                            deleted_count++;
                            freed_space += size;
                            std::cout << "删除临时文件: " << name << std::endl;
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "清理临时文件时出错: " << e.what() << std::endl;
            return;
        }
        
        std::cout << "\n清理完成!\n";
        std::cout << "删除文件: " << deleted_count << " 个\n";
        std::cout << "释放空间: " << format_size(freed_space) << std::endl;
    }
    
private:
    std::string format_size(std::uintmax_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unit = 0;
        double size = bytes;
        
        while (size >= 1024 && unit < 4) {
            size /= 1024;
            unit++;
        }
        
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << size << " " << units[unit];
        return oss.str();
    }
    
    std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
        return oss.str();
    }
    
    void backup_recursive(const std::string& src, const std::string& dst, 
                         size_t& files_copied, std::uintmax_t& bytes_copied) {
        // 递归备份实现（简化版）
        try {
            for (fs_libc::directory_iterator it(src); 
                 it != fs_libc::directory_iterator::end(); ++it) {
                
                std::string name = it->d_name;
                if (name == "." || name == "..") continue;
                
                std::string src_path = src + "/" + name;
                std::string dst_path = dst + "/" + name;
                
                if (fs_libc::is_directory(src_path)) {
                    fs_libc::create_directory(dst_path);
                    backup_recursive(src_path, dst_path, files_copied, bytes_copied);
                } else if (fs_libc::is_regular_file(src_path)) {
                    fs_libc::copy_file(src_path, dst_path);
                    files_copied++;
                    bytes_copied += fs_libc::file_size(src_path);
                    
                    if (files_copied % 100 == 0) {
                        std::cout << "已复制 " << files_copied << " 个文件..." << std::endl;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "备份时出错: " << e.what() << std::endl;
        }
    }
};
```

## LibC接口的设计优势

1. **易用性**：提供程序员熟悉的标准接口，降低学习成本
2. **缓冲优化**：内置缓冲机制提高I/O性能
3. **格式化支持**：printf系列函数提供强大的格式化功能
4. **C++集成**：现代C++流接口提供类型安全和异常处理
5. **跨平台兼容**：标准POSIX接口保证代码可移植性

LibC接口就像文件系统的"前台接待"，让每个程序员都能轻松、愉快地使用文件系统服务！