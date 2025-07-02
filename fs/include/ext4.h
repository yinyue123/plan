#ifndef EXT4_H
#define EXT4_H

#include "types.h"
#include "vfs.h"
#include "block_device.h"
#include <array>

// EXT4常量定义
static constexpr u32 EXT4_SUPER_MAGIC = 0xEF53;        // EXT4魔数
static constexpr u32 EXT4_MIN_BLOCK_SIZE = 1024;       // 最小块大小
static constexpr u32 EXT4_MAX_BLOCK_SIZE = 65536;      // 最大块大小
static constexpr u32 EXT4_DEF_BLOCK_SIZE = 4096;       // 默认块大小

static constexpr u32 EXT4_INODE_SIZE = 256;            // Inode大小
static constexpr u32 EXT4_GOOD_OLD_INODE_SIZE = 128;   // 旧版Inode大小
static constexpr u32 EXT4_ROOT_INO = 2;                // 根目录inode号
static constexpr u32 EXT4_FIRST_INO = 11;              // 第一个用户inode号

static constexpr u32 EXT4_NAME_LEN = 255;              // 最大文件名长度
static constexpr u32 EXT4_N_BLOCKS = 15;               // 每个inode的块指针数

// EXT4特性标志
static constexpr u32 EXT4_FEATURE_INCOMPAT_FILETYPE = 0x0002;
static constexpr u32 EXT4_FEATURE_INCOMPAT_EXTENTS = 0x0040;
static constexpr u32 EXT4_FEATURE_INCOMPAT_64BIT = 0x0080;

// EXT4文件类型
enum class Ext4FileType : u8 {
    UNKNOWN = 0,
    REG_FILE = 1,
    DIR = 2,
    CHRDEV = 3,
    BLKDEV = 4,
    FIFO = 5,
    SOCK = 6,
    SYMLINK = 7
};

// EXT4超级块结构
struct Ext4SuperBlock {
    u32 s_inodes_count;         // inode总数
    u32 s_blocks_count_lo;      // 块总数(低32位)
    u32 s_r_blocks_count_lo;    // 保留块数(低32位)
    u32 s_free_blocks_count_lo; // 空闲块数(低32位)
    u32 s_free_inodes_count;    // 空闲inode数
    u32 s_first_data_block;     // 第一个数据块
    u32 s_log_block_size;       // 块大小(log2(块大小/1024))
    u32 s_log_cluster_size;     // 簇大小
    u32 s_blocks_per_group;     // 每个块组的块数
    u32 s_clusters_per_group;   // 每个块组的簇数
    u32 s_inodes_per_group;     // 每个块组的inode数
    u32 s_mtime;               // 挂载时间
    u32 s_wtime;               // 写入时间
    u16 s_mnt_count;           // 挂载次数
    u16 s_max_mnt_count;       // 最大挂载次数
    u16 s_magic;               // 魔数
    u16 s_state;               // 文件系统状态
    u16 s_errors;              // 错误处理方式
    u16 s_minor_rev_level;     // 次版本号
    u32 s_lastcheck;           // 最后检查时间
    u32 s_checkinterval;       // 检查间隔
    u32 s_creator_os;          // 创建者OS
    u32 s_rev_level;           // 版本号
    u16 s_def_resuid;          // 保留块默认uid
    u16 s_def_resgid;          // 保留块默认gid
    
    // EXT4扩展字段
    u32 s_first_ino;           // 第一个非保留inode
    u16 s_inode_size;          // inode大小
    u16 s_block_group_nr;      // 备份超级块的块组号
    u32 s_feature_compat;      // 兼容特性
    u32 s_feature_incompat;    // 不兼容特性
    u32 s_feature_ro_compat;   // 只读兼容特性
    u8  s_uuid[16];            // 128位UUID
    char s_volume_name[16];    // 卷名
    char s_last_mounted[64];   // 最后挂载路径
    u32 s_algorithm_usage_bitmap; // 压缩算法
    
    u8  s_prealloc_blocks;     // 预分配块数
    u8  s_prealloc_dir_blocks; // 目录预分配块数
    u16 s_reserved_gdt_blocks; // 保留的GDT块数
    
    u8  s_journal_uuid[16];    // 日志UUID
    u32 s_journal_inum;        // 日志inode号
    u32 s_journal_dev;         // 日志设备
    u32 s_last_orphan;         // 孤儿inode链表头
    u32 s_hash_seed[4];        // HTREE哈希种子
    u8  s_def_hash_version;    // 默认哈希版本
    u8  s_jnl_backup_type;     // 日志备份类型
    u16 s_desc_size;           // 组描述符大小
    u32 s_default_mount_opts;  // 默认挂载选项
    u32 s_first_meta_bg;       // 第一个元数据块组
    u32 s_mkfs_time;           // 文件系统创建时间
    u32 s_jnl_blocks[17];      // 日志inode备份
    
    // 64位支持
    u32 s_blocks_count_hi;     // 块总数(高32位)
    u32 s_r_blocks_count_hi;   // 保留块数(高32位)
    u32 s_free_blocks_count_hi; // 空闲块数(高32位)
    u16 s_min_extra_isize;     // 最小额外inode大小
    u16 s_want_extra_isize;    // 期望额外inode大小
    u32 s_flags;               // 其他标志
    u16 s_raid_stride;         // RAID步长
    u16 s_mmp_update_interval; // MMP更新间隔
    u64 s_mmp_block;           // MMP块
    u32 s_raid_stripe_width;   // RAID条带宽度
    u8  s_log_groups_per_flex; // 每个flex组的组数
    u8  s_checksum_type;       // 校验和类型
    u8  s_reserved_pad[2];     // 保留
    u64 s_kbytes_written;      // 已写入KB数
    u32 s_snapshot_inum;       // 快照inode
    u32 s_snapshot_id;         // 快照ID
    u64 s_snapshot_r_blocks_count; // 快照保留块数
    u32 s_snapshot_list;       // 快照链表头
    u32 s_error_count;         // 错误计数
    u32 s_first_error_time;    // 第一次错误时间
    u32 s_first_error_ino;     // 第一次错误inode
    u64 s_first_error_block;   // 第一次错误块
    u8  s_first_error_func[32]; // 第一次错误函数
    u32 s_first_error_line;    // 第一次错误行号
    u32 s_last_error_time;     // 最后错误时间
    u32 s_last_error_ino;      // 最后错误inode
    u32 s_last_error_line;     // 最后错误行号
    u64 s_last_error_block;    // 最后错误块
    u8  s_last_error_func[32]; // 最后错误函数
    u8  s_mount_opts[64];      // 挂载选项
    u32 s_usr_quota_inum;      // 用户配额inode
    u32 s_grp_quota_inum;      // 组配额inode
    u32 s_overhead_clusters;   // 开销簇数
    u32 s_backup_bgs[2];       // 备份块组
    u8  s_encrypt_algos[4];    // 加密算法
    u8  s_encrypt_pw_salt[16]; // 加密密码盐
    u32 s_lpf_ino;             // lost+found inode
    u32 s_prj_quota_inum;      // 项目配额inode
    u32 s_checksum_seed;       // 校验和种子
    u32 s_reserved[98];        // 保留
    u32 s_checksum;            // 超级块校验和
    
    // 辅助方法
    u32 get_block_size() const {
        return 1024U << s_log_block_size;
    }
    
    u64 get_blocks_count() const {
        return static_cast<u64>(s_blocks_count_hi) << 32 | s_blocks_count_lo;
    }
    
    u64 get_free_blocks_count() const {
        return static_cast<u64>(s_free_blocks_count_hi) << 32 | s_free_blocks_count_lo;
    }
    
    bool has_feature(u32 feature) const {
        return s_feature_incompat & feature;
    }
} __attribute__((packed));

// EXT4块组描述符
struct Ext4GroupDesc {
    u32 bg_block_bitmap_lo;     // 块位图位置(低32位)
    u32 bg_inode_bitmap_lo;     // inode位图位置(低32位)
    u32 bg_inode_table_lo;      // inode表位置(低32位)
    u16 bg_free_blocks_count_lo; // 空闲块数(低16位)
    u16 bg_free_inodes_count_lo; // 空闲inode数(低16位)
    u16 bg_used_dirs_count_lo;   // 使用的目录数(低16位)
    u16 bg_flags;               // 标志
    u32 bg_exclude_bitmap_lo;   // 快照排除位图(低32位)
    u32 bg_block_bitmap_csum_lo; // 块位图校验和(低32位)
    u32 bg_inode_bitmap_csum_lo; // inode位图校验和(低32位)
    u16 bg_itable_unused_lo;    // 未使用inode数(低16位)
    u16 bg_checksum;            // 组描述符校验和
    
    // 64位扩展字段
    u32 bg_block_bitmap_hi;     // 块位图位置(高32位)
    u32 bg_inode_bitmap_hi;     // inode位图位置(高32位)
    u32 bg_inode_table_hi;      // inode表位置(高32位)
    u16 bg_free_blocks_count_hi; // 空闲块数(高16位)
    u16 bg_free_inodes_count_hi; // 空闲inode数(高16位)
    u16 bg_used_dirs_count_hi;   // 使用的目录数(高16位)
    u16 bg_itable_unused_hi;    // 未使用inode数(高16位)
    u32 bg_exclude_bitmap_hi;   // 快照排除位图(高32位)
    u32 bg_block_bitmap_csum_hi; // 块位图校验和(高32位)
    u32 bg_inode_bitmap_csum_hi; // inode位图校验和(高32位)
    u32 bg_reserved;            // 保留
    
    // 辅助方法
    u64 get_block_bitmap() const {
        return static_cast<u64>(bg_block_bitmap_hi) << 32 | bg_block_bitmap_lo;
    }
    
    u64 get_inode_bitmap() const {
        return static_cast<u64>(bg_inode_bitmap_hi) << 32 | bg_inode_bitmap_lo;
    }
    
    u64 get_inode_table() const {
        return static_cast<u64>(bg_inode_table_hi) << 32 | bg_inode_table_lo;
    }
    
    u32 get_free_blocks_count() const {
        return static_cast<u32>(bg_free_blocks_count_hi) << 16 | bg_free_blocks_count_lo;
    }
    
    u32 get_free_inodes_count() const {
        return static_cast<u32>(bg_free_inodes_count_hi) << 16 | bg_free_inodes_count_lo;
    }
} __attribute__((packed));

// EXT4 inode结构
struct Ext4Inode {
    u16 i_mode;                 // 文件类型和权限
    u16 i_uid;                  // 用户ID(低16位)
    u32 i_size_lo;              // 文件大小(低32位)
    u32 i_atime;                // 访问时间
    u32 i_ctime;                // 状态改变时间
    u32 i_mtime;                // 修改时间
    u32 i_dtime;                // 删除时间
    u16 i_gid;                  // 组ID(低16位)
    u16 i_links_count;          // 硬链接计数
    u32 i_blocks_lo;            // 块数(低32位)
    u32 i_flags;                // 标志
    union {
        struct {
            u32 l_i_version;    // Linux: 版本
        } linux1;
        struct {
            u32 h_i_translator; // Hurd: 转换器
        } hurd1;
        struct {
            u32 m_i_reserved1;  // Masix: 保留
        } masix1;
    } osd1;
    
    u32 i_block[EXT4_N_BLOCKS]; // 块指针
    u32 i_generation;           // 文件版本
    u32 i_file_acl_lo;          // 扩展属性块(低32位)
    u32 i_size_high;            // 文件大小(高32位)
    u32 i_obso_faddr;           // 废弃的片段地址
    
    union {
        struct {
            u16 l_i_blocks_high; // 块数(高16位)
            u16 l_i_file_acl_high; // 扩展属性块(高16位)
            u16 l_i_uid_high;    // 用户ID(高16位)
            u16 l_i_gid_high;    // 组ID(高16位)
            u16 l_i_checksum_lo; // inode校验和(低16位)
            u16 l_i_reserved;    // 保留
        } linux2;
        struct {
            u16 h_i_reserved1;   // Hurd: 保留
            u16 h_i_mode_high;   // Hurd: 模式(高16位)
            u16 h_i_uid_high;    // Hurd: 用户ID(高16位)
            u16 h_i_gid_high;    // Hurd: 组ID(高16位)
            u32 h_i_author;      // Hurd: 作者
        } hurd2;
        struct {
            u16 h_i_reserved1;   // Masix: 保留
            u16 m_i_file_acl_high; // Masix: 扩展属性块(高16位)
            u32 m_i_reserved2[2]; // Masix: 保留
        } masix2;
    } osd2;
    
    u16 i_extra_isize;          // 额外inode大小
    u16 i_checksum_hi;          // inode校验和(高16位)
    u32 i_ctime_extra;          // 额外状态改变时间
    u32 i_mtime_extra;          // 额外修改时间
    u32 i_atime_extra;          // 额外访问时间
    u32 i_crtime;               // 创建时间
    u32 i_crtime_extra;         // 额外创建时间
    u32 i_version_hi;           // 版本(高32位)
    u32 i_projid;               // 项目ID
    
    // 辅助方法
    u64 get_size() const {
        return static_cast<u64>(i_size_high) << 32 | i_size_lo;
    }
    
    void set_size(u64 size) {
        i_size_lo = size & 0xFFFFFFFFUL;
        i_size_high = size >> 32;
    }
    
    u64 get_blocks() const {
        return static_cast<u64>(osd2.linux2.l_i_blocks_high) << 32 | i_blocks_lo;
    }
    
    u32 get_uid() const {
        return static_cast<u32>(osd2.linux2.l_i_uid_high) << 16 | i_uid;
    }
    
    u32 get_gid() const {
        return static_cast<u32>(osd2.linux2.l_i_gid_high) << 16 | i_gid;
    }
    
    bool is_dir() const {
        return (i_mode & 0xF000) == 0x4000;
    }
    
    bool is_reg() const {
        return (i_mode & 0xF000) == 0x8000;
    }
    
    bool is_symlink() const {
        return (i_mode & 0xF000) == 0xA000;
    }
} __attribute__((packed));

// EXT4目录项结构
struct Ext4DirEntry2 {
    u32 inode;                  // inode号
    u16 rec_len;                // 记录长度
    u8  name_len;               // 文件名长度
    u8  file_type;              // 文件类型
    char name[];                // 文件名(变长)
    
    // 辅助方法
    std::string get_name() const {
        return std::string(name, name_len);
    }
    
    Ext4FileType get_file_type() const {
        return static_cast<Ext4FileType>(file_type);
    }
    
    size_t get_total_size() const {
        return sizeof(Ext4DirEntry2) + name_len;
    }
    
    size_t get_aligned_size() const {
        return (get_total_size() + 3) & ~3;  // 4字节对齐
    }
} __attribute__((packed));

// EXT4文件系统类
class Ext4FileSystem : public FileSystem, public SuperBlockOperations, public InodeOperations, 
                      public std::enable_shared_from_this<Ext4FileSystem> {
private:
    SharedPtr<SuperBlock> sb_;              // 超级块
    Ext4SuperBlock ext4_sb_;                // EXT4超级块数据
    std::vector<Ext4GroupDesc> group_desc_; // 块组描述符
    u32 block_size_;                        // 块大小
    u32 groups_count_;                      // 块组数
    
    // 位图缓存
    std::unordered_map<u32, std::vector<u8>> block_bitmaps_;
    std::unordered_map<u32, std::vector<u8>> inode_bitmaps_;
    mutable std::mutex bitmap_mutex_;
    
    // 内部方法
    Result<void> load_super_block(SharedPtr<BlockDevice> device);
    Result<void> load_group_descriptors(SharedPtr<BlockDevice> device);
    Result<std::vector<u8>&> get_block_bitmap(u32 group);
    Result<std::vector<u8>&> get_inode_bitmap(u32 group);
    Result<block_t> alloc_block(u32 group = 0);
    Result<void> free_block(block_t block);
    Result<inode_t> alloc_inode(u32 group = 0);
    Result<void> free_inode(inode_t ino);
    u32 inode_to_group(inode_t ino) const;
    u32 block_to_group(block_t block) const;

public:
    Ext4FileSystem();
    ~Ext4FileSystem();
    
    // FileSystem接口
    std::string get_name() const override;
    Result<SharedPtr<SuperBlock>> mount(SharedPtr<BlockDevice> device, 
                                       u32 flags, const std::string& options) override;
    Result<void> umount(SharedPtr<SuperBlock> sb) override;
    Result<void> statfs(SharedPtr<SuperBlock> sb, struct statfs* buf) override;
    
    // SuperBlockOperations接口
    Result<SharedPtr<Inode>> alloc_inode() override;
    Result<void> free_inode(SharedPtr<Inode> inode) override;
    Result<SharedPtr<Inode>> read_inode(inode_t ino) override;
    Result<void> write_inode(SharedPtr<Inode> inode) override;
    Result<void> sync() override;
    Result<void> statfs(struct statfs* buf) override;
    Result<void> remount(u32 flags) override;
    
    // InodeOperations接口
    Result<size_t> read(SharedPtr<Inode> inode, offset_t pos, 
                       void* buffer, size_t size) override;
    Result<size_t> write(SharedPtr<Inode> inode, offset_t pos, 
                        const void* buffer, size_t size) override;
    Result<std::vector<DirentEntry>> readdir(SharedPtr<Inode> inode) override;
    Result<SharedPtr<Inode>> lookup(SharedPtr<Inode> dir, 
                                   const std::string& name) override;
    Result<SharedPtr<Inode>> create(SharedPtr<Inode> dir, 
                                   const std::string& name, 
                                   FileMode mode) override;
    Result<void> unlink(SharedPtr<Inode> dir, const std::string& name) override;
    Result<void> mkdir(SharedPtr<Inode> dir, const std::string& name, 
                      FileMode mode) override;
    Result<void> rmdir(SharedPtr<Inode> dir, const std::string& name) override;
    Result<void> rename(SharedPtr<Inode> old_dir, const std::string& old_name,
                       SharedPtr<Inode> new_dir, const std::string& new_name) override;
    Result<FileAttribute> getattr(SharedPtr<Inode> inode) override;
    Result<void> setattr(SharedPtr<Inode> inode, const FileAttribute& attr) override;
    Result<std::string> getxattr(SharedPtr<Inode> inode, const std::string& name) override;
    Result<void> setxattr(SharedPtr<Inode> inode, const std::string& name, 
                         const std::string& value) override;
    Result<std::vector<std::string>> listxattr(SharedPtr<Inode> inode) override;
    Result<void> removexattr(SharedPtr<Inode> inode, const std::string& name) override;
    
    // 格式化方法
    static Result<void> mkfs(SharedPtr<BlockDevice> device, 
                            const std::string& options = "");
};

#endif // EXT4_H