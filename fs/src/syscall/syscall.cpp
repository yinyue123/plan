#include "../../include/syscall.h"
#include <cstring>
#include <cerrno>
#include <thread>

// 全局线程本地存储
thread_local ProcessFsContext* SystemCall::current_process_ = nullptr;

// SystemCall实现
void SystemCall::init() {
    // 初始化全局VFS
    // g_vfs已经在vfs.cpp中定义
}

void SystemCall::set_current_process(ProcessFsContext* ctx) {
    current_process_ = ctx;
}

ProcessFsContext* SystemCall::get_current_process() {
    return current_process_;
}

Result<SharedPtr<Dentry>> SystemCall::resolve_path(const std::string& path, bool follow_symlinks) {
    (void)follow_symlinks;
    return g_vfs.lookup(path);
}

Result<SharedPtr<Dentry>> SystemCall::resolve_parent_path(const std::string& path, std::string& filename) {
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        filename = path;
        auto ctx = get_current_process();
        return ctx ? Result<SharedPtr<Dentry>>(ctx->cwd) : Result<SharedPtr<Dentry>>(ErrorCode::FS_EIO);
    }
    
    filename = path.substr(pos + 1);
    std::string parent_path = path.substr(0, pos);
    if (parent_path.empty()) parent_path = "/";
    
    return resolve_path(parent_path);
}

// 文件操作系统调用实现
int SystemCall::sys_open(const char* pathname, int flags, mode_t mode) {
    auto ctx = get_current_process();
    if (!ctx) return -1;
    
    auto file_result = g_vfs.open(pathname, flags, FileMode(mode));
    if (file_result.is_err()) {
        set_errno(error_to_errno(file_result.error()));
        return -1;
    }
    
    auto fd_result = ctx->fd_table.alloc_fd(file_result.unwrap());
    if (fd_result.is_err()) {
        set_errno(error_to_errno(fd_result.error()));
        return -1;
    }
    
    return fd_result.unwrap();
}

int SystemCall::sys_close(int fd) {
    auto ctx = get_current_process();
    if (!ctx) return -1;
    
    auto result = ctx->fd_table.free_fd(fd);
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    
    return 0;
}

ssize_t SystemCall::sys_read(int fd, void* buf, size_t count) {
    auto ctx = get_current_process();
    if (!ctx) return -1;
    
    auto file = ctx->fd_table.get_file(fd);
    if (!file) {
        set_errno(9); // EBADF
        return -1;
    }
    
    auto result = file->read(buf, count);
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    
    return static_cast<ssize_t>(result.unwrap());
}

ssize_t SystemCall::sys_write(int fd, const void* buf, size_t count) {
    auto ctx = get_current_process();
    if (!ctx) return -1;
    
    auto file = ctx->fd_table.get_file(fd);
    if (!file) {
        set_errno(9); // EBADF
        return -1;
    }
    
    auto result = file->write(buf, count);
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    
    return static_cast<ssize_t>(result.unwrap());
}

off_t SystemCall::sys_lseek(int fd, off_t offset, int whence) {
    auto ctx = get_current_process();
    if (!ctx) return -1;
    
    auto file = ctx->fd_table.get_file(fd);
    if (!file) {
        set_errno(9); // EBADF
        return -1;
    }
    
    auto result = file->seek(offset, whence);
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    
    return result.unwrap();
}

int SystemCall::sys_fsync(int fd) {
    auto ctx = get_current_process();
    if (!ctx) return -1;
    
    auto file = ctx->fd_table.get_file(fd);
    if (!file) {
        set_errno(9); // EBADF
        return -1;
    }
    
    auto result = file->fsync();
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    
    return 0;
}

int SystemCall::sys_ftruncate(int fd, off_t length) {
    auto ctx = get_current_process();
    if (!ctx) return -1;
    
    auto file = ctx->fd_table.get_file(fd);
    if (!file) {
        set_errno(9); // EBADF
        return -1;
    }
    
    auto result = file->truncate(static_cast<u64>(length));
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    
    return 0;
}

// 目录操作系统调用实现
int SystemCall::sys_mkdir(const char* pathname, mode_t mode) {
    auto result = g_vfs.mkdir(pathname, FileMode(mode));
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    return 0;
}

int SystemCall::sys_rmdir(const char* pathname) {
    auto result = g_vfs.rmdir(pathname);
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    return 0;
}

int SystemCall::sys_opendir(const char* pathname) {
    return sys_open(pathname, 0, 0);
}

int SystemCall::sys_readdir(int fd, struct dirent* entry) {
    auto ctx = get_current_process();
    if (!ctx) return -1;
    
    auto file = ctx->fd_table.get_file(fd);
    if (!file) {
        set_errno(9); // EBADF
        return -1;
    }
    
    auto result = file->readdir();
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    
    // 简化实现：返回第一个条目
    auto entries = result.unwrap();
    if (entries.empty()) {
        return 0; // EOF
    }
    
    entry->d_ino = entries[0].ino;
    std::strncpy(entry->d_name, entries[0].name.c_str(), sizeof(entry->d_name) - 1);
    entry->d_name[sizeof(entry->d_name) - 1] = '\0';
    
    return 1;
}

int SystemCall::sys_closedir(int fd) {
    return sys_close(fd);
}

// 其他系统调用实现
int SystemCall::sys_unlink(const char* pathname) {
    auto result = g_vfs.unlink(pathname);
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    return 0;
}

int SystemCall::sys_rename(const char* oldpath, const char* newpath) {
    auto result = g_vfs.rename(oldpath, newpath);
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    return 0;
}

int SystemCall::sys_link(const char* oldpath, const char* newpath) {
    (void)oldpath; (void)newpath;
    set_errno(38); // ENOSYS
    return -1;
}

int SystemCall::sys_symlink(const char* target, const char* linkpath) {
    auto result = g_vfs.symlink(target, linkpath);
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    return 0;
}

ssize_t SystemCall::sys_readlink(const char* pathname, char* buf, size_t bufsiz) {
    auto result = g_vfs.readlink(pathname);
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    
    auto target = result.unwrap();
    size_t len = std::min(target.length(), bufsiz - 1);
    std::memcpy(buf, target.c_str(), len);
    buf[len] = '\0';
    
    return static_cast<ssize_t>(len);
}

// 文件属性系统调用
int SystemCall::sys_stat(const char* pathname, struct stat* statbuf) {
    auto result = g_vfs.stat(pathname);
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    
    auto attr = result.unwrap();
    std::memset(statbuf, 0, sizeof(*statbuf));
    statbuf->st_mode = attr.mode.mode;
    statbuf->st_size = static_cast<off_t>(attr.size);
    statbuf->st_uid = attr.uid;
    statbuf->st_gid = attr.gid;
    statbuf->st_nlink = attr.nlink;
    
    return 0;
}

int SystemCall::sys_lstat(const char* pathname, struct stat* statbuf) {
    return sys_stat(pathname, statbuf); // 简化实现
}

int SystemCall::sys_fstat(int fd, struct stat* statbuf) {
    auto ctx = get_current_process();
    if (!ctx) return -1;
    
    auto file = ctx->fd_table.get_file(fd);
    if (!file) {
        set_errno(9); // EBADF
        return -1;
    }
    
    auto result = file->fstat();
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    
    auto attr = result.unwrap();
    std::memset(statbuf, 0, sizeof(*statbuf));
    statbuf->st_mode = attr.mode.mode;
    statbuf->st_size = static_cast<off_t>(attr.size);
    statbuf->st_uid = attr.uid;
    statbuf->st_gid = attr.gid;
    statbuf->st_nlink = attr.nlink;
    
    return 0;
}

int SystemCall::sys_chmod(const char* pathname, mode_t mode) {
    auto result = g_vfs.chmod(pathname, FileMode(mode));
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    return 0;
}

int SystemCall::sys_fchmod(int fd, mode_t mode) {
    (void)fd; (void)mode;
    set_errno(38); // ENOSYS
    return -1;
}

int SystemCall::sys_chown(const char* pathname, uid_t owner, gid_t group) {
    auto result = g_vfs.chown(pathname, owner, group);
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    return 0;
}

int SystemCall::sys_fchown(int fd, uid_t owner, gid_t group) {
    (void)fd; (void)owner; (void)group;
    set_errno(38); // ENOSYS
    return -1;
}

int SystemCall::sys_lchown(const char* pathname, uid_t owner, gid_t group) {
    return sys_chown(pathname, owner, group); // 简化实现
}

// 文件描述符操作
int SystemCall::sys_dup(int fd) {
    auto ctx = get_current_process();
    if (!ctx) return -1;
    
    auto result = ctx->fd_table.dup_fd(fd);
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    
    return result.unwrap();
}

int SystemCall::sys_dup2(int oldfd, int newfd) {
    auto ctx = get_current_process();
    if (!ctx) return -1;
    
    auto result = ctx->fd_table.dup2_fd(oldfd, newfd);
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    
    return result.unwrap();
}

int SystemCall::sys_fcntl(int fd, int cmd, ... /* arg */) {
    (void)fd; (void)cmd;
    set_errno(38); // ENOSYS
    return -1;
}

// 工作目录操作
int SystemCall::sys_chdir(const char* path) {
    auto ctx = get_current_process();
    if (!ctx) return -1;
    
    auto result = resolve_path(path);
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    
    ctx->cwd = result.unwrap();
    return 0;
}

int SystemCall::sys_fchdir(int fd) {
    (void)fd;
    set_errno(38); // ENOSYS
    return -1;
}

char* SystemCall::sys_getcwd(char* buf, size_t size) {
    auto ctx = get_current_process();
    if (!ctx || !ctx->cwd) {
        set_errno(2); // ENOENT
        return nullptr;
    }
    
    std::string path = ctx->cwd->get_path();
    if (path.length() >= size) {
        set_errno(34); // ERANGE
        return nullptr;
    }
    
    std::strcpy(buf, path.c_str());
    return buf;
}

// 挂载系统调用
int SystemCall::sys_mount(const char* source, const char* target,
                         const char* filesystemtype, unsigned long mountflags,
                         const void* data) {
    auto result = g_vfs.mount(source, target, filesystemtype, 
                             static_cast<u32>(mountflags), 
                             data ? static_cast<const char*>(data) : "");
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    return 0;
}

int SystemCall::sys_umount(const char* target) {
    auto result = g_vfs.umount(target);
    if (result.is_err()) {
        set_errno(error_to_errno(result.error()));
        return -1;
    }
    return 0;
}

int SystemCall::sys_umount2(const char* target, int flags) {
    (void)flags;
    return sys_umount(target);
}

// 文件系统信息
int SystemCall::sys_statfs(const char* path, struct statfs* buf) {
    (void)path; (void)buf;
    set_errno(38); // ENOSYS
    return -1;
}

int SystemCall::sys_fstatfs(int fd, struct statfs* buf) {
    (void)fd; (void)buf;
    set_errno(38); // ENOSYS
    return -1;
}

int SystemCall::sys_statvfs(const char* path, struct statvfs* buf) {
    (void)path; (void)buf;
    set_errno(38); // ENOSYS
    return -1;
}

int SystemCall::sys_fstatvfs(int fd, struct statvfs* buf) {
    (void)fd; (void)buf;
    set_errno(38); // ENOSYS
    return -1;
}

// 同步操作
void SystemCall::sys_sync() {
    g_vfs.sync();
}

int SystemCall::sys_syncfs(int fd) {
    (void)fd;
    sys_sync();
    return 0;
}

// 访问权限检查
int SystemCall::sys_access(const char* pathname, int mode) {
    (void)pathname; (void)mode;
    return 0; // 简化实现：总是允许访问
}

int SystemCall::sys_faccessat(int dirfd, const char* pathname, int mode, int flags) {
    (void)dirfd; (void)flags;
    return sys_access(pathname, mode);
}

// 扩展属性（简化实现）
ssize_t SystemCall::sys_getxattr(const char* path, const char* name, void* value, size_t size) {
    (void)path; (void)name; (void)value; (void)size;
    set_errno(95); // ENODATA
    return -1;
}

ssize_t SystemCall::sys_lgetxattr(const char* path, const char* name, void* value, size_t size) {
    return sys_getxattr(path, name, value, size);
}

ssize_t SystemCall::sys_fgetxattr(int fd, const char* name, void* value, size_t size) {
    (void)fd; (void)name; (void)value; (void)size;
    set_errno(95); // ENODATA
    return -1;
}

int SystemCall::sys_setxattr(const char* path, const char* name, const void* value, size_t size, int flags) {
    (void)path; (void)name; (void)value; (void)size; (void)flags;
    set_errno(38); // ENOSYS
    return -1;
}

int SystemCall::sys_lsetxattr(const char* path, const char* name, const void* value, size_t size, int flags) {
    return sys_setxattr(path, name, value, size, flags);
}

int SystemCall::sys_fsetxattr(int fd, const char* name, const void* value, size_t size, int flags) {
    (void)fd; (void)name; (void)value; (void)size; (void)flags;
    set_errno(38); // ENOSYS
    return -1;
}

ssize_t SystemCall::sys_listxattr(const char* path, char* list, size_t size) {
    (void)path; (void)list; (void)size;
    return 0; // 没有扩展属性
}

ssize_t SystemCall::sys_llistxattr(const char* path, char* list, size_t size) {
    return sys_listxattr(path, list, size);
}

ssize_t SystemCall::sys_flistxattr(int fd, char* list, size_t size) {
    (void)fd; (void)list; (void)size;
    return 0; // 没有扩展属性
}

int SystemCall::sys_removexattr(const char* path, const char* name) {
    (void)path; (void)name;
    set_errno(95); // ENODATA
    return -1;
}

int SystemCall::sys_lremovexattr(const char* path, const char* name) {
    return sys_removexattr(path, name);
}

int SystemCall::sys_fremovexattr(int fd, const char* name) {
    (void)fd; (void)name;
    set_errno(95); // ENODATA
    return -1;
}

// 错误处理
void SystemCall::set_errno(int err) {
    errno = err;
}

int SystemCall::get_errno() {
    return errno;
}