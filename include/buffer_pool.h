#pragma once
#include "common.h"
#include "page.h"
#include "disk_manager.h"
#include <unordered_map>

// Intrusive doubly-linked list node for LRU/free lists.
// Indexed by frame_id_t (0..pool_size_-1), so frame_id IS the array index
// into lru_nodes_[]. This eliminates O(N) std::list::remove scans and avoids
// heap allocation per LRU operation.
struct LRUNode {
    int prev;  // previous node index (-1 = none)
    int next;  // next node index (-1 = none)
};

class BufferPool {
public:
    BufferPool(size_t pool_size, DiskManager* disk_manager);
    ~BufferPool();

    Page* FetchPage(page_id_t page_id);
    Page* NewPage(page_id_t* out_page_id);
    bool UnpinPage(page_id_t page_id, bool is_dirty);

private:
    bool FindVictim(frame_id_t* out_frame_id);

    // Intrusive linked-list helpers — all O(1), zero heap allocation.
    void DetachFromLRU(int frame_id);
    void AppendToLRU(int frame_id);
    int  PopFromFree();
    void PushToFree(int frame_id);

    size_t pool_size_;
    DiskManager* disk_manager_;
    Page* pages_;  // heap-allocated page frames[pool_size_]

    std::unordered_map<page_id_t, frame_id_t> page_table_;  // page_id -> frame_id

    LRUNode* lru_nodes_;  // pre-allocated array[pool_size_]; frame_id = array index

    int free_head_;  // head of free-list  (-1 = empty)
    int free_tail_;  // tail of free-list
    int lru_head_;   // head of LRU list (least recently used, -1 = empty)
    int lru_tail_;   // tail of LRU list (most recently used, -1 = empty)
};
