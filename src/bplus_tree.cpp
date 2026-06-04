/*
    1.页面获取流程
    （1）获取页面 (Fetch/Pin)：
    B+ 树遍历索引需要特定页面时，向 BufferPool 发起请求（通过 page_id 或页号）。
    （2）命中与加载：
    命中：若页面已在内存中，直接返回内存地址。
    未命中：BufferPool 从磁盘读取该页，并将其加载至空闲缓存槽位中。若缓冲池已满，则执行替换算法淘汰冷数据页。
    （3）加锁保护 (Pin/Fix)：BufferPool 为请求页加锁，防止在 B+ 树遍历期间该页被意外换出。
    2.页面修改流程
    （1）修改脏页：获取页面后，B+ 树直接在内存中执行插入、删除或更新操作。
    （2）标记脏页：B+ 树完成修改后，需通知 BufferPool 将该页标记为“脏页”。
    （3）并发与事务控制：结合日志先行策略（WAL），页面释放后调用 unfix/unpin 解除锁定。
*/

#include "bplus_tree.h"
#include <vector>

namespace {
    // 采用二分检索获取下一层物理页号的索引偏移
    int FindChildIndex(BPlusTreeNodeHeader* node, const KeyType& key) {
        int left = 1, right = node->current_size_ - 1;
        int ans = 0; // 默认沉降至最左侧子树
        while (left <= right) {
            int mid = left + (right - left) / 2;
            if (key >= node->array_[mid].first) {
                ans = mid;
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }
        return ans;
    }

    // 内存地址块内的物理后移与有序插入
    void InsertAt(BPlusTreeNodeHeader* node, const KeyType& key, const ValueType& value) {
        int i = node->current_size_ - 1;
        // 若为内部节点，array_[0] 的位置属于独立指针区，绝对不可被覆盖或右移
        int limit = node->is_leaf_ ? 0 : 1; 
        while (i >= limit && node->array_[i].first > key) {
            node->array_[i + 1] = node->array_[i];
            i--;
        }
        node->array_[i + 1] = {key, value};
        node->current_size_++;
    }
}

BPlusTree::BPlusTree(BufferPool* buffer_pool) 
    : buffer_pool_(buffer_pool), root_page_id_(INVALID_PAGE_ID) {}

bool BPlusTree::GetValue(const KeyType& key, ValueType* result) {
    if (root_page_id_ == INVALID_PAGE_ID) return false;

    page_id_t curr_id = root_page_id_;
    Page* page = buffer_pool_->FetchPage(curr_id);
    auto* node = reinterpret_cast<BPlusTreeNodeHeader*>(page->GetData());

    // 严密执行 While 循环向下遍历内部节点，直到击穿至叶子层
    while (!node->is_leaf_) {
        int child_idx = FindChildIndex(node, key);
        page_id_t next_id = static_cast<page_id_t>(node->array_[child_idx].second);
        
        // 遵守蟹行（Crabbing）规则，安全释放上层锁定
        buffer_pool_->UnpinPage(curr_id, false); 
        
        curr_id = next_id;
        page = buffer_pool_->FetchPage(curr_id);
        node = reinterpret_cast<BPlusTreeNodeHeader*>(page->GetData());
    }

    // 抵达叶子节点边界，执行对数级别的时间复杂度查找
    bool found = false;
    int left = 0, right = node->current_size_ - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (node->array_[mid].first == key) {
            *result = node->array_[mid].second;
            found = true;
            break;
        } else if (node->array_[mid].first < key) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    // 退出树操作前，务必注销底层引用计数防线
    buffer_pool_->UnpinPage(curr_id, false);
    return found;
}

bool BPlusTree::Insert(const KeyType& key, const ValueType& value) {
    // 树冠不存在情况的物理初始化边界
    if (root_page_id_ == INVALID_PAGE_ID) {
        Page* root_page = buffer_pool_->NewPage(&root_page_id_);
        auto* root_node = reinterpret_cast<BPlusTreeNodeHeader*>(root_page->GetData());
        root_node->is_leaf_ = true;
        root_node->current_size_ = 1;
        root_node->max_size_ = 250;
        root_node->next_page_id_ = INVALID_PAGE_ID;
        root_node->array_[0] = {key, value};
        
        buffer_pool_->UnpinPage(root_page_id_, true);
        return true;
    }

    // 设置路径栈区空间，负责捕捉遍历途经的祖先节点，为后续满载情况下的向上分裂做准备
    std::vector<page_id_t> path;
    page_id_t curr_id = root_page_id_;
    Page* page = buffer_pool_->FetchPage(curr_id);
    auto* node = reinterpret_cast<BPlusTreeNodeHeader*>(page->GetData());

    // 1. O(log_m N) 复杂度向下遍历并推入状态栈
    while (!node->is_leaf_) {
        path.push_back(curr_id);
        int child_idx = FindChildIndex(node, key);
        page_id_t next_id = static_cast<page_id_t>(node->array_[child_idx].second);
        
        buffer_pool_->UnpinPage(curr_id, false);
        
        curr_id = next_id;
        page = buffer_pool_->FetchPage(curr_id);
        node = reinterpret_cast<BPlusTreeNodeHeader*>(page->GetData());
    }

    // 2. 检测是否存在主键冲突现象
    int left = 0, right = node->current_size_ - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (node->array_[mid].first == key) {
            buffer_pool_->UnpinPage(curr_id, false);
            return false; // 主键排他性约束，拦截插入
        } else if (node->array_[mid].first < key) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    // 3. 将新实体并入定长数组并修正槽位界限
    InsertAt(node, key, value);

    // 4. 若仍处在容积范围内，静默完成
    if (node->current_size_ < node->max_size_) {
        buffer_pool_->UnpinPage(curr_id, true);
        return true;
    }

    // 5. 触发内存分裂 (Physical Splitting) 状态机
    page_id_t node_id = curr_id;
    bool is_leaf = true;

    while (node->current_size_ >= node->max_size_) {
        page_id_t new_page_id;
        Page* new_page = buffer_pool_->NewPage(&new_page_id);
        auto* new_node = reinterpret_cast<BPlusTreeNodeHeader*>(new_page->GetData());
        
        new_node->is_leaf_ = is_leaf;
        new_node->max_size_ = node->max_size_;
        
        // 切割阵列内存，精确完成元素迁徙
        int split_idx = node->current_size_ / 2;
        int move_count = node->current_size_ - split_idx;
        
        for (int i = 0; i < move_count; ++i) {
            new_node->array_[i] = node->array_[split_idx + i];
        }
        new_node->current_size_ = move_count;
        node->current_size_ = split_idx;
        
        KeyType up_key;
        if (is_leaf) {
            new_node->next_page_id_ = node->next_page_id_;
            node->next_page_id_ = new_page_id;
            up_key = new_node->array_[0].first; // 叶子节点执行 Copy Up
        } else {
            up_key = new_node->array_[0].first; // 内部节点执行 Push Up
        }

        // 当前页组分裂结束，立刻解锁解除 I/O 牵制
        buffer_pool_->UnpinPage(node_id, true);
        buffer_pool_->UnpinPage(new_page_id, true);

        // 如果路径栈消耗殆尽，证明分裂冲击波已突破原树冠，必须架设新一层根节点
        if (path.empty()) {
            page_id_t new_root_id;
            Page* new_root_page = buffer_pool_->NewPage(&new_root_id);
            auto* new_root = reinterpret_cast<BPlusTreeNodeHeader*>(new_root_page->GetData());
            
            new_root->is_leaf_ = false;
            new_root->max_size_ = node->max_size_;
            new_root->current_size_ = 2;
            new_root->next_page_id_ = INVALID_PAGE_ID;
            
            new_root->array_[0] = {0, node_id};          // 路由左枝干
            new_root->array_[1] = {up_key, new_page_id}; // 路由右枝干
            
            root_page_id_ = new_root_id;
            buffer_pool_->UnpinPage(new_root_id, true);
            break; // 演化完成，阻断迭代
        } else {
            // 父层接收节点回填，继续检测链式分裂波段
            page_id_t parent_id = path.back();
            path.pop_back();
            
            Page* parent_page = buffer_pool_->FetchPage(parent_id);
            node = reinterpret_cast<BPlusTreeNodeHeader*>(parent_page->GetData());
            node_id = parent_id;
            is_leaf = false;
            
            InsertAt(node, up_key, new_page_id);
        }
    }

    // 中断退出循环时，防备最后触及的页面发生资源遗漏，补全资源释放协议
    if (node->current_size_ < node->max_size_) {
        buffer_pool_->UnpinPage(node_id, true);
    }

    return true;
}