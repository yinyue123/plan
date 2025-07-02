#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"
#include "vfs.h"
#include <sys/stat.h>
#include <unistd.h>

// 文件描述符管理
class FileDescriptorTable {
private:
    std::vector<SharedPtr<File>> files_;    // 文件描述符表
    std::mutex mutex_;                      // 保护并发访问
    
public:
    FileDescriptorTable(size_t size = 1024);
    ~FileDescriptorTable();
    
    // 分配文件描述符
    Result<int> alloc_fd(SharedPtr<File> file);
    
    // 释放文件描述符
    Result<void> free_fd(int fd);
    
    // 获取文件对象
    SharedPtr<File> get_file(int fd);
    
    // 复制文件描述符
    Result<int> dup_fd(int fd);
    Result<int> dup2_fd(int oldfd, int newfd);
    
    // 获取下一个可用的文件描述符
    int get_next_fd() const;
    
    // 检查文件描述符是否有效
    bool is_valid_fd(int fd) const;
};

// 进程文件系统上下文
struct ProcessFsContext {
    SharedPtr<Dentry> cwd;                  // 当前工作目录
    SharedPtr<Dentry> root;                 // 根目录
    FileDescriptorTable fd_table;           // 文件描述符表
    FileMode umask;                         // 文件创建掩码
    
    ProcessFsContext();
};

// 系统调用接口类
class SystemCall {
private:
    static thread_local ProcessFsContext* current_process_;
    
    // 内部辅助方法
    static ProcessFsContext* get_current_process();
    static Result<SharedPtr<Dentry>> resolve_path(const std::string& path, bool follow_symlinks = true);
    static Result<SharedPtr<Dentry>> resolve_parent_path(const std::string& path, std::string& filename);

public:
    // 初始化系统调用
    static void init();
    
    // 设置当前进程上下文
    static void set_current_process(ProcessFsContext* ctx);
    
    // 文件操作系统调用
    static int sys_open(const char* pathname, int flags, mode_t mode = 0);
    static int sys_close(int fd);
    static ssize_t sys_read(int fd, void* buf, size_t count);
    static ssize_t sys_write(int fd, const void* buf, size_t count);
    static off_t sys_lseek(int fd, off_t offset, int whence);
    static int sys_fsync(int fd);
    static int sys_ftruncate(int fd, off_t length);
    
    // 目录操作系统调用
    static int sys_mkdir(const char* pathname, mode_t mode);
    static int sys_rmdir(const char* pathname);
    static int sys_opendir(const char* pathname);
    static int sys_readdir(int fd, struct dirent* entry);
    static int sys_closedir(int fd);
    
    // 文件/目录管理系统调用
    static int sys_unlink(const char* pathname);
    static int sys_rename(const char* oldpath, const char* newpath);
    static int sys_link(const char* oldpath, const char* newpath);
    static int sys_symlink(const char* target, const char* linkpath);
    static ssize_t sys_readlink(const char* pathname, char* buf, size_t bufsiz);
    
    // 文件属性系统调用
    static int sys_stat(const char* pathname, struct stat* statbuf);
    static int sys_lstat(const char* pathname, struct stat* statbuf);
    static int sys_fstat(int fd, struct stat* statbuf);
    static int sys_chmod(const char* pathname, mode_t mode);
    static int sys_fchmod(int fd, mode_t mode);
    static int sys_chown(const char* pathname, uid_t owner, gid_t group);
    static int sys_fchown(int fd, uid_t owner, gid_t group);
    static int sys_lchown(const char* pathname, uid_t owner, gid_t group);
    
    // 文件描述符操作系统调用
    static int sys_dup(int fd);
    static int sys_dup2(int oldfd, int newfd);
    static int sys_fcntl(int fd, int cmd, ... /* arg */ );
    
    // 工作目录系统调用
    static int sys_chdir(const char* path);
    static int sys_fchdir(int fd);
    static char* sys_getcwd(char* buf, size_t size);
    
    // 挂载系统调用
    static int sys_mount(const char* source, const char* target,
                        const char* filesystemtype, unsigned long mountflags,
                        const void* data);
    static int sys_umount(const char* target);
    static int sys_umount2(const char* target, int flags);
    
    // 文件系统信息系统调用
    static int sys_statfs(const char* path, struct statfs* buf);
    static int sys_fstatfs(int fd, struct statfs* buf);
    static int sys_statvfs(const char* path, struct statvfs* buf);
    static int sys_fstatvfs(int fd, struct statvfs* buf);
    
    // 同步系统调用
    static void sys_sync();
    static int sys_syncfs(int fd);
    
    // 访问权限检查系统调用
    static int sys_access(const char* pathname, int mode);
    static int sys_faccessat(int dirfd, const char* pathname, int mode, int flags);
    
    // 扩展属性系统调用
    static ssize_t sys_getxattr(const char* path, const char* name, void* value, size_t size);
    static ssize_t sys_lgetxattr(const char* path, const char* name, void* value, size_t size);
    static ssize_t sys_fgetxattr(int fd, const char* name, void* value, size_t size);
    static int sys_setxattr(const char* path, const char* name, const void* value, size_t size, int flags);
    static int sys_lsetxattr(const char* path, const char* name, const void* value, size_t size, int flags);
    static int sys_fsetxattr(int fd, const char* name, const void* value, size_t size, int flags);
    static ssize_t sys_listxattr(const char* path, char* list, size_t size);
    static ssize_t sys_llistxattr(const char* path, char* list, size_t size);
    static ssize_t sys_flistxattr(int fd, char* list, size_t size);
    static int sys_removexattr(const char* path, const char* name);
    static int sys_lremovexattr(const char* path, const char* name);
    static int sys_fremovexattr(int fd, const char* name);
    
    // 错误处理
    static void set_errno(int err);
    static int get_errno();
};

// 系统调用包装宏
#define SYSCALL_ENTER() \
    do { \
        if (!SystemCall::get_current_process()) { \
            SystemCall::set_errno(ENOSYS); \
            return -1; \
        } \
    } while(0)

#define SYSCALL_RETURN(result) \
    do { \
        if ((result).is_err()) { \
            SystemCall::set_errno(static_cast<int>((result).error())); \
            return -1; \
        } \
        return (result).is_ok() ? 0 : -1; \
    } while(0)

#define SYSCALL_RETURN_VALUE(result, value) \
    do { \
        if ((result).is_err()) { \
            SystemCall::set_errno(static_cast<int>((result).error())); \
            return -1; \
        } \
        return (value); \
    } while(0)

// 错误码转换
inline int error_to_errno(ErrorCode err) {
    switch (err) {
        case ErrorCode::SUCCESS: return 0;
        case ErrorCode::ENOENT: return ENOENT;
        case ErrorCode::EIO: return EIO;
        case ErrorCode::ENOMEM: return ENOMEM;
        case ErrorCode::EACCES: return EACCES;
        case ErrorCode::EEXIST: return EEXIST;
        case ErrorCode::ENOTDIR: return ENOTDIR;
        case ErrorCode::EISDIR: return EISDIR;
        case ErrorCode::EINVAL: return EINVAL;
        case ErrorCode::ENOSPC: return ENOSPC;
        case ErrorCode::EROFS: return EROFS;
        default: return EIO;
    }
}

// 模式转换
inline FileMode mode_to_filemode(mode_t mode) {
    return FileMode(mode);
}

inline mode_t filemode_to_mode(FileMode mode) {
    return mode.mode;
}

// 文件属性转换
void fileattr_to_stat(const FileAttribute& attr, struct stat* st);
void stat_to_fileattr(const struct stat* st, FileAttribute& attr);

#endif // SYSCALL_H