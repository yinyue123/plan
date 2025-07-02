#include "../../include/libc.h"
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <sys/mman.h>

// C接口实现（使用SystemCall）
extern "C" {

int open(const char* pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
    }
    return SystemCall::sys_open(pathname, flags, mode);
}

int close(int fd) {
    return SystemCall::sys_close(fd);
}

ssize_t read(int fd, void* buf, size_t count) {
    return SystemCall::sys_read(fd, buf, count);
}

ssize_t write(int fd, const void* buf, size_t count) {
    return SystemCall::sys_write(fd, buf, count);
}

off_t lseek(int fd, off_t offset, int whence) {
    return SystemCall::sys_lseek(fd, offset, whence);
}

int fsync(int fd) {
    return SystemCall::sys_fsync(fd);
}

int ftruncate(int fd, off_t length) {
    return SystemCall::sys_ftruncate(fd, length);
}

int mkdir(const char* pathname, mode_t mode) {
    return SystemCall::sys_mkdir(pathname, mode);
}

int rmdir(const char* pathname) {
    return SystemCall::sys_rmdir(pathname);
}

DIR* opendir(const char* name) {
    int fd = SystemCall::sys_opendir(name);
    if (fd < 0) return nullptr;
    
    DIR* dir = static_cast<DIR*>(malloc(sizeof(DIR)));
    if (!dir) {
        SystemCall::sys_close(fd);
        return nullptr;
    }
    
    // 简化实现：将fd存储在DIR结构中
    *reinterpret_cast<int*>(dir) = fd;
    return dir;
}

struct dirent* readdir(DIR* dirp) {
    if (!dirp) return nullptr;
    
    int fd = *reinterpret_cast<int*>(dirp);
    static struct dirent entry;
    
    int result = SystemCall::sys_readdir(fd, &entry);
    return (result > 0) ? &entry : nullptr;
}

int closedir(DIR* dirp) {
    if (!dirp) return -1;
    
    int fd = *reinterpret_cast<int*>(dirp);
    int result = SystemCall::sys_closedir(fd);
    free(dirp);
    return result;
}

void rewinddir(DIR* dirp) {
    if (!dirp) return;
    int fd = *reinterpret_cast<int*>(dirp);
    SystemCall::sys_lseek(fd, 0, SEEK_SET);
}

long telldir(DIR* dirp) {
    if (!dirp) return -1;
    int fd = *reinterpret_cast<int*>(dirp);
    return SystemCall::sys_lseek(fd, 0, SEEK_CUR);
}

void seekdir(DIR* dirp, long loc) {
    if (!dirp) return;
    int fd = *reinterpret_cast<int*>(dirp);
    SystemCall::sys_lseek(fd, loc, SEEK_SET);
}

int unlink(const char* pathname) {
    return SystemCall::sys_unlink(pathname);
}

int rename(const char* oldpath, const char* newpath) {
    return SystemCall::sys_rename(oldpath, newpath);
}

int link(const char* oldpath, const char* newpath) {
    return SystemCall::sys_link(oldpath, newpath);
}

int symlink(const char* target, const char* linkpath) {
    return SystemCall::sys_symlink(target, linkpath);
}

ssize_t readlink(const char* pathname, char* buf, size_t bufsiz) {
    return SystemCall::sys_readlink(pathname, buf, bufsiz);
}

int stat(const char* pathname, struct stat* statbuf) {
    return SystemCall::sys_stat(pathname, statbuf);
}

int lstat(const char* pathname, struct stat* statbuf) {
    return SystemCall::sys_lstat(pathname, statbuf);
}

int fstat(int fd, struct stat* statbuf) {
    return SystemCall::sys_fstat(fd, statbuf);
}

int chmod(const char* pathname, mode_t mode) {
    return SystemCall::sys_chmod(pathname, mode);
}

int fchmod(int fd, mode_t mode) {
    return SystemCall::sys_fchmod(fd, mode);
}

int chown(const char* pathname, uid_t owner, gid_t group) {
    return SystemCall::sys_chown(pathname, owner, group);
}

int fchown(int fd, uid_t owner, gid_t group) {
    return SystemCall::sys_fchown(fd, owner, group);
}

int lchown(const char* pathname, uid_t owner, gid_t group) {
    return SystemCall::sys_lchown(pathname, owner, group);
}

int dup(int oldfd) {
    return SystemCall::sys_dup(oldfd);
}

int dup2(int oldfd, int newfd) {
    return SystemCall::sys_dup2(oldfd, newfd);
}

int fcntl(int fd, int cmd, ... /* arg */) {
    return SystemCall::sys_fcntl(fd, cmd);
}

int chdir(const char* path) {
    return SystemCall::sys_chdir(path);
}

int fchdir(int fd) {
    return SystemCall::sys_fchdir(fd);
}

char* getcwd(char* buf, size_t size) {
    return SystemCall::sys_getcwd(buf, size);
}

int mount(const char* source, const char* target,
          const char* filesystemtype, unsigned long mountflags,
          const void* data) {
    return SystemCall::sys_mount(source, target, filesystemtype, mountflags, data);
}

int umount(const char* target) {
    return SystemCall::sys_umount(target);
}

int umount2(const char* target, int flags) {
    return SystemCall::sys_umount2(target, flags);
}

int statfs(const char* path, struct statfs* buf) {
    return SystemCall::sys_statfs(path, buf);
}

int fstatfs(int fd, struct statfs* buf) {
    return SystemCall::sys_fstatfs(fd, buf);
}

void sync(void) {
    SystemCall::sys_sync();
}

int syncfs(int fd) {
    return SystemCall::sys_syncfs(fd);
}

int access(const char* pathname, int mode) {
    return SystemCall::sys_access(pathname, mode);
}

ssize_t getxattr(const char* path, const char* name, void* value, size_t size) {
    return SystemCall::sys_getxattr(path, name, value, size);
}

ssize_t lgetxattr(const char* path, const char* name, void* value, size_t size) {
    return SystemCall::sys_lgetxattr(path, name, value, size);
}

ssize_t fgetxattr(int fd, const char* name, void* value, size_t size) {
    return SystemCall::sys_fgetxattr(fd, name, value, size);
}

int setxattr(const char* path, const char* name, const void* value, size_t size, int flags) {
    return SystemCall::sys_setxattr(path, name, value, size, flags);
}

int lsetxattr(const char* path, const char* name, const void* value, size_t size, int flags) {
    return SystemCall::sys_lsetxattr(path, name, value, size, flags);
}

int fsetxattr(int fd, const char* name, const void* value, size_t size, int flags) {
    return SystemCall::sys_fsetxattr(fd, name, value, size, flags);
}

ssize_t listxattr(const char* path, char* list, size_t size) {
    return SystemCall::sys_listxattr(path, list, size);
}

ssize_t llistxattr(const char* path, char* list, size_t size) {
    return SystemCall::sys_llistxattr(path, list, size);
}

ssize_t flistxattr(int fd, char* list, size_t size) {
    return SystemCall::sys_flistxattr(fd, list, size);
}

int removexattr(const char* path, const char* name) {
    return SystemCall::sys_removexattr(path, name);
}

int lremovexattr(const char* path, const char* name) {
    return SystemCall::sys_lremovexattr(path, name);
}

int fremovexattr(int fd, const char* name) {
    return SystemCall::sys_fremovexattr(fd, name);
}

// 简化的FILE*流操作实现
FILE* fopen(const char* pathname, const char* mode) {
    int flags = 0;
    
    if (strchr(mode, 'r')) flags |= O_RDONLY;
    if (strchr(mode, 'w')) flags |= O_WRONLY | O_CREAT | O_TRUNC;
    if (strchr(mode, 'a')) flags |= O_WRONLY | O_CREAT | O_APPEND;
    if (strchr(mode, '+')) flags |= O_RDWR;
    
    int fd = open(pathname, flags, 0644);
    if (fd < 0) return nullptr;
    
    return fdopen(fd, mode);
}

FILE* fdopen(int fd, const char* mode) {
    (void)mode;
    // 简化实现：将fd包装成FILE*
    return reinterpret_cast<FILE*>(static_cast<intptr_t>(fd + 1000)); // 偏移避免与实际指针冲突
}

FILE* freopen(const char* pathname, const char* mode, FILE* stream) {
    if (stream) {
        fclose(stream);
    }
    return fopen(pathname, mode);
}

int fclose(FILE* stream) {
    if (!stream) return EOF;
    
    int fd = static_cast<int>(reinterpret_cast<intptr_t>(stream) - 1000);
    return close(fd);
}

int fflush(FILE* stream) {
    if (!stream) return 0;
    
    int fd = static_cast<int>(reinterpret_cast<intptr_t>(stream) - 1000);
    return fsync(fd);
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (!stream) return 0;
    
    int fd = static_cast<int>(reinterpret_cast<intptr_t>(stream) - 1000);
    ssize_t bytes = read(fd, ptr, size * nmemb);
    return (bytes > 0) ? bytes / size : 0;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (!stream) return 0;
    
    int fd = static_cast<int>(reinterpret_cast<intptr_t>(stream) - 1000);
    ssize_t bytes = write(fd, ptr, size * nmemb);
    return (bytes > 0) ? bytes / size : 0;
}

int fgetc(FILE* stream) {
    char c;
    return (fread(&c, 1, 1, stream) == 1) ? static_cast<unsigned char>(c) : EOF;
}

int fputc(int c, FILE* stream) {
    char ch = static_cast<char>(c);
    return (fwrite(&ch, 1, 1, stream) == 1) ? c : EOF;
}

char* fgets(char* s, int size, FILE* stream) {
    if (!s || size <= 0 || !stream) return nullptr;
    
    int i = 0;
    while (i < size - 1) {
        int c = fgetc(stream);
        if (c == EOF) {
            if (i == 0) return nullptr;
            break;
        }
        s[i++] = static_cast<char>(c);
        if (c == '\n') break;
    }
    s[i] = '\0';
    return s;
}

int fputs(const char* s, FILE* stream) {
    return (fwrite(s, 1, strlen(s), stream) == strlen(s)) ? 0 : EOF;
}

int fprintf(FILE* stream, const char* format, ...) {
    (void)stream; (void)format;
    return 0; // 简化实现
}

int fscanf(FILE* stream, const char* format, ...) {
    (void)stream; (void)format;
    return 0; // 简化实现
}

int fseek(FILE* stream, long offset, int whence) {
    if (!stream) return -1;
    
    int fd = static_cast<int>(reinterpret_cast<intptr_t>(stream) - 1000);
    return (lseek(fd, offset, whence) >= 0) ? 0 : -1;
}

long ftell(FILE* stream) {
    if (!stream) return -1;
    
    int fd = static_cast<int>(reinterpret_cast<intptr_t>(stream) - 1000);
    return lseek(fd, 0, SEEK_CUR);
}

void rewind(FILE* stream) {
    fseek(stream, 0, SEEK_SET);
}

int fgetpos(FILE* stream, fpos_t* pos) {
    if (!pos) return -1;
    long offset = ftell(stream);
    if (offset < 0) return -1;
    *pos = offset;
    return 0;
}

int fsetpos(FILE* stream, const fpos_t* pos) {
    if (!pos) return -1;
    return fseek(stream, *pos, SEEK_SET);
}

int feof(FILE* stream) {
    (void)stream;
    return 0; // 简化实现
}

int ferror(FILE* stream) {
    (void)stream;
    return 0; // 简化实现
}

void clearerr(FILE* stream) {
    (void)stream;
}

int fileno(FILE* stream) {
    if (!stream) return -1;
    return static_cast<int>(reinterpret_cast<intptr_t>(stream) - 1000);
}

int setvbuf(FILE* stream, char* buf, int mode, size_t size) {
    (void)stream; (void)buf; (void)mode; (void)size;
    return 0; // 简化实现
}

void setbuf(FILE* stream, char* buf) {
    setvbuf(stream, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

FILE* tmpfile(void) {
    return fopen("/tmp/tmpfileXXXXXX", "w+");
}

char* tmpnam(char* s) {
    static char buffer[L_tmpnam];
    if (!s) s = buffer;
    strcpy(s, "/tmp/tmpnamXXXXXX");
    return s;
}

char* tempnam(const char* dir, const char* pfx) {
    char* result = static_cast<char*>(malloc(256));
    if (!result) return nullptr;
    
    snprintf(result, 256, "%s/%sXXXXXX", dir ? dir : "/tmp", pfx ? pfx : "tmp");
    return result;
}

int mkstemp(char* template_str) {
    (void)template_str;
    return open("/tmp/tmpXXXXXX", O_RDWR | O_CREAT | O_EXCL, 0600);
}

char* mkdtemp(char* template_str) {
    if (mkdir(template_str, 0700) == 0) {
        return template_str;
    }
    return nullptr;
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    (void)addr; (void)length; (void)prot; (void)flags; (void)fd; (void)offset;
    return MAP_FAILED; // 简化实现：不支持mmap
}

int munmap(void* addr, size_t length) {
    (void)addr; (void)length;
    return -1; // 简化实现
}

int msync(void* addr, size_t length, int flags) {
    (void)addr; (void)length; (void)flags;
    return -1; // 简化实现
}

} // extern "C"