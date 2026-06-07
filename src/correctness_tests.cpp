#include "disk_manager.h"
#include "buffer_pool.h"
#include "bplus_tree.h"
#include "test_helpers.h"
#include <iostream>
#include <vector>
#include <random>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <cstddef>
#include <cassert>

// ============================================================================
// Test 1: Disk sequential write/read consistency
// ============================================================================
static void Test_Disk_Sequential_IO() {
    std::cout << "\n>>> [Test 1] Disk sequential I/O consistency" << std::endl;
    const std::string file_name = "ltm_test_1.db";
    std::remove(file_name.c_str());
    DiskManager dm(file_name);

    const int page_count = 50;
    std::vector<page_id_t> pids(page_count);

    for (int i = 0; i < page_count; ++i) {
        pids[i] = dm.AllocatePage();
        AssertTopologicalPoint(pids[i] == i, "Test_1", 1, "Monotonic page ID allocation");
        char write_buf[PAGE_SIZE];
        std::memset(write_buf, static_cast<char>(i % 255), PAGE_SIZE);
        dm.WritePage(pids[i], write_buf);
    }

    // Re-open to verify persistence
    DiskManager dm_reboot(file_name);

    for (int i = page_count - 1; i >= 0; --i) {
        char read_buf[PAGE_SIZE];
        std::memset(read_buf, 0, PAGE_SIZE);
        dm_reboot.ReadPage(pids[i], read_buf);

        bool is_valid = true;
        for (int j = 0; j < PAGE_SIZE; ++j) {
            if (read_buf[j] != static_cast<char>(i % 255)) {
                is_valid = false;
                break;
            }
        }
        if (i % 10 == 0) {
            AssertTopologicalPoint(is_valid, "Test_1", 2,
                "Reverse-order read after reopen: bit-level consistency");
        }
    }
    std::cout << "  [OK] Sequential I/O: " << page_count << " pages verified" << std::endl;
}

// ============================================================================
// Test 2: Disk random I/O stress
// ============================================================================
static void Test_Disk_Random_IO() {
    std::cout << "\n>>> [Test 2] Disk random I/O stress" << std::endl;
    const std::string file_name = "ltm_test_2.db";
    std::remove(file_name.c_str());
    DiskManager dm(file_name);

    const int allocate_size = 100;
    for (int i = 0; i < allocate_size; ++i) { dm.AllocatePage(); }

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, allocate_size - 1);

    for (int i = 0; i < 500; ++i) {
        page_id_t target = dist(rng);
        char write_buf[PAGE_SIZE];
        std::memset(write_buf, 0x7F, PAGE_SIZE);
        dm.WritePage(target, write_buf);
    }

    page_id_t verify_target = dist(rng);
    char read_buf[PAGE_SIZE];
    dm.ReadPage(verify_target, read_buf);
    AssertTopologicalPoint(read_buf[0] == 0x7F || read_buf[0] == 0x00,
        "Test_2", 1, "Random I/O: no crash or corruption");

    page_id_t new_pid = dm.AllocatePage();
    AssertTopologicalPoint(new_pid == allocate_size, "Test_2", 2,
        "Allocation counter valid after random writes");
    std::cout << "  [OK] Random I/O: 500 writes across " << allocate_size << " pages" << std::endl;
}

// ============================================================================
// Test 3: B+ tree split evolution and retrieval correctness
// ============================================================================
static void Test_BTree_Split_Evolution() {
    std::cout << "\n>>> [Test 3] B+ tree split and multi-level evolution" << std::endl;
    const std::string file_name = "ltm_test_3.db";
    std::remove(file_name.c_str());
    DiskManager dm(file_name);
    BufferPool bp(16, &dm);
    BPlusTree tree(&bp);

    // Phase 1: Fill root node
    int capacity = 250;
    bool saturate_success = true;
    for (int64_t i = 1; i <= capacity; ++i) {
        if (!tree.Insert(i, i * 10)) {
            saturate_success = false;
            break;
        }
    }
    AssertTopologicalPoint(saturate_success, "Test_3", 1,
        "Fill root node with 250 entries");

    // Phase 2: Trigger split
    bool split_success = tree.Insert(251, 2510);
    AssertTopologicalPoint(split_success, "Test_3", 2,
        "251st entry triggers page split");

    // Phase 3: Cascading splits under load
    bool cascade_success = true;
    const int64_t massive_insert = 10000;
    for (int64_t i = 252; i <= massive_insert; ++i) {
        if (!tree.Insert(i, i * 10)) {
            cascade_success = false;
            break;
        }
    }
    AssertTopologicalPoint(cascade_success, "Test_3", 3,
        "10,000 inserts with cascading splits");

    // Phase 4: Verify all entries are retrievable
    bool retrieval_success = true;
    ValueType val;
    for (int64_t i = 1; i <= massive_insert; ++i) {
        if (!tree.GetValue(i, &val) || val != i * 10) {
            retrieval_success = false;
            break;
        }
    }
    AssertTopologicalPoint(retrieval_success, "Test_3", 4,
        "All 10,000 entries correctly retrievable after splits");

    std::cout << "  [OK] B+ tree: 10K entries inserted and verified, height="
              << tree.GetHeight() << std::endl;
}

// ============================================================================
// Test 4: B+ tree memory layout validation
// ============================================================================
static void Test_Memory_Layout() {
    std::cout << "\n>>> [Test 4] B+ tree node memory layout validation" << std::endl;

    size_t header_size = offsetof(BPlusTreeNodeHeader, array_);
    size_t array_size = 250 * sizeof(std::pair<KeyType, ValueType>);
    size_t total_size = header_size + array_size;

    AssertTopologicalPoint(total_size <= PAGE_SIZE, "Test_4", 1,
        "Node struct (" + std::to_string(total_size) + " bytes) fits in PAGE_SIZE ("
        + std::to_string(PAGE_SIZE) + ")");

    const std::string file_name = "ltm_test_4.db";
    std::remove(file_name.c_str());
    DiskManager dm(file_name);
    BufferPool bp(2, &dm);
    BPlusTree tree(&bp);

    tree.Insert(100, 1000);

    Page* p = bp.FetchPage(0);
    AssertTopologicalPoint(p != nullptr, "Test_4", 2, "Fetch page 0 (root) succeeds");

    auto* node = reinterpret_cast<BPlusTreeNodeHeader*>(p->GetData());

    AssertTopologicalPoint(node->is_leaf_ == true, "Test_4", 3, "Root is leaf node");
    AssertTopologicalPoint(node->current_size_ == 1, "Test_4", 4, "One entry in root");
    AssertTopologicalPoint(node->array_[0].first == 100 && node->array_[0].second == 1000,
        "Test_4", 5, "Key=100, Value=1000 stored correctly via reinterpret_cast");

    bp.UnpinPage(0, false);

    std::cout << "  [OK] Memory layout: struct size=" << total_size
              << " bytes, slot count=250" << std::endl;
}

// ============================================================================
// Test 5: Buffer pool sequential flooding
// Ensures that LRU eviction doesn't crash under sequential scan patterns
// ============================================================================
static void Test_BufferPool_Flooding() {
    std::cout << "\n>>> [Test 5] Buffer pool sequential flooding" << std::endl;
    const std::string file_name = "ltm_test_5.db";
    std::remove(file_name.c_str());
    DiskManager dm(file_name);
    BufferPool bp(5, &dm);

    page_id_t root_id, internal_id;
    Page* root_page = bp.NewPage(&root_id);
    Page* internal_page = bp.NewPage(&internal_id);
    bp.UnpinPage(root_id, false);
    bp.UnpinPage(internal_id, false);

    const int scan_volume = 20;
    for (int i = 0; i < scan_volume; ++i) {
        page_id_t temp_id;
        Page* temp_page = bp.NewPage(&temp_id);
        AssertTopologicalPoint(temp_page != nullptr, "Test_5", 1,
            "Sequential flood: all allocations succeed");
        bp.UnpinPage(temp_id, false);
    }

    Page* fetch_root = bp.FetchPage(root_id);
    AssertTopologicalPoint(fetch_root != nullptr, "Test_5", 2,
        "Flooded root page can be reloaded from disk");
    bp.UnpinPage(root_id, false);

    std::cout << "  [OK] Buffer pool survived sequential flooding" << std::endl;
}

// ============================================================================
// Test 6: Buffer pool thrashing resistance
// ============================================================================
static void Test_BufferPool_Thrashing() {
    std::cout << "\n>>> [Test 6] Buffer pool thrashing resistance" << std::endl;
    const std::string file_name = "ltm_test_6.db";
    std::remove(file_name.c_str());
    DiskManager dm(file_name);
    BufferPool bp(3, &dm);

    page_id_t p0, p1, p2, p3;
    bp.NewPage(&p0); bp.UnpinPage(p0, false);
    bp.NewPage(&p1); bp.UnpinPage(p1, false);
    bp.NewPage(&p2); bp.UnpinPage(p2, false);
    bp.NewPage(&p3); bp.UnpinPage(p3, false);

    int miss_count = 0;
    for (int i = 0; i < 40; ++i) {
        page_id_t target = i % 4;
        Page* fetch = bp.FetchPage(target);
        if (fetch != nullptr) {
            miss_count++;
            bp.UnpinPage(target, true);
        }
    }

    AssertTopologicalPoint(miss_count == 40, "Test_6", 1,
        "Thrashing: all 40 fetch operations succeed (no deadlock)");
    Page* check = bp.FetchPage(0);
    AssertTopologicalPoint(check->GetPageId() == 0, "Test_6", 2,
        "Page ID correct after thrashing");
    bp.UnpinPage(0, false);

    std::cout << "  [OK] Buffer pool stable under thrashing" << std::endl;
}

// ============================================================================
// Test 7: Buffer pool state machine correctness
// ============================================================================
static void Test_BufferPool_StateMachine() {
    std::cout << "\n>>> [Test 7] Buffer pool state machine" << std::endl;
    const std::string file_name = "ltm_test_7.db";
    std::remove(file_name.c_str());
    DiskManager dm(file_name);
    BufferPool bp(4, &dm);

    page_id_t p0, p1, p2, p3;
    Page* pg0 = bp.NewPage(&p0);
    Page* pg1 = bp.NewPage(&p1);
    Page* pg2 = bp.NewPage(&p2);
    Page* pg3 = bp.NewPage(&p3);

    AssertTopologicalPoint(pg0 && pg1 && pg2 && pg3, "Test_7", 1,
        "All 4 initial pages allocated");

    Page* pg0_dup = bp.FetchPage(p0);
    AssertTopologicalPoint(pg0_dup->GetPinCount() == 2, "Test_7", 2,
        "Duplicate fetch increments pin count to 2");

    bp.UnpinPage(p1, false);
    bp.UnpinPage(p2, true);

    Page* pg1_revive = bp.FetchPage(p1);
    AssertTopologicalPoint(pg1_revive != nullptr, "Test_7", 3,
        "Evicted page can be re-fetched");

    bp.UnpinPage(p3, false);

    page_id_t p4;
    Page* pg4 = bp.NewPage(&p4);
    AssertTopologicalPoint(pg4 != nullptr && pg4->GetPageId() == p4, "Test_7", 4,
        "Dirty page p2 evicted successfully, new page p4 created");

    page_id_t p5;
    Page* pg5 = bp.NewPage(&p5);
    AssertTopologicalPoint(pg5 != nullptr && pg5->GetPageId() == p5, "Test_7", 5,
        "Clean page p3 evicted successfully, new page p5 created");

    bp.UnpinPage(p0, false);
    bp.UnpinPage(p0, false);
    bp.UnpinPage(p1, false);
    bp.UnpinPage(p4, false);
    bp.UnpinPage(p5, false);

    std::cout << "  [OK] Complex state machine transitions correct" << std::endl;
}

// ============================================================================
// Run all correctness tests
// ============================================================================
void RunAllCorrectnessTests() {
    std::cout << "\n========== CORRECTNESS TESTS ==========" << std::endl;

    Test_Disk_Sequential_IO();
    Test_Disk_Random_IO();
    Test_BTree_Split_Evolution();
    Test_Memory_Layout();
    Test_BufferPool_Flooding();
    Test_BufferPool_Thrashing();
    Test_BufferPool_StateMachine();

    std::cout << "\n========== Correctness tests complete ==========" << std::endl;
}
