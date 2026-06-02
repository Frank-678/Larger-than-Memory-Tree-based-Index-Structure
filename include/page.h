#pragma once
#include "include/common.h"

typedef struct page{
    static constexpr int cap = 40;
    std::vector<data_t> data; // 用array效率更高吗？
    int key1;
    int key2;
    
    page() : data(cap) {}
} page;  // 设定死的参数。
