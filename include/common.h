#pragma once
#include <cstdint>

// 物理页大小锁定为 4096 字节
constexpr int PAGE_SIZE = 4096;
using page_id_t = int32_t;
using frame_id_t = int32_t;
constexpr page_id_t INVALID_PAGE_ID = -1;

// 定长主键与定长数值
using KeyType = int64_t;
using ValueType = int64_t;