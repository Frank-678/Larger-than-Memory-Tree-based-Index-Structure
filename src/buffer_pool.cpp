#include "buffer_pool.h"

// ============================================================================
// Intrusive doubly-linked-list helpers — O(1), zero heap allocation.
// All index arguments are frame_id_t values that also serve as indices
// into lru_nodes_[].  A node with prev == -1 && next == -1 is detached.
// ============================================================================

void BufferPool::DetachFromLRU(int frame_id) {
    int prev = lru_nodes_[frame_id].prev;
    int next = lru_nodes_[frame_id].next;

    if (prev != -1) lru_nodes_[prev].next = next;
    else            lru_head_ = next;  // was the head

    if (next != -1) lru_nodes_[next].prev = prev;
    else            lru_tail_ = prev;   // was the tail

    lru_nodes_[frame_id].prev = -1;
    lru_nodes_[frame_id].next = -1;
}

void BufferPool::AppendToLRU(int frame_id) {
    // Node should already be detached (safety: detach if still linked).
    // We tolerate a linked node by detaching it first.
    if (lru_nodes_[frame_id].prev != -1 || lru_nodes_[frame_id].next != -1) {
        DetachFromLRU(frame_id);
    }

    if (lru_tail_ == -1) {
        // Empty LRU list
        lru_head_ = lru_tail_ = frame_id;
        lru_nodes_[frame_id].prev = -1;
        lru_nodes_[frame_id].next = -1;
    } else {
        lru_nodes_[lru_tail_].next = frame_id;
        lru_nodes_[frame_id].prev = lru_tail_;
        lru_nodes_[frame_id].next = -1;
        lru_tail_ = frame_id;
    }
}

int BufferPool::PopFromFree() {
    if (free_head_ == -1) return -1;

    int frame_id = free_head_;
    free_head_ = lru_nodes_[free_head_].next;

    if (free_head_ != -1) {
        lru_nodes_[free_head_].prev = -1;
    } else {
        free_tail_ = -1;  // free list became empty
    }

    lru_nodes_[frame_id].prev = -1;
    lru_nodes_[frame_id].next = -1;
    return frame_id;
}

void BufferPool::PushToFree(int frame_id) {
    lru_nodes_[frame_id].prev = free_tail_;
    lru_nodes_[frame_id].next = -1;

    if (free_tail_ != -1) {
        lru_nodes_[free_tail_].next = frame_id;
    } else {
        free_head_ = frame_id;  // free list was empty
    }
    free_tail_ = frame_id;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

BufferPool::BufferPool(size_t pool_size, DiskManager* disk_manager)
    : pool_size_(pool_size), disk_manager_(disk_manager),
      free_head_(-1), free_tail_(-1),
      lru_head_(-1), lru_tail_(-1) {

    pages_ = new Page[pool_size_];
    lru_nodes_ = new LRUNode[pool_size_];

    // Wire all frames into the free list as a contiguous doubly-linked chain.
    for (size_t i = 0; i < pool_size_; ++i) {
        lru_nodes_[i].prev = (i == 0) ? -1 : static_cast<int>(i - 1);
        lru_nodes_[i].next = (i == pool_size_ - 1) ? -1 : static_cast<int>(i + 1);
    }
    free_head_ = 0;
    free_tail_ = static_cast<int>(pool_size_ - 1);
}

BufferPool::~BufferPool() {
    // Flush any dirty pages back to disk before tearing down.
    for (size_t i = 0; i < pool_size_; ++i) {
        if (pages_[i].is_dirty_) {
            disk_manager_->WritePage(pages_[i].GetPageId(), pages_[i].GetData());
        }
    }
    delete[] pages_;
    delete[] lru_nodes_;
}

// ============================================================================
// FindVictim — first tries the free list (O(1)), then scans the LRU from head
// (least recently used) for an unpinned page.  Returns false only when ALL
// frames are pinned (system is thrashing).
// ============================================================================

bool BufferPool::FindVictim(frame_id_t* out_frame_id) {
    // 1. Free list has priority — avoids unnecessary I/O.
    {
        int free_frame = PopFromFree();
        if (free_frame != -1) {
            *out_frame_id = static_cast<frame_id_t>(free_frame);
            return true;
        }
    }

    // 2. All frames are in use: scan LRU for the least recently used unpinned page.
    for (int cur = lru_head_; cur != -1; cur = lru_nodes_[cur].next) {
        if (pages_[cur].pin_count_ == 0) {
            if (pages_[cur].is_dirty_) {
                disk_manager_->WritePage(pages_[cur].GetPageId(), pages_[cur].GetData());
            }
            page_table_.erase(pages_[cur].GetPageId());
            DetachFromLRU(cur);
            *out_frame_id = static_cast<frame_id_t>(cur);
            return true;
        }
    }
    return false;  // All frames pinned
}

// ============================================================================
// FetchPage — O(1) on cache hit, O(pool_size) on miss (victim scan).
// ============================================================================

Page* BufferPool::FetchPage(page_id_t page_id) {
    // --- Cache hit: O(1) path ---
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        frame_id_t frame_id = it->second;
        pages_[frame_id].pin_count_++;
        DetachFromLRU(frame_id);   // Remove from LRU while pinned (O(1))
        return &pages_[frame_id];
    }

    // --- Cache miss: find a victim frame ---
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

// ============================================================================
// NewPage — allocate a fresh page.  Delegates to FindVictim which tries the
// free list first, then evicts an unpinned LRU page.
// ============================================================================

Page* BufferPool::NewPage(page_id_t* out_page_id) {
    frame_id_t frame_id;
    if (!FindVictim(&frame_id)) return nullptr;

    page_id_t new_page_id = disk_manager_->AllocatePage();
    *out_page_id = new_page_id;

    Page* target_page = &pages_[frame_id];
    target_page->ResetMemory();
    target_page->page_id_ = new_page_id;
    target_page->pin_count_ = 1;
    target_page->is_dirty_ = false;

    page_table_[new_page_id] = frame_id;
    return target_page;
}

// ============================================================================
// UnpinPage — release a pin. If the page becomes unpinned, re-insert into
// the tail of the LRU list (most recently used position) so it is evictable.
// ============================================================================

bool BufferPool::UnpinPage(page_id_t page_id, bool is_dirty) {
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return false;

    frame_id_t frame_id = it->second;
    if (pages_[frame_id].pin_count_ <= 0) return false;

    pages_[frame_id].pin_count_--;
    if (is_dirty) pages_[frame_id].is_dirty_ = true;

    // Once unpinned, the page becomes eligible for eviction.
    if (pages_[frame_id].pin_count_ == 0) {
        AppendToLRU(frame_id);  // O(1), zero heap allocation
    }
    return true;
}
