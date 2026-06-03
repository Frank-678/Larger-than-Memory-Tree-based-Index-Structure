/*
    1.页面获取流程
    （1）获取页面 (Fetch/Pin)：
    B+ 树遍历索引需要特定页面时，向 BufferPool 发起请求（通过 page_id 或页号）。
    （2）命中与加载：
    命中：若页面已在内存中，直接返回内存地址。
    未命中：BufferPool 从磁盘读取该页，并将其加载至空闲缓存槽位中。若缓冲池已满，则执行替换算法（如 LRU 或 Clock）淘汰冷数据页。
    （3）加锁保护 (Pin/Fix)：BufferPool 为请求页加锁（Pin 或 Fix），防止在 B+ 树遍历期间该页被意外换出。
    2.页面修改流程
    （1）修改脏页：获取页面后，B+ 树直接在内存中执行插入、删除或更新操作。
    （2）标记脏页：B+ 树完成修改后，需通知 BufferPool 将该页标记为“脏页 (Dirty Page)”，以便后台线程稍后将其刷回磁盘。
    （3）并发与事务控制：结合日志先行策略（WAL），BufferPool 会确保 redo log 先于数据页落盘。页面释放后调用 unfix/unpin 解除锁定，供其他并发事务访问。
*/

#include "bplus_tree.h"

BPlusTree::BPlusTree(BufferPool* buffer_pool) 
    : buffer_pool_(buffer_pool), root_page_id_(INVALID_PAGE_ID) {}

bool BPlusTree::GetValue(const KeyType& key, ValueType* result) {
    if (root_page_id_ == INVALID_PAGE_ID) return false;

    Page* page = buffer_pool_->FetchPage(root_page_id_);
    auto* node = reinterpret_cast<BPlusTreeNodeHeader*>(page->GetData());

    // TODO: 实现 While 循环向下遍历内部节点的过程
    if (node->is_leaf_) {
        for (int i = 0; i < node->current_size_; ++i) {
            if (node->array_[i].first == key) {
                *result = node->array_[i].second;
                buffer_pool_->UnpinPage(root_page_id_, false);
                return true;
            }
        }
    }
    
    buffer_pool_->UnpinPage(root_page_id_, false);
    return false;
}

bool BPlusTree::Insert(const KeyType& key, const ValueType& value) {
    // 根节点不存在时的初始化
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

    // TODO: 
    // 1. 实现二分查找找到正确的叶子节点
    // 2. 将 (key, value) 插入定长数组并保持有序
    // 3. 严密检测 node->current_size_ == node->max_size_
    // 4. 若满载，申请 NewPage 进行一半数据的物理拆分 (Split)
    
    return false;
}
