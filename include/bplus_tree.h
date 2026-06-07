#pragma once
#include "common.h"
#include "buffer_pool.h"
#include <utility>

// B+ tree node with fixed-length slot array that fits within a single 4KB page
struct BPlusTreeNodeHeader {
    bool is_leaf_;
    int32_t current_size_;
    int32_t max_size_;
    page_id_t next_page_id_;
    // 250 fixed-length slots; total size verified to fit within PAGE_SIZE
    std::pair<KeyType, ValueType> array_[250];
};

class BPlusTree {
public:
    BPlusTree(BufferPool* buffer_pool);

    bool Insert(const KeyType& key, const ValueType& value);
    bool GetValue(const KeyType& key, ValueType* result);
    int GetHeight() const;

private:
    BufferPool* buffer_pool_;
    page_id_t root_page_id_;
    int tree_height_ = 0;
};