#pragma once
#include "common.h"
#include "page.h"
#include "disk_manager.h"
#include <unordered_map>
#include <list>
#include <vector>

class BufferPool {
public:
    BufferPool(size_t pool_size, DiskManager* disk_manager);
    ~BufferPool();

    Page* FetchPage(page_id_t page_id);
    Page* NewPage(page_id_t* out_page_id);
    bool UnpinPage(page_id_t page_id, bool is_dirty);

private:
    bool FindVictim(frame_id_t* out_frame_id);

    size_t pool_size_;
    DiskManager* disk_manager_;
    Page* pages_; // 在堆上分配的物理页面数组

    std::unordered_map<page_id_t, frame_id_t> page_table_; // 页表映射
    std::list<frame_id_t> free_list_;                      // 空闲物理框链表
    std::list<frame_id_t> lru_list_;                       // LRU 置换链表
};