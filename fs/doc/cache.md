# 页面缓存子系统 (Page Cache)

## 概述

页面缓存子系统就像是文件系统的"智能助手"或"记忆管家"。它在内存中缓存文件数据，让你不用每次都去慢速的硬盘读取，大大提高了系统性能。

想象一下：
- 页面缓存就像你桌上的"速记本"
- 经常用的资料放在桌上（内存），随手就能拿到
- 不常用的资料放在文件柜里（硬盘），需要时才去取
- 桌子空间有限，需要定期整理（页面淘汰）

## 数据结构关系图

```
文件系统层
    ↓
┌──────────────────────────────────────────────────────────┐
│                    页面缓存系统                          │
│  ┌────────────────────────────────────────────────────┐  │
│  │                 PageCache                          │  │
│  │  - pages_: Map<(inode,offset), Page>  页面索引    │  │
│  │  - lru_list_: 最近使用列表                       │  │
│  │  - max_pages_: 最大页面数                        │  │
│  │  - hits_: 缓存命中次数                           │  │
│  │  - misses_: 缓存未命中次数                       │  │
│  │  - writebacks_: 写回次数                         │  │
│  └────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────┘
    ↓
┌──────────────────────────────────────────────────────────┐
│                    页面对象                              │
│  ┌────────────────────────────────────────────────────┐  │
│  │                   Page                             │  │
│  │  - inode_: 所属文件                               │  │
│  │  - offset_: 在文件中的偏移量                      │  │
│  │  - data_: 实际数据 (4KB)                          │  │
│  │  - dirty_: 是否已修改                             │  │
│  │  - ref_count_: 引用计数                           │  │
│  │  - access_time_: 最后访问时间                     │  │
│  └────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────┘
    ↓
┌──────────────────────────────────────────────────────────┐
│                 LRU淘汰机制                              │
│  最近使用 ←→ Page1 ←→ Page2 ←→ Page3 ←→ 最久未用         │
│  (新增到此)                               (从此淘汰)     │
└──────────────────────────────────────────────────────────┘
```

## 头文件详解 (page_cache.h)

### 核心类型定义

```cpp
// 页面大小常量 - 就像笔记本的页面大小
static constexpr u32 PAGE_SIZE = 4096;  // 4KB

// 页面状态标志
enum PageFlags {
    PAGE_DIRTY = 1,      // 脏页 - 内容已修改，需要写回硬盘
    PAGE_LOCKED = 2,     // 锁定 - 正在使用，不能被淘汰
    PAGE_UPTODATE = 4,   // 最新 - 数据是最新的
    PAGE_ERROR = 8       // 错误 - 读取时发生错误
};
```

### Page 类 - 页面对象

每个Page就像笔记本的一页纸，记录着文件的一部分数据：

```cpp
class Page {
private:
    SharedPtr<Inode> inode_;        // 属于哪个文件
    offset_t offset_;               // 在文件中的位置（第几页）
    std::unique_ptr<u8[]> data_;    // 实际数据（4KB）
    u32 flags_;                     // 页面状态标志
    std::atomic<u32> ref_count_;    // 有多少人在使用这页
    std::chrono::steady_clock::time_point access_time_;  // 最后使用时间
    mutable std::mutex mutex_;      // 保护并发访问
    
public:
    Page(SharedPtr<Inode> inode, offset_t offset);
    
    // 获取/释放引用
    void get() { ref_count_++; }    // 有人开始使用这页
    void put() { ref_count_--; }    // 有人用完了这页
    
    // 状态管理
    void mark_dirty() { flags_ |= PAGE_DIRTY; }      // 标记为已修改
    void clear_dirty() { flags_ &= ~PAGE_DIRTY; }    // 清除修改标记
    bool is_dirty() const { return flags_ & PAGE_DIRTY; }
    
    // 数据访问
    u8* get_data() { return data_.get(); }           // 获取数据指针
    const u8* get_data() const { return data_.get(); }
};
```

### PageCache 类 - 页面缓存管理器

PageCache就像一个智能的"资料管理员"：

```cpp
class PageCache {
private:
    // 页面索引 - 就像图书馆的卡片目录
    std::unordered_map<PageKey, SharedPtr<Page>, PageKeyHash> pages_;
    
    // LRU链表 - 记录使用顺序
    std::list<SharedPtr<Page>> lru_list_;
    
    // 配置参数
    size_t max_pages_;              // 桌子能放多少张纸
    
    // 统计信息
    std::atomic<u64> hits_;         // 命中次数 - 在桌上找到资料的次数
    std::atomic<u64> misses_;       // 未命中次数 - 需要去文件柜找的次数
    std::atomic<u64> writebacks_;   // 写回次数 - 把资料归档到文件柜的次数
    
    mutable std::shared_mutex mutex_;  // 读写锁
    
public:
    PageCache(size_t max_pages = 1024);  // 默认最多1024页（4MB）
    
    // 核心操作
    SharedPtr<Page> find_or_create_page(SharedPtr<Inode> inode, offset_t offset);
    Result<SharedPtr<Page>> read_page(SharedPtr<Inode> inode, offset_t offset);
    Result<void> write_page(SharedPtr<Page> page);
    Result<void> sync_pages(SharedPtr<Inode> inode);
    void invalidate_pages(SharedPtr<Inode> inode);
};
```

## 源文件实现详解 (page_cache.cpp)

### PageKey - 页面索引键

就像图书馆用"书名+页码"来找书页一样：

```cpp
struct PageKey {
    SharedPtr<Inode> inode;  // 哪本书
    offset_t offset;         // 第几页
    
    bool operator==(const PageKey& other) const {
        return inode == other.inode && offset == other.offset;
    }
};

// 哈希函数 - 为快速查找提供"索引号"
struct PageKeyHash {
    size_t operator()(const PageKey& key) const {
        return std::hash<void*>()(key.inode.get()) ^ std::hash<u64>()(key.offset);
    }
};
```

### 核心操作实现

#### find_or_create_page - 查找或创建页面

这是缓存系统的核心，就像智能助手帮你找资料：

```cpp
SharedPtr<Page> PageCache::find_or_create_page(SharedPtr<Inode> inode, offset_t offset) {
    PageKey key{inode, offset};
    
    // 1. 先在桌上找（缓存查找）
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = pages_.find(key);
        if (it != pages_.end()) {
            hits_++;  // 命中！在桌上找到了
            auto page = it->second;
            
            // 更新访问时间（移动到LRU链表前端）
            update_lru(page);
            page->get();  // 增加引用计数
            return page;
        }
    }
    
    // 2. 桌上没有，需要创建新页面
    misses_++;  // 未命中，需要去文件柜找
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // 检查是否需要淘汰旧页面（桌子满了）
    if (pages_.size() >= max_pages_) {
        evict_pages();  // 整理桌面，移除不常用的资料
    }
    
    // 创建新页面
    auto page = std::make_shared<Page>(inode, offset);
    pages_[key] = page;
    lru_list_.push_front(page);  // 放在最前面（最近使用）
    
    page->get();  // 增加引用计数
    return page;
}
```

#### 实际操作例子

**例子1：第一次访问文件页面**

```cpp
// 操作前状态：
// pages_ = {} (空)
// hits_ = 0, misses_ = 0

auto inode = get_file_inode("/tmp/test.txt");
auto page = cache.find_or_create_page(inode, 0);

// 操作后状态：
// pages_ = {(inode,0): Page对象}
// hits_ = 0, misses_ = 1
// lru_list_ = [Page(inode,0)]  # 最新的在前面
// page->ref_count_ = 1
```

**例子2：再次访问相同页面**

```cpp
// 操作前状态：
// pages_ = {(inode,0): Page对象}
// hits_ = 0, misses_ = 1

auto page2 = cache.find_or_create_page(inode, 0);

// 操作后状态：
// pages_ = {(inode,0): Page对象}  # 没变化
// hits_ = 1, misses_ = 1  # 命中+1
// page2 == page  # 返回同一个对象
// page->ref_count_ = 2  # 引用计数增加
```

#### read_page - 读取页面数据

就像从文件柜中取出资料并放到桌上：

```cpp
Result<SharedPtr<Page>> PageCache::read_page(SharedPtr<Inode> inode, offset_t offset) {
    // 1. 先尝试从缓存获取
    auto page = find_or_create_page(inode, offset);
    
    // 2. 如果页面是最新的，直接返回
    if (page->is_uptodate()) {
        return page;
    }
    
    // 3. 从硬盘读取数据
    auto read_result = inode->read(offset, page->get_data(), PAGE_SIZE);
    if (read_result.is_err()) {
        page->mark_error();
        return read_result.error();
    }
    
    // 4. 标记为最新
    page->mark_uptodate();
    return page;
}
```

#### 实际操作例子

**例子3：读取文件数据**

```cpp
// 操作前状态：
// 文件内容: "Hello, World!" (在硬盘上)
// 缓存为空

auto result = cache.read_page(inode, 0);
if (result.is_ok()) {
    auto page = result.unwrap();
    
    // 操作后状态：
    // page->get_data()[0-12] = "Hello, World!"
    // page->is_uptodate() = true
    // 缓存中有了这一页数据
    
    // 可以直接从内存读取，不用再访问硬盘
    std::string content(reinterpret_cast<char*>(page->get_data()), 13);
    std::cout << "文件内容: " << content << std::endl;  // 输出: Hello, World!
}
```

#### write_page - 写入页面数据

就像把修改过的资料归档回文件柜：

```cpp
Result<void> PageCache::write_page(SharedPtr<Page> page) {
    // 1. 检查页面是否需要写回
    if (!page->is_dirty()) {
        return Result<void>();  // 没有修改，不需要写回
    }
    
    // 2. 写入到硬盘
    auto inode = page->get_inode();
    auto offset = page->get_offset();
    auto write_result = inode->write(offset, page->get_data(), PAGE_SIZE);
    
    if (write_result.is_err()) {
        return write_result.error();
    }
    
    // 3. 清除脏标记
    page->clear_dirty();
    writebacks_++;  // 写回计数+1
    
    return Result<void>();
}
```

#### 实际操作例子

**例子4：修改文件内容**

```cpp
// 操作前状态：
// page->get_data() = "Hello, World!"
// page->is_dirty() = false

// 修改页面内容
strcpy(reinterpret_cast<char*>(page->get_data()), "Hello, Linux!");
page->mark_dirty();

// 操作后状态：
// page->get_data() = "Hello, Linux!"
// page->is_dirty() = true

// 写回到硬盘
auto result = cache.write_page(page);

// 最终状态：
// 硬盘文件内容: "Hello, Linux!"
// page->is_dirty() = false
// writebacks_ = 1
```

### LRU淘汰机制

当缓存满了时，需要移除最久未使用的页面：

```cpp
void PageCache::evict_pages() {
    // 从LRU链表末尾开始淘汰（最久未使用的）
    while (pages_.size() > max_pages_ * 0.8 && !lru_list_.empty()) {
        auto victim = lru_list_.back();  // 最久未使用的页面
        
        // 只能淘汰引用计数为0的页面
        if (victim->get_ref_count() == 0) {
            // 如果是脏页，先写回硬盘
            if (victim->is_dirty()) {
                write_page(victim);
            }
            
            // 从缓存中移除
            PageKey key{victim->get_inode(), victim->get_offset()};
            pages_.erase(key);
            lru_list_.pop_back();
        } else {
            break;  // 找不到可淘汰的页面
        }
    }
}
```

#### 实际操作例子

**例子5：缓存淘汰**

```cpp
// 操作前状态：
// max_pages_ = 3
// lru_list_ = [Page1, Page2, Page3]  # Page3最久未使用
// pages_.size() = 3
// Page3->ref_count_ = 0, Page3->is_dirty() = true

// 需要添加新页面，触发淘汰
auto new_page = cache.find_or_create_page(inode2, 4096);

// 淘汰过程：
// 1. 发现Page3可以淘汰（ref_count=0）
// 2. Page3是脏页，先写回硬盘
// 3. 从pages_和lru_list_中移除Page3
// 4. 添加new_page

// 操作后状态：
// lru_list_ = [new_page, Page1, Page2]
// pages_.size() = 3
// Page3已被释放，内容已写回硬盘
```

## 使用场景和例子

### 场景1：频繁读取同一文件

```cpp
// 就像反复查看同一份资料

auto inode = get_file_inode("/etc/config.txt");

// 第一次读取 - 需要从硬盘加载
auto start = std::chrono::high_resolution_clock::now();
auto page1 = cache.read_page(inode, 0);
auto time1 = std::chrono::high_resolution_clock::now() - start;

// 第二次读取 - 从缓存获取，很快！
start = std::chrono::high_resolution_clock::now();
auto page2 = cache.read_page(inode, 0);
auto time2 = std::chrono::high_resolution_clock::now() - start;

std::cout << "第一次读取用时: " << time1.count() << "ns" << std::endl;
std::cout << "第二次读取用时: " << time2.count() << "ns" << std::endl;
// 第二次通常会快很多！
```

### 场景2：批量文件操作

```cpp
// 就像同时处理多份文档

std::vector<SharedPtr<Inode>> files = {inode1, inode2, inode3};

// 预加载所有文件的第一页到缓存
for (auto& inode : files) {
    cache.find_or_create_page(inode, 0);
}

// 现在可以快速访问这些文件
for (auto& inode : files) {
    auto page = cache.find_or_create_page(inode, 0);  // 缓存命中！
    // 处理文件内容...
}

// 查看缓存效果
std::cout << "缓存命中率: " << cache.get_hit_rate() * 100 << "%" << std::endl;
```

### 场景3：内存压力下的缓存管理

```cpp
// 就像桌子空间不够时的整理

PageCache small_cache(2);  // 只能缓存2页

auto page1 = small_cache.find_or_create_page(inode1, 0);    // 缓存: [page1]
auto page2 = small_cache.find_or_create_page(inode2, 0);    // 缓存: [page2, page1]

page1->put();  // 释放page1的引用

auto page3 = small_cache.find_or_create_page(inode3, 0);    // 缓存: [page3, page2]
                                                            // page1被淘汰

// 再次访问page1需要重新加载
auto page1_again = small_cache.find_or_create_page(inode1, 0);  // 缓存未命中
```

### 场景4：同步和一致性

```cpp
// 就像定期整理和归档资料

// 修改多个页面
auto page1 = cache.find_or_create_page(inode, 0);
auto page2 = cache.find_or_create_page(inode, 4096);

strcpy(reinterpret_cast<char*>(page1->get_data()), "Modified content 1");
strcpy(reinterpret_cast<char*>(page2->get_data()), "Modified content 2");

page1->mark_dirty();
page2->mark_dirty();

// 同步所有修改到硬盘
auto sync_result = cache.sync_pages(inode);
if (sync_result.is_ok()) {
    std::cout << "所有修改已保存到硬盘" << std::endl;
}

// 查看统计信息
std::cout << "总写回次数: " << cache.get_writebacks() << std::endl;
```

## 性能特点

1. **高命中率**：经常访问的数据保存在内存中，减少硬盘I/O
2. **智能淘汰**：LRU算法确保最常用的数据留在缓存中
3. **延迟写入**：修改先保存在内存，批量写回硬盘
4. **并发安全**：支持多线程同时访问
5. **统计监控**：提供详细的性能统计信息

页面缓存就像是文件系统的"记忆"，让系统更聪明、更快速！