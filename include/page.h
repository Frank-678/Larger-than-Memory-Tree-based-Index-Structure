#pragma once
#include "common.h"
#include <cstring>

class Page {
public:
    Page() { ResetMemory(); }
    
    // 返回 4096 字节物理内存起点
    inline char* GetData() { return data_; }
    inline page_id_t GetPageId() const { return page_id_; }
    inline int GetPinCount() const { return pin_count_; }
    inline bool IsDirty() const { return is_dirty_; }

private:
    friend class BufferPool;
    inline void ResetMemory() { std::memset(data_, 0, PAGE_SIZE); }

    char data_[PAGE_SIZE];                // 4KB raw data buffer
    page_id_t page_id_ = INVALID_PAGE_ID; // Logical page number loaded
    int pin_count_ = 0;                   // Non-zero prevents LRU eviction
    bool is_dirty_ = false;               // Modified since last disk write
};