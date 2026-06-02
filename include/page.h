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

    char data_[PAGE_SIZE];                // 4KB 字节数组
    page_id_t page_id_ = INVALID_PAGE_ID; // 当前装载的逻辑页号
    int pin_count_ = 0;                   // 引用计数，非0不会被 LRU 淘汰
    bool is_dirty_ = false;               // 脏页标记
};