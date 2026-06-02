#pragma once
#include "common.h"
#include "buffer_pool.h"
#include <utility>

// 基于定长槽位机制的 B+ 树物理面具
struct BPlusTreeNodeHeader {
    bool is_leaf_;
    int32_t current_size_;
    int32_t max_size_;
    page_id_t next_page_id_;
    // 物理分配 250 个定长槽位。总大小绝对受控在 4096 以内
    std::pair<KeyType, ValueType> array_[250]; 
};

class BPlusTree {
public:
    BPlusTree(BufferPool* buffer_pool);

    bool Insert(const KeyType& key, const ValueType& value);
    bool GetValue(const KeyType& key, ValueType* result);

private:
    BufferPool* buffer_pool_;
    page_id_t root_page_id_;
};