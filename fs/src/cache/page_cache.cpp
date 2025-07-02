#include "../../include/page_cache.h"
#include "../../include/vfs.h"
#include <cstring>
#include <algorithm>

// 全局页面缓存实例
PageCache g_page_cache;

// Page类实现

Page::Page(offset_t offset, SharedPtr<Inode> inode)
    : offset_(offset), inode_(inode), ref_count_(1), 
      state_(PageState::CLEAN), in_lru_(false) {
    
    // 分配页面数据(4KB对齐)
    data_ = static_cast<u8*>(std::aligned_alloc(PAGE_SIZE, PAGE_SIZE));
    if (!data_) {
        throw std::bad_alloc();
    }
    
    // 初始化为零
    std::memset(data_, 0, PAGE_SIZE);
}

Page::~Page() {
    if (data_) {
        std::free(data_);
    }
}

void Page::put() {
    if (--ref_count_ == 0) {
        // 页面引用计数为0，可以被回收
        g_page_cache.release_page(shared_from_this());
    }
}

void Page::lock() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // 等待页面解锁
    cv_.wait(lock, [this] {
        return state_.load() != PageState::LOCKED;
    });
    
    // 锁定页面
    state_.store(PageState::LOCKED);
}

void Page::unlock() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_.load() == PageState::LOCKED) {
            state_.store(PageState::UPTODATE);
        }
    }
    cv_.notify_all();
}

bool Page::try_lock() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_.load() == PageState::LOCKED) {
        return false;
    }
    
    state_.store(PageState::LOCKED);
    return true;
}

void Page::wait_unlock() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] {
        return state_.load() != PageState::LOCKED;
    });
}

void Page::mark_dirty() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    PageState old_state = state_.load();
    if (old_state != PageState::DIRTY && old_state != PageState::WRITEBACK) {
        state_.store(PageState::DIRTY);
        
        // 添加到脏页面链表
        g_page_cache.add_to_dirty_list(shared_from_this());
    }
}

void Page::clear_dirty() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_.load() == PageState::DIRTY) {
        state_.store(PageState::UPTODATE);
        
        // 从脏页面链表移除
        g_page_cache.remove_from_dirty_list(shared_from_this());
    }
}

// PageCache类实现

PageCache::PageCache(size_t max_pages)
    : max_pages_(max_pages), current_pages_(0),
      hits_(0), misses_(0), evictions_(0), writebacks_(0) {
}

PageCache::~PageCache() {
    // 同步所有脏页面
    sync_pages();
    
    // 清空缓存
    clear();
}

SharedPtr<Page> PageCache::find_page(SharedPtr<Inode> inode, offset_t offset) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    PageKey key(inode, offset);
    auto iter = pages_.find(key);
    
    if (iter != pages_.end()) {
        SharedPtr<Page> page = iter->second;
        update_lru(page);
        hits_++;
        return page;
    }
    
    misses_++;
    return nullptr;
}

SharedPtr<Page> PageCache::find_or_create_page(SharedPtr<Inode> inode, offset_t offset) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    PageKey key(inode, offset);
    SharedPtr<Page> page = find_page_locked(key);
    
    if (page) {
        return page;
    }
    
    // 创建新页面
    page = allocate_page(key);
    return page;
}

Result<SharedPtr<Page>> PageCache::read_page(SharedPtr<Inode> inode, offset_t offset) {
    // 查找或创建页面
    SharedPtr<Page> page = find_or_create_page(inode, offset);
    if (!page) {
        return ErrorCode::ENOMEM;
    }
    
    // 如果页面已是最新的，直接返回
    if (page->is_uptodate()) {
        return page;
    }
    
    // 锁定页面进行I/O
    page->lock();
    
    // 再次检查状态(避免重复读取)
    if (page->is_uptodate()) {
        page->unlock();
        return page;
    }
    
    // 从磁盘读取数据
    auto block_device = inode->get_block_device();
    if (!block_device) {
        page->unlock();
        return ErrorCode::EIO;
    }
    
    // 计算扇区号(假设页面对齐到扇区边界)
    sector_t sector = offset / block_device->get_sector_size();
    
    auto result = block_device->read(sector, page->get_data(), PAGE_SIZE);
    if (result.is_err()) {
        page->set_state(PageState::ERROR);
        page->unlock();
        return result.error();
    }
    
    // 标记页面为最新
    page->set_state(PageState::UPTODATE);
    page->unlock();
    
    return page;
}

void PageCache::write_page(SharedPtr<Page> page) {
    page->mark_dirty();
    
    std::lock_guard<std::mutex> lock(mutex_);
    update_lru(page);
}

Result<void> PageCache::sync_pages(SharedPtr<Inode> inode) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // 收集需要同步的脏页面
    std::vector<SharedPtr<Page>> pages_to_sync;
    
    for (auto& page : dirty_list_) {
        if (!inode || page->get_inode() == inode) {
            if (page->get_state() == PageState::DIRTY) {
                pages_to_sync.push_back(page);
            }
        }
    }
    
    lock.unlock();
    
    // 同步脏页面
    for (auto& page : pages_to_sync) {
        // 锁定页面
        page->lock();
        
        // 检查页面是否仍然是脏的
        if (page->get_state() != PageState::DIRTY) {
            page->unlock();
            continue;
        }
        
        // 标记为正在写回
        page->set_state(PageState::WRITEBACK);
        
        // 写入磁盘
        auto block_device = page->get_inode()->get_block_device();
        if (block_device) {
            sector_t sector = page->get_offset() / block_device->get_sector_size();
            auto result = block_device->write(sector, page->get_data(), PAGE_SIZE);
            
            if (result.is_ok()) {
                page->clear_dirty();
                writebacks_++;
            } else {
                page->set_state(PageState::ERROR);
                page->unlock();
                return result.error();
            }
        }
        
        page->unlock();
    }
    
    return Result<void>(ErrorCode::SUCCESS);
}

void PageCache::invalidate_pages(SharedPtr<Inode> inode) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 移除属于指定inode的所有页面
    auto iter = pages_.begin();
    while (iter != pages_.end()) {
        if (iter->first.inode == inode) {
            SharedPtr<Page> page = iter->second;
            
            // 从LRU链表移除
            remove_from_lru(page);
            
            // 从脏页面链表移除
            if (page->is_dirty()) {
                remove_from_dirty_list(page);
            }
            
            iter = pages_.erase(iter);
            current_pages_--;
        } else {
            ++iter;
        }
    }
}

void PageCache::release_page(SharedPtr<Page> page) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 只有引用计数为0的页面才能被释放
    if (page->ref_count() > 0) {
        return;
    }
    
    PageKey key(page->get_inode(), page->get_offset());
    pages_.erase(key);
    
    remove_from_lru(page);
    if (page->is_dirty()) {
        remove_from_dirty_list(page);
    }
    
    current_pages_--;
}

void PageCache::set_max_pages(size_t max_pages) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    max_pages_ = max_pages;
    
    // 如果当前页面数超过新的限制，需要淘汰一些页面
    if (current_pages_ > max_pages_) {
        evict_pages(current_pages_ - max_pages_);
    }
}

Result<void> PageCache::flush_all() {
    return sync_pages();
}

void PageCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    pages_.clear();
    lru_list_.clear();
    dirty_list_.clear();
    current_pages_ = 0;
}

// 私有方法实现

SharedPtr<Page> PageCache::find_page_locked(const PageKey& key) {
    auto iter = pages_.find(key);
    
    if (iter != pages_.end()) {
        SharedPtr<Page> page = iter->second;
        update_lru(page);
        hits_++;
        return page;
    }
    
    misses_++;
    return nullptr;
}

SharedPtr<Page> PageCache::allocate_page(const PageKey& key) {
    // 检查是否需要淘汰页面
    if (current_pages_ >= max_pages_) {
        evict_pages(1);
    }
    
    // 创建新页面
    SharedPtr<Page> page = std::make_shared<Page>(key.offset, key.inode);
    
    // 添加到哈希表
    pages_[key] = page;
    
    // 添加到LRU链表头部
    lru_list_.push_front(page);
    page->lru_iter_ = lru_list_.begin();
    page->in_lru_ = true;
    
    current_pages_++;
    
    return page;
}

void PageCache::evict_pages(size_t count) {
    size_t evicted = 0;
    
    // 从LRU链表尾部开始淘汰
    while (evicted < count && !lru_list_.empty()) {
        SharedPtr<Page> page = lru_list_.back();
        
        // 不能淘汰被引用的页面
        if (page->ref_count() > 1) {
            // 将页面移到前面，稍后再试
            lru_list_.pop_back();
            lru_list_.push_front(page);
            page->lru_iter_ = lru_list_.begin();
            continue;
        }
        
        // 如果是脏页面，需要先写回
        if (page->is_dirty()) {
            // 这里简化处理，实际应该异步写回
            auto block_device = page->get_inode()->get_block_device();
            if (block_device) {
                sector_t sector = page->get_offset() / block_device->get_sector_size();
                block_device->write(sector, page->get_data(), PAGE_SIZE);
                page->clear_dirty();
                writebacks_++;
            }
        }
        
        // 从哈希表移除
        PageKey key(page->get_inode(), page->get_offset());
        pages_.erase(key);
        
        // 从LRU链表移除
        remove_from_lru(page);
        
        current_pages_--;
        evicted++;
        evictions_++;
    }
}

void PageCache::update_lru(SharedPtr<Page> page) {
    if (page->in_lru_) {
        // 移到链表头部
        lru_list_.erase(page->lru_iter_);
        lru_list_.push_front(page);
        page->lru_iter_ = lru_list_.begin();
    }
}

void PageCache::remove_from_lru(SharedPtr<Page> page) {
    if (page->in_lru_) {
        lru_list_.erase(page->lru_iter_);
        page->in_lru_ = false;
    }
}

void PageCache::add_to_dirty_list(SharedPtr<Page> page) {
    // 简化实现：直接添加到脏链表
    dirty_list_.push_back(page);
}

void PageCache::remove_from_dirty_list(SharedPtr<Page> page) {
    // 简化实现：线性查找并移除
    dirty_list_.remove(page);
}