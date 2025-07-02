#include "../../include/vfs.h"

// 全局VFS实例
VFS g_vfs;

// VfsMount实现
VfsMount::VfsMount(SharedPtr<SuperBlock> sb, SharedPtr<Dentry> mountpoint, 
                   const std::string& device_name, u32 flags)
    : sb_(sb), mountpoint_(mountpoint), device_name_(device_name), flags_(flags) {
    root_ = sb->get_root();
}

// VFS实现
VFS::VFS() {
}

VFS::~VFS() {
}

void VFS::register_filesystem(SharedPtr<FileSystem> fs) {
    std::lock_guard<std::mutex> lock(mutex_);
    filesystems_[fs->get_name()] = fs;
}

void VFS::unregister_filesystem(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    filesystems_.erase(name);
}

SharedPtr<FileSystem> VFS::get_filesystem(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto iter = filesystems_.find(name);
    return (iter != filesystems_.end()) ? iter->second : nullptr;
}

Result<void> VFS::mount(const std::string& device, const std::string& mountpoint, 
                       const std::string& fstype, u32 flags, const std::string& options) {
    // Minimal stub implementation
    (void)device; (void)mountpoint; (void)fstype; (void)flags; (void)options;
    return ErrorCode::FS_EIO; // Not implemented
}

Result<void> VFS::umount(const std::string& mountpoint) {
    // Minimal stub implementation
    (void)mountpoint;
    return ErrorCode::FS_EIO; // Not implemented
}

Result<SharedPtr<File>> VFS::open(const std::string& path, u32 flags, FileMode mode) {
    // Minimal stub implementation
    (void)path; (void)flags; (void)mode;
    return ErrorCode::FS_EIO; // Not implemented
}

Result<void> VFS::close(SharedPtr<File> file) {
    // Minimal stub implementation
    (void)file;
    return Result<void>();
}

Result<SharedPtr<Inode>> VFS::mkdir(const std::string& path, FileMode mode) {
    // Minimal stub implementation
    (void)path; (void)mode;
    return ErrorCode::FS_EIO; // Not implemented
}

Result<void> VFS::rmdir(const std::string& path) {
    // Minimal stub implementation
    (void)path;
    return ErrorCode::FS_EIO; // Not implemented
}

Result<void> VFS::unlink(const std::string& path) {
    // Minimal stub implementation
    (void)path;
    return ErrorCode::FS_EIO; // Not implemented
}

Result<void> VFS::rename(const std::string& old_path, const std::string& new_path) {
    // Minimal stub implementation
    (void)old_path; (void)new_path;
    return ErrorCode::FS_EIO; // Not implemented
}

Result<SharedPtr<Dentry>> VFS::lookup(const std::string& path) {
    // Minimal stub implementation
    (void)path;
    return ErrorCode::FS_EIO; // Not implemented
}

Result<std::string> VFS::readlink(const std::string& path) {
    // Minimal stub implementation
    (void)path;
    return ErrorCode::FS_EIO; // Not implemented
}

Result<void> VFS::symlink(const std::string& target, const std::string& linkpath) {
    // Minimal stub implementation
    (void)target; (void)linkpath;
    return ErrorCode::FS_EIO; // Not implemented
}

Result<FileAttribute> VFS::stat(const std::string& path) {
    // Minimal stub implementation
    (void)path;
    return ErrorCode::FS_EIO; // Not implemented
}

Result<FileAttribute> VFS::lstat(const std::string& path) {
    // Minimal stub implementation
    (void)path;
    return ErrorCode::FS_EIO; // Not implemented
}

Result<void> VFS::chmod(const std::string& path, FileMode mode) {
    // Minimal stub implementation
    (void)path; (void)mode;
    return ErrorCode::FS_EIO; // Not implemented
}

Result<void> VFS::chown(const std::string& path, u32 uid, u32 gid) {
    // Minimal stub implementation
    (void)path; (void)uid; (void)gid;
    return ErrorCode::FS_EIO; // Not implemented
}

Result<void> VFS::sync() {
    // Minimal stub implementation
    return Result<void>();
}

std::vector<SharedPtr<VfsMount>> VFS::get_mounts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mounts_;
}

Result<SharedPtr<Dentry>> VFS::walk_path(const std::string& path, SharedPtr<Dentry> base) {
    // Minimal stub implementation
    (void)path; (void)base;
    return ErrorCode::FS_EIO; // Not implemented
}

Result<SharedPtr<Dentry>> VFS::walk_component(SharedPtr<Dentry> dir, const std::string& name) {
    // Minimal stub implementation
    (void)dir; (void)name;
    return ErrorCode::FS_EIO; // Not implemented
}