#ifndef VFS_H
#define VFS_H

#include "types.h"
#include "block_device.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>

// 前向声明
class Inode;
class Dentry;
class File;
class SuperBlock;
class FileSystem;
class VfsMount;

// 文件属性结构
struct FileAttribute {
    FileMode mode;              // 文件权限和类型
    u32 uid;                   // 用户ID
    u32 gid;                   // 组ID
    u64 size;                  // 文件大小
    u64 blocks;                // 占用的块数
    std::chrono::system_clock::time_point atime;  // 访问时间
    std::chrono::system_clock::time_point mtime;  // 修改时间
    std::chrono::system_clock::time_point ctime;  // 状态改变时间
    u32 nlink;                 // 硬链接数
    u32 blksize;               // 块大小
    
    FileAttribute();
};

// 目录项结构
struct DirentEntry {
    inode_t ino;                        // inode号
    std::string name;                   // 文件名
    FileType type;                      // 文件类型
    
    DirentEntry(inode_t i, const std::string& n, FileType t)
        : ino(i), name(n), type(t) {}
};

// Inode操作接口
class InodeOperations {
public:
    virtual ~InodeOperations() = default;
    
    // 文件内容操作
    virtual Result<size_t> read(SharedPtr<Inode> inode, offset_t pos, 
                               void* buffer, size_t size) = 0;
    virtual Result<size_t> write(SharedPtr<Inode> inode, offset_t pos, 
                                const void* buffer, size_t size) = 0;
    
    // 目录操作
    virtual Result<std::vector<DirentEntry>> readdir(SharedPtr<Inode> inode) = 0;
    virtual Result<SharedPtr<Inode>> lookup(SharedPtr<Inode> dir, 
                                           const std::string& name) = 0;
    virtual Result<SharedPtr<Inode>> create(SharedPtr<Inode> dir, 
                                           const std::string& name, 
                                           FileMode mode) = 0;
    virtual Result<void> unlink(SharedPtr<Inode> dir, const std::string& name) = 0;
    virtual Result<void> mkdir(SharedPtr<Inode> dir, const std::string& name, 
                              FileMode mode) = 0;
    virtual Result<void> rmdir(SharedPtr<Inode> dir, const std::string& name) = 0;
    virtual Result<void> rename(SharedPtr<Inode> old_dir, const std::string& old_name,
                               SharedPtr<Inode> new_dir, const std::string& new_name) = 0;
    
    // 文件属性操作
    virtual Result<FileAttribute> getattr(SharedPtr<Inode> inode) = 0;
    virtual Result<void> setattr(SharedPtr<Inode> inode, const FileAttribute& attr) = 0;
    
    // 扩展属性操作
    virtual Result<std::string> getxattr(SharedPtr<Inode> inode, const std::string& name) = 0;
    virtual Result<void> setxattr(SharedPtr<Inode> inode, const std::string& name, 
                                 const std::string& value) = 0;
    virtual Result<std::vector<std::string>> listxattr(SharedPtr<Inode> inode) = 0;
    virtual Result<void> removexattr(SharedPtr<Inode> inode, const std::string& name) = 0;
};

// Inode类
class Inode {
private:
    inode_t ino_;                       // inode号
    SharedPtr<SuperBlock> sb_;          // 所属超级块
    SharedPtr<InodeOperations> ops_;    // inode操作
    FileAttribute attr_;                // 文件属性
    mutable std::mutex mutex_;          // 保护并发访问
    std::atomic<u32> ref_count_;        // 引用计数
    
    // 缓存的页面(用于文件内容)
    std::unordered_map<offset_t, SharedPtr<Page>> pages_;

public:
    Inode(inode_t ino, SharedPtr<SuperBlock> sb, SharedPtr<InodeOperations> ops);
    ~Inode();
    
    // 禁止拷贝
    Inode(const Inode&) = delete;
    Inode& operator=(const Inode&) = delete;
    
    // 基本信息
    inode_t get_ino() const { return ino_; }
    SharedPtr<SuperBlock> get_sb() const { return sb_; }
    SharedPtr<BlockDevice> get_block_device() const;
    
    // 引用计数
    void get() { ref_count_++; }
    void put() { if (--ref_count_ == 0) delete this; }
    u32 ref_count() const { return ref_count_.load(); }
    
    // 文件操作
    Result<size_t> read(offset_t pos, void* buffer, size_t size);
    Result<size_t> write(offset_t pos, const void* buffer, size_t size);
    
    // 目录操作
    Result<std::vector<DirentEntry>> readdir();
    Result<SharedPtr<Inode>> lookup(const std::string& name);
    Result<SharedPtr<Inode>> create(const std::string& name, FileMode mode);
    Result<void> unlink(const std::string& name);
    Result<void> mkdir(const std::string& name, FileMode mode);
    Result<void> rmdir(const std::string& name);
    Result<void> rename(const std::string& old_name, SharedPtr<Inode> new_dir, 
                       const std::string& new_name);
    
    // 属性操作
    Result<FileAttribute> getattr() const;
    Result<void> setattr(const FileAttribute& attr);
    
    // 扩展属性
    Result<std::string> getxattr(const std::string& name);
    Result<void> setxattr(const std::string& name, const std::string& value);
    Result<std::vector<std::string>> listxattr();
    Result<void> removexattr(const std::string& name);
    
    // 便捷方法
    bool is_dir() const { return attr_.mode.type() == FileType::DIRECTORY; }
    bool is_reg() const { return attr_.mode.type() == FileType::REGULAR; }
    bool is_symlink() const { return attr_.mode.type() == FileType::SYMLINK; }
    u64 get_size() const { return attr_.size; }
    FileMode get_mode() const { return attr_.mode; }
    
    // 同步到磁盘
    Result<void> sync();
    
    // 截断文件
    Result<void> truncate(u64 size);
};

// 目录项缓存
class Dentry {
private:
    std::string name_;                  // 目录项名称
    SharedPtr<Inode> inode_;            // 关联的inode
    WeakPtr<Dentry> parent_;            // 父目录项
    std::unordered_map<std::string, SharedPtr<Dentry>> children_;  // 子目录项
    mutable std::mutex mutex_;          // 保护并发访问
    std::atomic<u32> ref_count_;        // 引用计数

public:
    Dentry(const std::string& name, SharedPtr<Inode> inode, 
           SharedPtr<Dentry> parent = nullptr);
    ~Dentry();
    
    // 基本信息
    const std::string& get_name() const { return name_; }
    SharedPtr<Inode> get_inode() const { return inode_; }
    SharedPtr<Dentry> get_parent() const { return parent_.lock(); }
    
    // 引用计数
    void get() { ref_count_++; }
    void put() { if (--ref_count_ == 0) delete this; }
    u32 ref_count() const { return ref_count_.load(); }
    
    // 查找子目录项
    SharedPtr<Dentry> lookup_child(const std::string& name);
    
    // 添加子目录项
    void add_child(SharedPtr<Dentry> child);
    
    // 移除子目录项
    void remove_child(const std::string& name);
    
    // 获取完整路径
    std::string get_path() const;
    
    // 列出所有子目录项
    std::vector<SharedPtr<Dentry>> list_children() const;
};

// 超级块操作接口
class SuperBlockOperations {
public:
    virtual ~SuperBlockOperations() = default;
    
    // inode操作
    virtual Result<SharedPtr<Inode>> alloc_inode() = 0;
    virtual Result<void> free_inode(SharedPtr<Inode> inode) = 0;
    virtual Result<SharedPtr<Inode>> read_inode(inode_t ino) = 0;
    virtual Result<void> write_inode(SharedPtr<Inode> inode) = 0;
    
    // 文件系统操作
    virtual Result<void> sync() = 0;
    virtual Result<void> statfs(struct statfs* buf) = 0;
    virtual Result<void> remount(u32 flags) = 0;
};

// 超级块类
class SuperBlock {
private:
    SharedPtr<BlockDevice> device_;     // 块设备
    SharedPtr<SuperBlockOperations> ops_;  // 超级块操作
    SharedPtr<FileSystem> fs_type_;     // 文件系统类型
    u32 flags_;                         // 挂载标志
    std::string device_name_;           // 设备名称
    SharedPtr<Dentry> root_;            // 根目录项
    
    // inode缓存
    std::unordered_map<inode_t, WeakPtr<Inode>> inode_cache_;
    mutable std::mutex inode_cache_mutex_;

public:
    SuperBlock(SharedPtr<BlockDevice> device, SharedPtr<FileSystem> fs_type, 
               u32 flags = 0);
    ~SuperBlock();
    
    // 基本信息
    SharedPtr<BlockDevice> get_device() const { return device_; }
    SharedPtr<FileSystem> get_fs_type() const { return fs_type_; }
    const std::string& get_device_name() const { return device_name_; }
    u32 get_flags() const { return flags_; }
    
    // 根目录
    SharedPtr<Dentry> get_root() const { return root_; }
    void set_root(SharedPtr<Dentry> root) { root_ = root; }
    
    // inode管理
    Result<SharedPtr<Inode>> get_inode(inode_t ino);
    void cache_inode(SharedPtr<Inode> inode);
    void evict_inode(inode_t ino);
    
    // 操作设置
    void set_ops(SharedPtr<SuperBlockOperations> ops) { ops_ = ops; }
    SharedPtr<SuperBlockOperations> get_ops() const { return ops_; }
    
    // 同步
    Result<void> sync();
};

// 文件系统类型接口
class FileSystem {
public:
    virtual ~FileSystem() = default;
    
    // 文件系统名称
    virtual std::string get_name() const = 0;
    
    // 挂载文件系统
    virtual Result<SharedPtr<SuperBlock>> mount(SharedPtr<BlockDevice> device, 
                                               u32 flags, const std::string& options) = 0;
    
    // 卸载文件系统
    virtual Result<void> umount(SharedPtr<SuperBlock> sb) = 0;
    
    // 获取文件系统信息
    virtual Result<void> statfs(SharedPtr<SuperBlock> sb, struct statfs* buf) = 0;
};

// 文件类
class File {
private:
    SharedPtr<Dentry> dentry_;          // 关联的目录项
    u32 flags_;                         // 打开标志
    offset_t pos_;                      // 当前位置
    mutable std::mutex mutex_;          // 保护并发访问
    std::atomic<u32> ref_count_;        // 引用计数

public:
    File(SharedPtr<Dentry> dentry, u32 flags);
    ~File();
    
    // 基本信息
    SharedPtr<Dentry> get_dentry() const { return dentry_; }
    SharedPtr<Inode> get_inode() const { return dentry_->get_inode(); }
    u32 get_flags() const { return flags_; }
    offset_t get_pos() const { std::lock_guard<std::mutex> lock(mutex_); return pos_; }
    
    // 引用计数
    void get() { ref_count_++; }
    void put() { if (--ref_count_ == 0) delete this; }
    u32 ref_count() const { return ref_count_.load(); }
    
    // 文件操作
    Result<size_t> read(void* buffer, size_t size);
    Result<size_t> write(const void* buffer, size_t size);
    Result<offset_t> seek(offset_t offset, int whence);
    Result<void> fsync();
    Result<void> truncate(u64 size);
    
    // 目录操作
    Result<std::vector<DirentEntry>> readdir();
    
    // 属性操作
    Result<FileAttribute> fstat();
};

// VFS挂载点
class VfsMount {
private:
    SharedPtr<SuperBlock> sb_;          // 超级块
    SharedPtr<Dentry> mountpoint_;      // 挂载点
    SharedPtr<Dentry> root_;            // 挂载的根目录
    std::string device_name_;           // 设备名称
    std::string mount_options_;         // 挂载选项
    u32 flags_;                         // 挂载标志

public:
    VfsMount(SharedPtr<SuperBlock> sb, SharedPtr<Dentry> mountpoint, 
             const std::string& device_name, u32 flags = 0);
    
    SharedPtr<SuperBlock> get_sb() const { return sb_; }
    SharedPtr<Dentry> get_mountpoint() const { return mountpoint_; }
    SharedPtr<Dentry> get_root() const { return root_; }
    const std::string& get_device_name() const { return device_name_; }
    u32 get_flags() const { return flags_; }
};

// VFS主类
class VFS {
private:
    std::unordered_map<std::string, SharedPtr<FileSystem>> filesystems_;  // 文件系统类型
    std::vector<SharedPtr<VfsMount>> mounts_;                            // 挂载列表
    SharedPtr<Dentry> root_;                                             // 全局根目录
    mutable std::mutex mutex_;                                           // 保护并发访问
    
    // 路径解析
    Result<SharedPtr<Dentry>> walk_path(const std::string& path, 
                                       SharedPtr<Dentry> base = nullptr);
    Result<SharedPtr<Dentry>> walk_component(SharedPtr<Dentry> dir, 
                                            const std::string& name);

public:
    VFS();
    ~VFS();
    
    // 文件系统注册
    void register_filesystem(SharedPtr<FileSystem> fs);
    void unregister_filesystem(const std::string& name);
    SharedPtr<FileSystem> get_filesystem(const std::string& name);
    
    // 挂载/卸载
    Result<void> mount(const std::string& device, const std::string& mountpoint, 
                      const std::string& fstype, u32 flags = 0, 
                      const std::string& options = "");
    Result<void> umount(const std::string& mountpoint);
    
    // 文件操作
    Result<SharedPtr<File>> open(const std::string& path, u32 flags, FileMode mode = FileMode(0644));
    Result<void> close(SharedPtr<File> file);
    
    // 目录操作
    Result<SharedPtr<Inode>> mkdir(const std::string& path, FileMode mode);
    Result<void> rmdir(const std::string& path);
    Result<void> unlink(const std::string& path);
    Result<void> rename(const std::string& old_path, const std::string& new_path);
    
    // 路径操作
    Result<SharedPtr<Dentry>> lookup(const std::string& path);
    Result<std::string> readlink(const std::string& path);
    Result<void> symlink(const std::string& target, const std::string& linkpath);
    
    // 属性操作
    Result<FileAttribute> stat(const std::string& path);
    Result<FileAttribute> lstat(const std::string& path);
    Result<void> chmod(const std::string& path, FileMode mode);
    Result<void> chown(const std::string& path, u32 uid, u32 gid);
    
    // 全局操作
    Result<void> sync();
    std::vector<SharedPtr<VfsMount>> get_mounts() const;
    
    // 设置根目录
    void set_root(SharedPtr<Dentry> root) { root_ = root; }
    SharedPtr<Dentry> get_root() const { return root_; }
};

// 全局VFS实例
extern VFS g_vfs;

#endif // VFS_H