#include "buffer_pool.h"

BufferPool::BufferPool(size_t pool_size, DiskManager* disk_manager) 
    : pool_size_(pool_size), disk_manager_(disk_manager) {
    pages_ = new Page[pool_size_];
    for (size_t i = 0; i < pool_size_; ++i) {
        free_list_.push_back(static_cast<frame_id_t>(i));
    }
}

BufferPool::~BufferPool() {
    for (size_t i = 0; i < pool_size_; ++i) {
        if (pages_[i].is_dirty_) {
            disk_manager_->WritePage(pages_[i].GetPageId(), pages_[i].GetData());
        }
    }
    delete[] pages_;
}

bool BufferPool::FindVictim(frame_id_t* out_frame_id) {
    if (!free_list_.empty()) {
        *out_frame_id = free_list_.front();
        free_list_.pop_front();
        return true;
    }
    // Scan LRU list (front = least recently used) for an unpinned page to evict
    for (auto it = lru_list_.begin(); it != lru_list_.end(); ++it) {
        frame_id_t frame_id = *it;
        if (pages_[frame_id].pin_count_ == 0) {
            if (pages_[frame_id].is_dirty_) {
                disk_manager_->WritePage(pages_[frame_id].GetPageId(), pages_[frame_id].GetData());
            }
            page_table_.erase(pages_[frame_id].GetPageId());
            *out_frame_id = frame_id;
            lru_list_.erase(it);
            return true;
        }
    }
    return false; // All frames pinned: no victim available
}

Page* BufferPool::FetchPage(page_id_t page_id) {
    if (page_table_.find(page_id) != page_table_.end()) {
        frame_id_t frame_id = page_table_[page_id];
        pages_[frame_id].pin_count_++;
        lru_list_.remove(frame_id); // Remove from LRU to prevent eviction
        return &pages_[frame_id];
    }

    frame_id_t victim_frame;
    if (!FindVictim(&victim_frame)) return nullptr;

    Page* target_page = &pages_[victim_frame];
    target_page->ResetMemory();
    target_page->page_id_ = page_id;
    target_page->pin_count_ = 1;
    target_page->is_dirty_ = false;
    
    disk_manager_->ReadPage(page_id, target_page->GetData());
    page_table_[page_id] = victim_frame;
    return target_page;
}

Page* BufferPool::NewPage(page_id_t* out_page_id) {
    frame_id_t victim_frame;
    if (!FindVictim(&victim_frame)) return nullptr;

    page_id_t new_page_id = disk_manager_->AllocatePage();
    *out_page_id = new_page_id;

    Page* target_page = &pages_[victim_frame];
    target_page->ResetMemory();
    target_page->page_id_ = new_page_id;
    target_page->pin_count_ = 1;
    target_page->is_dirty_ = false;

    page_table_[new_page_id] = victim_frame;
    return target_page;
}

bool BufferPool::UnpinPage(page_id_t page_id, bool is_dirty) {
    if (page_table_.find(page_id) == page_table_.end()) return false;
    frame_id_t frame_id = page_table_[page_id];
    
    if (pages_[frame_id].pin_count_ <= 0) return false;
    pages_[frame_id].pin_count_--;
    if (is_dirty) pages_[frame_id].is_dirty_ = true;

    // If unpinned, add to the tail of the LRU list (becomes evictable)
    if (pages_[frame_id].pin_count_ == 0) {
        lru_list_.push_back(frame_id);
    }
    return true;
}