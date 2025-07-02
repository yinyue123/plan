#ifndef PAGE_CACHE_H
#define PAGE_CACHE_H

#include "types.h"
#include "block_device.h"
#include <unordered_map>
#include <list>
#include <mutex>
#include <atomic>
#include <condition_variable>

// 前向声明
class PageCache;
class Inode;

// 页状态枚举
enum class PageState : u8 {
    CLEAN = 0,          // 干净页面，与磁盘一致
    DIRTY = 1,          // 脏页面，需要写回磁盘
    LOCKED = 2,         // 页面被锁定，正在I/O操作
    ERROR = 3,          // 页面发生错误
    UPTODATE = 4,       // 页面数据是最新的
    WRITEBACK = 5       // 页面正在写回磁盘
};

// 页面结构
class Page {
private:
    u8* data_;                          // 页面数据(4KB)
    offset_t offset_;                   // 页面在文件中的偏移量
    SharedPtr<Inode> inode_;            // 所属的inode
    std::atomic<u32> ref_count_;        // 引用计数
    std::atomic<PageState> state_;      // 页面状态
    mutable std::mutex mutex_;          // 页面锁
    std::condition_variable cv_;        // 条件变量
    
    // LRU链表节点
    std::list<SharedPtr<Page>>::iterator lru_iter_;
    bool in_lru_;                       // 是否在LRU链表中
    
    friend class PageCache;

public:
    Page(offset_t offset, SharedPtr<Inode> inode);
    ~Page();
    
    // 禁止拷贝和赋值
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;
    
    // 页面访问
    u8* get_data() { return data_; }
    const u8* get_data() const { return data_; }
    offset_t get_offset() const { return offset_; }
    SharedPtr<Inode> get_inode() const { return inode_; }
    
    // 引用计数管理
    void get() { ref_count_++; }
    void put();
    u32 ref_count() const { return ref_count_.load(); }
    
    // 状态管理
    PageState get_state() const { return state_.load(); }
    void set_state(PageState state) { state_.store(state); }
    bool is_dirty() const { return state_.load() == PageState::DIRTY; }
    bool is_locked() const { return state_.load() == PageState::LOCKED; }
    bool is_uptodate() const { return state_.load() == PageState::UPTODATE; }
    
    // 页面锁定
    void lock();
    void unlock();
    bool try_lock();
    
    // 等待页面解锁
    void wait_unlock();
    
    // 标记页面为脏
    void mark_dirty();
    
    // 清理页面
    void clear_dirty();
};

// 页面缓存键
struct PageKey {
    SharedPtr<Inode> inode;
    offset_t offset;
    
    PageKey(SharedPtr<Inode> ino, offset_t off) : inode(ino), offset(off) {}
    
    bool operator==(const PageKey& other) const {
        return inode == other.inode && offset == other.offset;
    }
};

// 页面缓存键的哈希函数
struct PageKeyHash {
    size_t operator()(const PageKey& key) const {
        size_t h1 = std::hash<void*>{}(key.inode.get());
        size_t h2 = std::hash<offset_t>{}(key.offset);
        return h1 ^ (h2 << 1);
    }
};

// 页面缓存类
class PageCache {
private:
    // 页面哈希表：键是(inode, offset)，值是页面
    std::unordered_map<PageKey, SharedPtr<Page>, PageKeyHash> pages_;
    
    // LRU链表：最近使用的页面在前面
    std::list<SharedPtr<Page>> lru_list_;
    
    // 脏页面链表：需要写回的页面
    std::list<SharedPtr<Page>> dirty_list_;
    
    mutable std::mutex mutex_;          // 保护并发访问
    size_t max_pages_;                  // 最大页面数
    size_t current_pages_;              // 当前页面数
    
    // 统计信息
    std::atomic<u64> hits_;             // 缓存命中次数
    std::atomic<u64> misses_;           // 缓存未命中次数
    std::atomic<u64> evictions_;        // 页面淘汰次数
    std::atomic<u64> writebacks_;       // 写回次数
    
    // 内部方法
    SharedPtr<Page> find_page_locked(const PageKey& key);
    SharedPtr<Page> allocate_page(const PageKey& key);
    void evict_pages(size_t count);
    void update_lru(SharedPtr<Page> page);
    void remove_from_lru(SharedPtr<Page> page);
    void add_to_dirty_list(SharedPtr<Page> page);
    void remove_from_dirty_list(SharedPtr<Page> page);

public:
    PageCache(size_t max_pages = 1024);  // 默认最大1024页(4MB)
    ~PageCache();
    
    // 查找或创建页面
    SharedPtr<Page> find_or_create_page(SharedPtr<Inode> inode, offset_t offset);
    
    // 查找页面(不创建)
    SharedPtr<Page> find_page(SharedPtr<Inode> inode, offset_t offset);
    
    // 读取页面(从磁盘加载)
    Result<SharedPtr<Page>> read_page(SharedPtr<Inode> inode, offset_t offset);
    
    // 写入页面(标记为脏)
    void write_page(SharedPtr<Page> page);
    
    // 同步脏页面到磁盘
    Result<void> sync_pages(SharedPtr<Inode> inode = nullptr);
    
    // 使页面失效
    void invalidate_pages(SharedPtr<Inode> inode);
    
    // 释放页面
    void release_page(SharedPtr<Page> page);
    
    // 统计信息
    size_t get_page_count() const { return current_pages_; }
    size_t get_max_pages() const { return max_pages_; }
    u64 get_hits() const { return hits_.load(); }
    u64 get_misses() const { return misses_.load(); }
    u64 get_evictions() const { return evictions_.load(); }
    u64 get_writebacks() const { return writebacks_.load(); }
    
    // 缓存命中率
    double get_hit_rate() const {
        u64 total = hits_.load() + misses_.load();
        return total == 0 ? 0.0 : static_cast<double>(hits_.load()) / total;
    }
    
    // 设置最大页面数
    void set_max_pages(size_t max_pages);
    
    // 强制刷新所有脏页面
    Result<void> flush_all();
    
    // 清空缓存
    void clear();
};

// 全局页面缓存实例
extern PageCache g_page_cache;

#endif // PAGE_CACHE_H