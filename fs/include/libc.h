#ifndef LIBC_H
#define LIBC_H

#include "syscall.h"
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// libc库文件操作函数
#ifdef __cplusplus
extern "C" {
#endif

// 基础文件操作
int open(const char* pathname, int flags, ...);
int close(int fd);
ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
off_t lseek(int fd, off_t offset, int whence);
int fsync(int fd);
int ftruncate(int fd, off_t length);

// 目录操作
int mkdir(const char* pathname, mode_t mode);
int rmdir(const char* pathname);
DIR* opendir(const char* name);
struct dirent* readdir(DIR* dirp);
int closedir(DIR* dirp);
void rewinddir(DIR* dirp);
long telldir(DIR* dirp);
void seekdir(DIR* dirp, long loc);

// 文件管理
int unlink(const char* pathname);
int rename(const char* oldpath, const char* newpath);
int link(const char* oldpath, const char* newpath);
int symlink(const char* target, const char* linkpath);
ssize_t readlink(const char* pathname, char* buf, size_t bufsiz);

// 文件属性
int stat(const char* pathname, struct stat* statbuf);
int lstat(const char* pathname, struct stat* statbuf);
int fstat(int fd, struct stat* statbuf);
int chmod(const char* pathname, mode_t mode);
int fchmod(int fd, mode_t mode);
int chown(const char* pathname, uid_t owner, gid_t group);
int fchown(int fd, uid_t owner, gid_t group);
int lchown(const char* pathname, uid_t owner, gid_t group);

// 文件描述符操作
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int fcntl(int fd, int cmd, ... /* arg */ );

// 工作目录
int chdir(const char* path);
int fchdir(int fd);
char* getcwd(char* buf, size_t size);

// 挂载操作
int mount(const char* source, const char* target,
          const char* filesystemtype, unsigned long mountflags,
          const void* data);
int umount(const char* target);
int umount2(const char* target, int flags);

// 文件系统信息
int statfs(const char* path, struct statfs* buf);
int fstatfs(int fd, struct statfs* buf);

// 同步操作
void sync(void);
int syncfs(int fd);

// 访问权限
int access(const char* pathname, int mode);

// 扩展属性
ssize_t getxattr(const char* path, const char* name, void* value, size_t size);
ssize_t lgetxattr(const char* path, const char* name, void* value, size_t size);
ssize_t fgetxattr(int fd, const char* name, void* value, size_t size);
int setxattr(const char* path, const char* name, const void* value, size_t size, int flags);
int lsetxattr(const char* path, const char* name, const void* value, size_t size, int flags);
int fsetxattr(int fd, const char* name, const void* value, size_t size, int flags);
ssize_t listxattr(const char* path, char* list, size_t size);
ssize_t llistxattr(const char* path, char* list, size_t size);
ssize_t flistxattr(int fd, char* list, size_t size);
int removexattr(const char* path, const char* name);
int lremovexattr(const char* path, const char* name);
int fremovexattr(int fd, const char* name);

// FILE*流操作(基于文件描述符实现)
FILE* fopen(const char* pathname, const char* mode);
FILE* fdopen(int fd, const char* mode);
FILE* freopen(const char* pathname, const char* mode, FILE* stream);
int fclose(FILE* stream);
int fflush(FILE* stream);

// 流读写操作
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int fgetc(FILE* stream);
int fputc(int c, FILE* stream);
char* fgets(char* s, int size, FILE* stream);
int fputs(const char* s, FILE* stream);
int fprintf(FILE* stream, const char* format, ...);
int fscanf(FILE* stream, const char* format, ...);

// 流定位操作
int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);
void rewind(FILE* stream);
int fgetpos(FILE* stream, fpos_t* pos);
int fsetpos(FILE* stream, const fpos_t* pos);

// 流状态检查
int feof(FILE* stream);
int ferror(FILE* stream);
void clearerr(FILE* stream);
int fileno(FILE* stream);

// 缓冲控制
int setvbuf(FILE* stream, char* buf, int mode, size_t size);
void setbuf(FILE* stream, char* buf);

// 临时文件
FILE* tmpfile(void);
char* tmpnam(char* s);
char* tempnam(const char* dir, const char* pfx);
int mkstemp(char* template_str);
char* mkdtemp(char* template_str);

// 内存映射文件(简化实现)
void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void* addr, size_t length);
int msync(void* addr, size_t length, int flags);

#ifdef __cplusplus
}
#endif

// C++流包装类
#ifdef __cplusplus
#include <iostream>
#include <fstream>
#include <sstream>

namespace fs_libc {

// 文件流缓冲区类
class FileBuf : public std::streambuf {
private:
    int fd_;
    char* buffer_;
    size_t buffer_size_;
    bool owns_fd_;

public:
    FileBuf(int fd, bool owns_fd = false);
    FileBuf(const char* filename, std::ios_base::openmode mode);
    ~FileBuf();
    
    bool is_open() const { return fd_ >= 0; }
    int get_fd() const { return fd_; }
    
protected:
    // 输入操作
    int_type underflow() override;
    int_type uflow() override;
    
    // 输出操作
    int_type overflow(int_type c = traits_type::eof()) override;
    int sync() override;
    
    // 定位操作
    pos_type seekoff(off_type off, std::ios_base::seekdir way,
                     std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;
    pos_type seekpos(pos_type sp,
                     std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;
};

// 文件输入流
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

// 文件输出流
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

// 文件输入输出流
class fstream : public std::iostream {
private:
    FileBuf buf_;

public:
    fstream() : std::iostream(&buf_), buf_(-1) {}
    explicit fstream(const char* filename, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
        : std::iostream(&buf_), buf_(filename, mode) {
        if (!buf_.is_open()) {
            setstate(std::ios_base::failbit);
        }
    }
    
    bool is_open() const { return buf_.is_open(); }
    void open(const char* filename, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out);
    void close();
};

// 目录迭代器
class directory_iterator {
private:
    DIR* dir_;
    struct dirent* current_;
    std::string path_;

public:
    directory_iterator() : dir_(nullptr), current_(nullptr) {}
    explicit directory_iterator(const std::string& path);
    ~directory_iterator();
    
    directory_iterator(const directory_iterator&) = delete;
    directory_iterator& operator=(const directory_iterator&) = delete;
    
    directory_iterator(directory_iterator&& other) noexcept;
    directory_iterator& operator=(directory_iterator&& other) noexcept;
    
    const struct dirent& operator*() const { return *current_; }
    const struct dirent* operator->() const { return current_; }
    
    directory_iterator& operator++();
    directory_iterator operator++(int);
    
    bool operator==(const directory_iterator& other) const;
    bool operator!=(const directory_iterator& other) const;
    
    // 迭代器结束标志
    static directory_iterator end() { return directory_iterator(); }
};

// 文件状态信息类
class file_status {
private:
    struct stat st_;
    bool valid_;

public:
    file_status() : valid_(false) {}
    explicit file_status(const struct stat& st) : st_(st), valid_(true) {}
    
    bool is_valid() const { return valid_; }
    const struct stat& get_stat() const { return st_; }
    
    bool is_regular_file() const { return valid_ && S_ISREG(st_.st_mode); }
    bool is_directory() const { return valid_ && S_ISDIR(st_.st_mode); }
    bool is_symlink() const { return valid_ && S_ISLNK(st_.st_mode); }
    bool is_block_file() const { return valid_ && S_ISBLK(st_.st_mode); }
    bool is_character_file() const { return valid_ && S_ISCHR(st_.st_mode); }
    bool is_fifo() const { return valid_ && S_ISFIFO(st_.st_mode); }
    bool is_socket() const { return valid_ && S_ISSOCK(st_.st_mode); }
    
    std::uintmax_t file_size() const { return valid_ ? st_.st_size : 0; }
    std::time_t last_write_time() const { return valid_ ? st_.st_mtime : 0; }
};

// 文件系统操作函数
file_status status(const std::string& path);
file_status symlink_status(const std::string& path);
bool exists(const std::string& path);
bool is_regular_file(const std::string& path);
bool is_directory(const std::string& path);
bool is_symlink(const std::string& path);
std::uintmax_t file_size(const std::string& path);

bool create_directory(const std::string& path);
bool create_directories(const std::string& path);
bool remove(const std::string& path);
std::uintmax_t remove_all(const std::string& path);
void rename(const std::string& old_path, const std::string& new_path);
void copy_file(const std::string& from, const std::string& to);

std::string current_path();
void current_path(const std::string& path);
std::string absolute(const std::string& path);
std::string canonical(const std::string& path);

} // namespace fs_libc

#endif // __cplusplus

#endif // LIBC_H