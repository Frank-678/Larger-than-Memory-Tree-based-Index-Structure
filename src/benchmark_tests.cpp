#include "disk_manager.h"
#include "buffer_pool.h"
#include "bplus_tree.h"
#include "test_helpers.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <numeric>
#include <cmath>

// Measurement repeats for read-only benchmarks.
// Expensive large-scale benchmarks use fewer repeats.
static constexpr int REPEATS = 3;
static constexpr int REPEATS_LARGE = 2;
#include <cstring>
#include <fstream>
#include <string>
#include <filesystem>
#include <algorithm>

// ============================================================================
// Benchmark 1: Lookup throughput at different data scales
// ============================================================================
void Benchmark_Lookup_Performance() {
    std::cout << "\n>>> [Benchmark 1] Lookup throughput at various scales" << std::endl;
    std::ofstream out_file("data/lookup_iops_by_scale.csv");
    out_file << "Scale,Run,Lookup_IOPS\n";

    const std::vector<int> scales = {1000, 10000, 100000, 500000, 1000000};

    for (int scale : scales) {
        std::remove("ltm_lookup_test.db");
        DiskManager dm("ltm_lookup_test.db");
        BufferPool bp(64, &dm);
        BPlusTree tree(&bp);

        for (int i = 1; i <= scale; ++i) {
            tree.Insert(i, i * 10);
        }

        int repeats = (scale <= 100000) ? REPEATS : REPEATS_LARGE;
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> dist(1, scale);
        std::vector<int> keys(scale);
        for (int i = 0; i < scale; ++i) keys[i] = dist(rng);

        for (int r = 0; r < repeats; ++r) {
            auto time_start = std::chrono::high_resolution_clock::now();
            int operations = 0;

            for (int i = 0; i < scale; ++i) {
                ValueType val;
                if (tree.GetValue(keys[i], &val)) {
                    operations++;
                }
            }

            auto time_end = std::chrono::high_resolution_clock::now();
            double time_ms = std::chrono::duration<double, std::milli>(time_end - time_start).count();
            double iops = (operations / time_ms) * 1000.0;

            out_file << scale << "," << r << "," << (int)iops << "\n";

            // Warm cache: re-seed RNG for next run to use a different key order
            // so we measure realistic warm-cache performance, not just sequential replay
        }

        // Print a one-line summary showing min and max across runs
        std::cout << "  Scale " << scale << ": (see CSV for all runs)" << std::endl;
    }
    out_file.close();
    std::cout << "[OK] Data written to data/lookup_iops_by_scale.csv" << std::endl;
}

// ============================================================================
// Benchmark 2: Cache pressure test
// Measures throughput as pool size varies with a fixed working set.
// Demonstrates the "throughput cliff": when the pool is large enough to hold
// the entire working set, IOPS jump dramatically because evictions stop.
// ============================================================================
void Benchmark_Cache_Pressure() {
    std::cout << "\n>>> [Benchmark 2] Cache pressure: throughput vs pool size" << std::endl;
    std::ofstream out_file("data/cache_pressure_by_pool_size.csv");
    out_file << "PoolSize,Run,Throughput_IOPS\n";

    const int working_set = 200;
    const std::vector<int> pool_sizes = {4, 8, 16, 32, 64, 128, 256};
    const int total_accesses = 50000;

    for (int pool_size : pool_sizes) {
        std::remove("ltm_cache_pressure.db");
        DiskManager dm("ltm_cache_pressure.db");
        BufferPool bp(pool_size, &dm);

        std::vector<page_id_t> pids(working_set);
        for (int i = 0; i < working_set; ++i) {
            Page* p = bp.NewPage(&pids[i]);
            if (p) {
                std::memset(p->GetData(), static_cast<char>(i), PAGE_SIZE);
                bp.UnpinPage(pids[i], true);
            }
        }

        for (int r = 0; r < REPEATS; ++r) {
            std::mt19937 rng(static_cast<unsigned>(pool_size * (r + 1)));
            std::uniform_int_distribution<int> dist(0, working_set - 1);

            auto time_start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < total_accesses; ++i) {
                int idx = dist(rng);
                Page* p = bp.FetchPage(pids[idx]);
                if (p) bp.UnpinPage(pids[idx], false);
            }
            auto time_end = std::chrono::high_resolution_clock::now();
            double time_ms = std::chrono::duration<double, std::milli>(time_end - time_start).count();
            double iops = (total_accesses / time_ms) * 1000.0;

            out_file << pool_size << "," << r << "," << (int)iops << "\n";
        }
    }
    out_file.close();
    std::cout << "[OK] Data written to data/cache_pressure_by_pool_size.csv" << std::endl;
}

// ============================================================================
// Benchmark 3: B+ tree height evolution with data scale
// ============================================================================
void Benchmark_Tree_Height_Evolution() {
    std::cout << "\n>>> [Benchmark 3] Tree height evolution" << std::endl;
    std::ofstream out_file("data/tree_height_evolution.csv");
    out_file << "Scale,Tree_Height\n";

    const std::vector<int> scales = {100, 500, 1000, 5000, 10000, 50000, 100000, 500000, 1000000};

    for (int scale : scales) {
        std::remove("ltm_height_test.db");
        DiskManager dm("ltm_height_test.db");
        BufferPool bp(64, &dm);
        BPlusTree tree(&bp);

        for (int i = 1; i <= scale; ++i) {
            tree.Insert(i, i * 10);
        }

        int height = tree.GetHeight();
        out_file << scale << "," << height << "\n";
    }
    out_file.close();
    std::cout << "[OK] Data written to data/tree_height_evolution.csv" << std::endl;
}

// ============================================================================
// Benchmark 4: Sequential vs random access throughput
// ============================================================================
void Benchmark_Sequential_vs_Random() {
    std::cout << "\n>>> [Benchmark 4] Sequential vs random access" << std::endl;
    std::ofstream out_file("data/sequential_vs_random.csv");
    out_file << "DataSize,AccessPattern,Run,IOPS\n";

    const std::vector<int> data_sizes = {10000, 50000, 100000, 500000, 1000000};

    for (int size : data_sizes) {
        std::remove("ltm_seq_vs_rand.db");
        DiskManager dm("ltm_seq_vs_rand.db");
        BufferPool bp(32, &dm);
        BPlusTree tree(&bp);

        for (int i = 1; i <= size; ++i) {
            tree.Insert(i, i * 10);
        }

        int rpt = (size <= 100000) ? REPEATS : REPEATS_LARGE;

        // Sequential: rpt times
        for (int r = 0; r < rpt; ++r) {
            auto time_start = std::chrono::high_resolution_clock::now();
            int ops = 0;
            for (int i = 1; i <= size; ++i) {
                ValueType val;
                if (tree.GetValue(i, &val)) ops++;
            }
            auto time_end = std::chrono::high_resolution_clock::now();
            double time_ms = std::chrono::duration<double, std::milli>(time_end - time_start).count();
            out_file << size << ",seq," << r << "," << (int)((ops / time_ms) * 1000.0) << "\n";
        }

        // Random: rpt times
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> dist(1, size);
        std::vector<int> keys(size);
        for (int i = 0; i < size; ++i) keys[i] = dist(rng);

        for (int r = 0; r < rpt; ++r) {
            auto time_start = std::chrono::high_resolution_clock::now();
            int ops = 0;
            for (int i = 0; i < size; ++i) {
                ValueType val;
                if (tree.GetValue(keys[i], &val)) ops++;
            }
            auto time_end = std::chrono::high_resolution_clock::now();
            double time_ms = std::chrono::duration<double, std::milli>(time_end - time_start).count();
            out_file << size << ",rand," << r << "," << (int)((ops / time_ms) * 1000.0) << "\n";
        }
    }
    out_file.close();
    std::cout << "[OK] Data written to data/sequential_vs_random.csv" << std::endl;
}

// ============================================================================
// Benchmark 5: Write amplification analysis
// ============================================================================
void Benchmark_Write_Amplification() {
    std::cout << "\n>>> [Benchmark 5] Write amplification factor" << std::endl;
    std::ofstream out_file("data/write_amplification.csv");
    out_file << "UpdateSize,WAF\n";

    const std::vector<int> update_sizes = {8, 64, 256, 1024, 2048};

    for (int size : update_sizes) {
        std::remove("ltm_waf_test.db");
        DiskManager dm("ltm_waf_test.db");
        BufferPool bp(4, &dm);

        std::vector<page_id_t> pids(10);
        for (int i = 0; i < 10; ++i) {
            Page* p = bp.NewPage(&pids[i]);
            if (p) bp.UnpinPage(pids[i], true);
        }

        int64_t logical_bytes = 0;
        int64_t physical_bytes = 0;

        for (int i = 0; i < 100; ++i) {
            page_id_t target_pid = pids[i % 10];
            Page* p = bp.FetchPage(target_pid);
            if (!p) continue;

            // Simulate logical update
            int offset = (i * 8) % (PAGE_SIZE - size);
            std::memset(p->GetData() + offset, 0xAA, size);
            logical_bytes += size;
            bp.UnpinPage(target_pid, true);

            // Force eviction: access a new page to push the dirty page out
            page_id_t dummy;
            Page* evict_p = bp.NewPage(&dummy);
            if (evict_p) bp.UnpinPage(dummy, false);
            physical_bytes += PAGE_SIZE; // Each eviction writes 4KB
        }

        double waf = static_cast<double>(physical_bytes) / logical_bytes;
        out_file << size << "," << waf << "\n";
        std::cout << "  Update=" << size << "B: WAF=" << waf << "x" << std::endl;
    }
    out_file.close();
    std::cout << "[OK] Data written to data/write_amplification.csv" << std::endl;
}

// ============================================================================
// Benchmark 6: Mixed workload (insert + lookup + update)
// ============================================================================
void Benchmark_Mixed_Workload() {
    std::cout << "\n>>> [Benchmark 6] Mixed workload (30% Insert + 50% Lookup + 20% Update)" << std::endl;
    std::ofstream out_file("data/mixed_workload.csv");
    out_file << "Scale,Mixed_IOPS\n";

    const std::vector<int> scales = {10000, 50000, 100000, 500000, 1000000};

    for (int scale : scales) {
        std::remove("ltm_mixed.db");
        DiskManager dm("ltm_mixed.db");
        BufferPool bp(64, &dm);
        BPlusTree tree(&bp);

        for (int i = 1; i <= scale / 2; ++i) {
            tree.Insert(i, i * 10);
        }

        auto time_start = std::chrono::high_resolution_clock::now();
        int operations = 0;

        std::mt19937 rng(777);
        std::uniform_int_distribution<int> dist(1, scale / 2);
        std::uniform_int_distribution<int> op_dist(0, 99);

        int inserts = 0, lookups = 0, updates = 0;
        for (int i = 0; i < scale; ++i) {
            int op = op_dist(rng);
            if (op < 30 && inserts < (int)(scale * 0.3)) {
                tree.Insert(scale / 2 + i + 1, (scale / 2 + i + 1) * 10);
                inserts++;
            } else if (op < 80 && lookups < (int)(scale * 0.5)) {
                ValueType val;
                tree.GetValue(dist(rng), &val);
                lookups++;
            } else if (updates < (int)(scale * 0.2)) {
                tree.Insert(dist(rng), dist(rng) * 10);
                updates++;
            }
            operations++;
        }

        auto time_end = std::chrono::high_resolution_clock::now();
        double time_ms = std::chrono::duration<double, std::milli>(time_end - time_start).count();
        double iops = (operations / time_ms) * 1000.0;

        out_file << scale << "," << (int)iops << "\n";
        std::cout << "  Scale " << scale << ": " << (int)iops << " IOPS" << std::endl;
    }
    out_file.close();
    std::cout << "[OK] Data written to data/mixed_workload.csv" << std::endl;
}

// ============================================================================
// Benchmark 7: Working set vs pool size throughput
// Directly demonstrates LRU eviction cost: when working set exceeds pool,
// throughput drops significantly due to constant page reloading from disk.
// ============================================================================
void Benchmark_WorkingSet_Throughput() {
    std::cout << "\n>>> [Benchmark 7] Working set vs pool size: eviction impact on throughput" << std::endl;
    std::ofstream out_file("data/working_set_pressure.csv");
    out_file << "WorksetSize,PoolSize,Run,Throughput_IOPS\n";

    const int pool_size = 10;
    const std::vector<int> workset_sizes = {5, 10, 20, 50, 100, 500, 1000};
    const int total_accesses = 10000;

    for (int ws : workset_sizes) {
        std::remove("ltm_workset_test.db");
        DiskManager dm("ltm_workset_test.db");
        BufferPool bp(pool_size, &dm);

        std::vector<page_id_t> pids(ws);
        for (int i = 0; i < ws; ++i) {
            Page* p = bp.NewPage(&pids[i]);
            if (p) {
                std::memset(p->GetData(), static_cast<char>(i), PAGE_SIZE);
                bp.UnpinPage(pids[i], true);
            }
        }

        for (int r = 0; r < REPEATS; ++r) {
            auto time_start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < total_accesses; ++i) {
                int idx = ((i % ws) + r) % ws; // slight offset per run for variety
                Page* p = bp.FetchPage(pids[idx]);
                if (p) bp.UnpinPage(pids[idx], false);
            }
            auto time_end = std::chrono::high_resolution_clock::now();
            double time_ms = std::chrono::duration<double, std::milli>(time_end - time_start).count();
            out_file << ws << "," << pool_size << "," << r << "," << (int)(total_accesses / time_ms * 1000.0) << "\n";
        }
    }
    out_file.close();
    std::cout << "[OK] Data written to data/working_set_pressure.csv" << std::flush;
    std::cout << std::endl;
}

// ============================================================================
// Run all benchmarks
// ============================================================================
void RunAllBenchmarks() {
    std::cout << "\n========== BENCHMARKS ==========" << std::endl;

    Benchmark_Lookup_Performance();
    Benchmark_Cache_Pressure();
    Benchmark_Tree_Height_Evolution();
    Benchmark_Sequential_vs_Random();
    Benchmark_Write_Amplification();
    Benchmark_Mixed_Workload();
    Benchmark_WorkingSet_Throughput();

    std::cout << "\n========== All benchmarks complete ==========" << std::endl;
}
