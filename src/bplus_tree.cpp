#include "bplus_tree.h"
#include <vector>

namespace {
    // Binary search to find the child index for a given key.
    // Internal nodes store pointers at array_[0] as the leftmost child,
    // so the search range starts at index 1. Returns 0 if key is smaller
    // than all keys in the node.
    int FindChildIndex(BPlusTreeNodeHeader* node, const KeyType& key) {
        int left = 1, right = node->current_size_ - 1;
        int ans = 0;
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

    // Shift elements right to maintain sorted order, then insert.
    // For internal nodes, index 0 is reserved for the leftmost child pointer,
    // so insertion starts from index 1.
    void InsertAt(BPlusTreeNodeHeader* node, const KeyType& key, const ValueType& value) {
        int i = node->current_size_ - 1;
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

    // Traverse internal nodes down to the leaf level
    while (!node->is_leaf_) {
        int child_idx = FindChildIndex(node, key);
        page_id_t next_id = static_cast<page_id_t>(node->array_[child_idx].second);

        // Release parent page early (crabbing pattern)
        buffer_pool_->UnpinPage(curr_id, false);

        curr_id = next_id;
        page = buffer_pool_->FetchPage(curr_id);
        node = reinterpret_cast<BPlusTreeNodeHeader*>(page->GetData());
    }

    // Binary search within the leaf node
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
    
    // Unpin leaf page before returning
    buffer_pool_->UnpinPage(curr_id, false);
    return found;
}

bool BPlusTree::Insert(const KeyType& key, const ValueType& value) {
    // Empty tree: create the root node
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

    // Record the traversal path for upward split propagation
    std::vector<page_id_t> path;
    page_id_t curr_id = root_page_id_;
    Page* page = buffer_pool_->FetchPage(curr_id);
    auto* node = reinterpret_cast<BPlusTreeNodeHeader*>(page->GetData());

    // 1. Traverse down to the leaf, recording the path
    while (!node->is_leaf_) {
        path.push_back(curr_id);
        int child_idx = FindChildIndex(node, key);
        page_id_t next_id = static_cast<page_id_t>(node->array_[child_idx].second);
        
        buffer_pool_->UnpinPage(curr_id, false);
        
        curr_id = next_id;
        page = buffer_pool_->FetchPage(curr_id);
        node = reinterpret_cast<BPlusTreeNodeHeader*>(page->GetData());
    }

    // 2. Check for duplicate key
    int left = 0, right = node->current_size_ - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (node->array_[mid].first == key) {
            buffer_pool_->UnpinPage(curr_id, false);
            return false;
        } else if (node->array_[mid].first < key) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    // 3. Insert into the sorted slot array
    InsertAt(node, key, value);

    // 4. If the node still has space, done
    if (node->current_size_ < node->max_size_) {
        buffer_pool_->UnpinPage(curr_id, true);
        return true;
    }

    // 5. Node is full: trigger split and propagate upward
    page_id_t node_id = curr_id;
    bool is_leaf = true;

    while (node->current_size_ >= node->max_size_) {
        page_id_t new_page_id;
        Page* new_page = buffer_pool_->NewPage(&new_page_id);
        auto* new_node = reinterpret_cast<BPlusTreeNodeHeader*>(new_page->GetData());
        
        new_node->is_leaf_ = is_leaf;
        new_node->max_size_ = node->max_size_;
        
        // Split: move half of the entries to the new node
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
            up_key = new_node->array_[0].first; // Copy up for leaf
        } else {
            up_key = new_node->array_[0].first; // Push up for internal
        }

        // Unpin both split nodes
        buffer_pool_->UnpinPage(node_id, true);
        buffer_pool_->UnpinPage(new_page_id, true);

        // If the path is empty, the split reached the root: create a new root
        if (path.empty()) {
            page_id_t new_root_id;
            Page* new_root_page = buffer_pool_->NewPage(&new_root_id);
            auto* new_root = reinterpret_cast<BPlusTreeNodeHeader*>(new_root_page->GetData());
            
            new_root->is_leaf_ = false;
            new_root->max_size_ = node->max_size_;
            new_root->current_size_ = 2;
            new_root->next_page_id_ = INVALID_PAGE_ID;
            
            new_root->array_[0] = {0, node_id};          // Left child
            new_root->array_[1] = {up_key, new_page_id}; // Right child

            root_page_id_ = new_root_id;
            buffer_pool_->UnpinPage(new_root_id, true);
            break;
        } else {
            // Propagate split upward: insert the split key into the parent
            page_id_t parent_id = path.back();
            path.pop_back();
            
            Page* parent_page = buffer_pool_->FetchPage(parent_id);
            node = reinterpret_cast<BPlusTreeNodeHeader*>(parent_page->GetData());
            node_id = parent_id;
            is_leaf = false;
            
            InsertAt(node, up_key, new_page_id);
        }
    }

    // Final unpin for the last node if the loop exited early
    if (node->current_size_ < node->max_size_) {
        buffer_pool_->UnpinPage(node_id, true);
    }

    return true;
}

int BPlusTree::GetHeight() const {
    if (root_page_id_ == INVALID_PAGE_ID) return 0;
    
    int height = 0;
    page_id_t curr_id = root_page_id_;
    Page* page = const_cast<BufferPool*>(buffer_pool_)->FetchPage(curr_id);
    auto* node = reinterpret_cast<BPlusTreeNodeHeader*>(page->GetData());
    
    while (!node->is_leaf_) {
        height++;
        page_id_t next_id = static_cast<page_id_t>(node->array_[0].second);
        const_cast<BufferPool*>(buffer_pool_)->UnpinPage(curr_id, false);
        
        curr_id = next_id;
        page = const_cast<BufferPool*>(buffer_pool_)->FetchPage(curr_id);
        node = reinterpret_cast<BPlusTreeNodeHeader*>(page->GetData());
    }
    
    height++; // Count the leaf level
    const_cast<BufferPool*>(buffer_pool_)->UnpinPage(curr_id, false);
    
    return height;
}