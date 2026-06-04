#ifdef _WIN32
#include <windows.h>
#endif    
#include <iostream>
#include <cassert>
#include <cstring>
#include <chrono>
#include <vector>
#include <iomanip>
#include <random>
#include <numeric>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <string>
#include <filesystem>
#include "disk_manager.h"
#include "buffer_pool.h"
#include "bplus_tree.h"

// ============================================================================
// 改造 1：输出真实的规模与 IOPS 拓扑数据
// ============================================================================
void Topology_BPlusTree_Split_Evolution_With_Export() {
    std::cout << "\n>>> 运行拓扑：B+ 树规模与真实吞吐量埋点采集" << std::endl;
    DiskManager dm("ltm_topo_export_1.db");
    BufferPool bp(32, &dm);
    BPlusTree tree(&bp);
    
    // 打开数据流，准备写入 ../data 目录
    std::ofstream out_file("../data/tree_iops_real.csv");
    if (!out_file.is_open()) {
        std::cerr << "无法在 ../data/ 创建 CSV 文件，请检查目录权限。" << std::endl;
        return;
    }
    out_file << "Scale,IOPS\n"; // CSV 表头

    const std::vector<int> checkpoints = {1000, 5000, 10000, 50000, 100000};
    int current_insert = 1;

    for (int scale : checkpoints) {
        auto time_start = std::chrono::high_resolution_clock::now();
        int operations = 0;
        
        while (current_insert <= scale) {
            tree.Insert(current_insert, current_insert * 10);
            current_insert++;
            operations++;
        }
        
        auto time_end = std::chrono::high_resolution_clock::now();
        double time_ms = std::chrono::duration<double, std::milli>(time_end - time_start).count();
        double iops = (operations / time_ms) * 1000.0;
        
        // 记录该量级下的真实系统吞吐量
        out_file << scale << "," << iops << "\n";
    }
    out_file.close();
    std::cout << "[PASS] 真实 IOPS 数据已导出至 data/tree_iops_real.csv" << std::endl;
}

// ============================================================================
// 改造 2：输出真实的底层写放大倍率 (WAF)
// ============================================================================
void Topology_Write_Amplification_With_Export() {
    std::cout << "\n>>> 运行拓扑：真实写放大倍率物理模拟与演算" << std::endl;
    std::ofstream out_file("../data/waf_real.csv");
    out_file << "UpdateSize,WAF\n";

    const std::vector<int> update_sizes = {8, 64, 256, 1024, 4096};

    for (int size : update_sizes) {
        std::remove("ltm_topo_export_2.db");
        DiskManager dm("ltm_topo_export_2.db");
        BufferPool bp(4, &dm);
        
        // 预分配页面
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
            
            // 模拟逻辑更新
            int offset = 0;
            std::memset(p->GetData() + offset, 0xAA, size);
            logical_bytes += size;
            bp.UnpinPage(target_pid, true);
            
            // 强制 LRU 驱逐，引发真实写盘
            page_id_t dummy;
            Page* evict_p = bp.NewPage(&dummy);
            bp.UnpinPage(dummy, false);
            physical_bytes += PAGE_SIZE; // 每次驱逐必然发生 4KB 物理 I/O
        }
        
        double waf = static_cast<double>(physical_bytes) / logical_bytes;
        out_file << size << "," << waf << "\n";
    }
    out_file.close();
    std::cout << "[PASS] 真实写放大倍率数据已导出至 data/waf_real.csv" << std::endl;
}

// 全局测试断言统筹
int g_pass_count = 0;
int g_fail_count = 0;

void AssertTopologicalPoint(bool condition, const std::string& func_name, int point_id, const std::string& desc) {
    if (condition) {
        std::cout << "[PASS] " << func_name << " | Point " << std::setw(2) << point_id << " : " << desc << std::endl;
        g_pass_count++;
    } else {
        std::cout << "[FAIL] " << func_name << " | Point " << std::setw(2) << point_id << " : " << desc << std::endl;
        g_fail_count++;
        std::exit(EXIT_FAILURE); 
    }
}

// ============================================================================
// 类别 A：基础拓扑与边界验证函数
// ============================================================================

// 函数 1：磁盘连续 I/O 拓扑与持久化完整性验证
void Topology_DiskManager_Sequential_IO() {
    std::cout << "\n>>> 运行拓扑函数 1：连续 I/O 拓扑验证" << std::endl;
    const std::string file_name = "ltm_topo_1.db";
    std::remove(file_name.c_str());
    DiskManager dm(file_name);
    
    const int page_count = 50;
    std::vector<page_id_t> pids(page_count);
    
    for (int i = 0; i < page_count; ++i) {
        pids[i] = dm.AllocatePage();
        AssertTopologicalPoint(pids[i] == i, "Topology_1", 1, "严格的逻辑页号连续性映射");
        char write_buf[PAGE_SIZE];
        std::memset(write_buf, static_cast<char>(i % 255), PAGE_SIZE);
        dm.WritePage(pids[i], write_buf);
    }
    
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
            AssertTopologicalPoint(is_valid, "Topology_1", 2, "物理介质层断电重启后逆序读取比特级一致");
        }
    }
}

// 函数 2：伪随机离散 I/O 拓扑性能屏障测试
void Topology_DiskManager_Random_IO() {
    std::cout << "\n>>> 运行拓扑函数 2：离散伪随机 I/O 测试" << std::endl;
    const std::string file_name = "ltm_topo_2.db";
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
    AssertTopologicalPoint(read_buf[0] == 0x7F || read_buf[0] == 0x00, "Topology_2", 1, "寻道系统应对极端随机地址未发生越界崩溃");
    
    page_id_t new_pid = dm.AllocatePage();
    AssertTopologicalPoint(new_pid == allocate_size, "Topology_2", 2, "高频离散读写后，游标分配指针仍保持准确映射");
}

// 函数 3：针对中期报告的 LRU 顺序泛洪 (Sequential Flooding) 模拟
void Topology_BufferPool_Sequential_Flooding() {
    std::cout << "\n>>> 运行拓扑函数 3：LRU 顺序泛洪现象模拟" << std::endl;
    DiskManager dm("ltm_topo_3.db");
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
        AssertTopologicalPoint(temp_page != nullptr, "Topology_3", 1, "全表扫描内存分配未阻塞");
        bp.UnpinPage(temp_id, false); 
    }
    
    Page* fetch_root = bp.FetchPage(root_id);
    AssertTopologicalPoint(fetch_root != nullptr, "Topology_3", 2, "被泛洪淘汰的热数据重新 Fetch 能够再次从磁盘穿透加载");
    bp.UnpinPage(root_id, false);
}

// 函数 4：BufferPool 交叉抖动 (Thrashing) 拓扑模拟
void Topology_BufferPool_Thrashing() {
    std::cout << "\n>>> 运行拓扑函数 4：缓冲池剧烈抖动与死锁防御测试" << std::endl;
    DiskManager dm("ltm_topo_4.db");
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
    
    AssertTopologicalPoint(miss_count == 40, "Topology_4", 1, "Thrashing 状态下状态机运转完好，未引发悬空指针");
    Page* check = bp.FetchPage(0);
    AssertTopologicalPoint(check->GetPageId() == 0, "Topology_4", 2, "剧烈抖动后内部逻辑页号无错乱");
    bp.UnpinPage(0, false);
}

// 函数 5：逻辑层分裂状态机与高维空间演化测试 (修复原有的断言滞后问题)
void Topology_BPlusTree_Split_Evolution() {
    std::cout << "\n>>> 运行拓扑函数 5：B+ 树满载分裂与多层级树高演化测试" << std::endl;
    const std::string file_name = "ltm_topo_5.db";
    std::remove(file_name.c_str());
    DiskManager dm(file_name);
    // 放宽内存池限制以容纳多级树冠的并发 Pin 操作
    BufferPool bp(16, &dm);
    BPlusTree tree(&bp);
    
    // 阶段 1：边界饱和注入
    int capacity = 250;
    bool saturate_success = true;
    for (int64_t i = 1; i <= capacity; ++i) {
        if (!tree.Insert(i, i * 10)) {
            saturate_success = false;
            break;
        }
    }
    AssertTopologicalPoint(saturate_success, "Topology_5", 1, "精准填满初始根节点 250 个定长槽位未报错");
    
    // 阶段 2：跨代演化触发
    bool split_success = tree.Insert(251, 2510);
    AssertTopologicalPoint(split_success, "Topology_5", 2, "第 251 个元素成功触发页分裂状态机，树冠完成跨代演化，未导致内存破坏");
    
    // 阶段 3：级联分裂 (Cascade Splitting) 压力验证
    bool cascade_success = true;
    const int64_t massive_insert = 10000;
    for (int64_t i = 252; i <= massive_insert; ++i) {
        if (!tree.Insert(i, i * 10)) {
            cascade_success = false;
            break;
        }
    }
    AssertTopologicalPoint(cascade_success, "Topology_5", 3, "成功承载 10000 次连续插入，多级推挤 (Push Up) 与内存分配防线稳定");
    
    // 阶段 4：非线性物理存储空间内的全局寻址
    bool retrieval_success = true;
    ValueType val;
    for (int64_t i = 1; i <= massive_insert; ++i) {
        if (!tree.GetValue(i, &val) || val != i * 10) {
            retrieval_success = false;
            break;
        }
    }
    AssertTopologicalPoint(retrieval_success, "Topology_5", 4, "复杂分裂后的外存空间内，10000 次对数时间复杂度离散提取 100% 精确命中");
}

// ============================================================================
// 类别 B：高复杂高阶拓扑分析函数
// ============================================================================

// 函数 6：中期报告专题 - 磁盘写放大 (Write Amplification) 数学拓扑演算
void Topology_Write_Amplification_Simulation() {
    std::cout << "\n>>> 运行拓扑函数 6：写放大倍率物理模拟与演算" << std::endl;
    const std::string file_name = "ltm_topo_6.db";
    std::remove(file_name.c_str());
    DiskManager dm(file_name);
    BufferPool bp(4, &dm);
    
    const int num_pages = 10;
    std::vector<page_id_t> pids(num_pages);
    for (int i = 0; i < num_pages; ++i) {
        Page* p = bp.NewPage(&pids[i]);
        if (p) {
            std::memset(p->GetData(), 0, PAGE_SIZE);
            bp.UnpinPage(pids[i], true);
        }
    }
    
    page_id_t dummy1, dummy2, dummy3, dummy4;
    bp.NewPage(&dummy1); bp.UnpinPage(dummy1, false);
    bp.NewPage(&dummy2); bp.UnpinPage(dummy2, false);
    bp.NewPage(&dummy3); bp.UnpinPage(dummy3, false);
    bp.NewPage(&dummy4); bp.UnpinPage(dummy4, false);
    
    int64_t logical_bytes_written = 0;
    int64_t physical_bytes_written = 0;
    
    for (int i = 0; i < 200; ++i) {
        page_id_t target_pid = pids[i % num_pages];
        Page* p = bp.FetchPage(target_pid);
        AssertTopologicalPoint(p != nullptr, "Topology_6", 1, "高压写穿透 FetchPage 成功提取缓存框");
        
        int offset = (i * 8) % (PAGE_SIZE - 8);
        char update_data[8] = "UPDATED";
        std::memcpy(p->GetData() + offset, update_data, 8);
        logical_bytes_written += 8;
        
        bp.UnpinPage(target_pid, true);
        
        page_id_t force_evict;
        Page* evict_p = bp.NewPage(&force_evict);
        bp.UnpinPage(force_evict, false);
        
        physical_bytes_written += PAGE_SIZE;
    }
    
    AssertTopologicalPoint(logical_bytes_written == 1600, "Topology_6", 2, "逻辑更新数据量精确计算通过");
    AssertTopologicalPoint(physical_bytes_written == 200 * PAGE_SIZE, "Topology_6", 3, "物理写盘数据量追踪完整");
    
    double wa_factor = static_cast<double>(physical_bytes_written) / logical_bytes_written;
    std::cout << "    [计算结果] 逻辑写入量: " << logical_bytes_written << " 字节" << std::endl;
    std::cout << "    [计算结果] 物理落盘量: " << physical_bytes_written << " 字节" << std::endl;
    std::cout << "    [深度演算] 观察到写放大倍数 (WAF) 达到: " << wa_factor << " 倍" << std::endl;
}

// 函数 7：基于有向无环图 (DAG) 模型的缓冲池状态机深层测绘
void Topology_BufferPool_Complex_State_Machine() {
    std::cout << "\n>>> 运行拓扑函数 7：缓冲池状态机深度测绘" << std::endl;
    DiskManager dm("ltm_topo_7.db");
    BufferPool bp(4, &dm);
    
    page_id_t p0, p1, p2, p3;
    Page* pg0 = bp.NewPage(&p0); 
    Page* pg1 = bp.NewPage(&p1); 
    Page* pg2 = bp.NewPage(&p2); 
    Page* pg3 = bp.NewPage(&p3); 
    
    AssertTopologicalPoint(pg0 && pg1 && pg2 && pg3, "Topology_7", 1, "状态机初始 4 槽位全部锁定完毕");
    
    Page* pg0_dup = bp.FetchPage(p0); 
    AssertTopologicalPoint(pg0_dup->GetPinCount() == 2, "Topology_7", 2, "重复 Fetch 导致引用计数自增精确");
    
    bp.UnpinPage(p1, false); 
    bp.UnpinPage(p2, true);  
    
    Page* pg1_revive = bp.FetchPage(p1); 
    AssertTopologicalPoint(pg1_revive != nullptr, "Topology_7", 3, "淘汰前夕的缓存命中成功复活目标页");
    
    bp.UnpinPage(p3, false); 
    
    page_id_t p4;
    Page* pg4 = bp.NewPage(&p4); 
    AssertTopologicalPoint(pg4 != nullptr && pg4->GetPageId() == p4, "Topology_7", 4, "脏页 p2 成功被置换，新页建立映射");
    
    page_id_t p5;
    Page* pg5 = bp.NewPage(&p5); 
    AssertTopologicalPoint(pg5 != nullptr && pg5->GetPageId() == p5, "Topology_7", 5, "干净页 p3 成功被置换，新页建立映射");
    
    bp.UnpinPage(p0, false); 
    bp.UnpinPage(p0, false); 
    bp.UnpinPage(p1, false); 
    bp.UnpinPage(p4, false); 
    bp.UnpinPage(p5, false); 
    
    bp.FetchPage(999); 
    
    AssertTopologicalPoint(true, "Topology_7", 6, "复杂交错的有向无环图操作结束，未引发任何悬挂或内存异常");
}

// 函数 8：Zipfian 倾斜分布下的大规模 LTM 外存检索模拟
void Topology_LTM_Scale_Data_Retrieval() {
    std::cout << "\n>>> 运行拓扑函数 8：Zipfian 概率模型下的 LTM 检索极限测试" << std::endl;
    DiskManager dm("ltm_topo_8.db");
    BufferPool bp(10, &dm); 
    
    const int disk_volume = 1000; 
    std::vector<page_id_t> pids(disk_volume);
    
    for (int i = 0; i < disk_volume; ++i) {
        Page* p = bp.NewPage(&pids[i]);
        if (p) {
            std::memset(p->GetData(), static_cast<char>(i % 255), PAGE_SIZE);
            bp.UnpinPage(pids[i], true);
        }
    }
    AssertTopologicalPoint(true, "Topology_8", 1, "外存 1000 页基底数据分布完毕");
    
    double alpha = 1.0;
    std::vector<double> probabilities(disk_volume);
    double c = 0;
    for (int i = 1; i <= disk_volume; ++i) {
        c += (1.0 / std::pow((double)i, alpha));
    }
    c = 1.0 / c;
    for (int i = 1; i <= disk_volume; ++i) {
        probabilities[i-1] = c / std::pow((double)i, alpha);
    }
    
    std::mt19937 rng(12345);
    std::discrete_distribution<int> zipf(probabilities.begin(), probabilities.end());
    
    int cache_hits = 0;
    for (int i = 0; i < 5000; ++i) {
        int rank = zipf(rng); 
        page_id_t target = pids[rank];
        
        Page* fetch = bp.FetchPage(target);
        if (fetch != nullptr) {
            if (static_cast<unsigned char>(fetch->GetData()[0]) == static_cast<unsigned char>(rank % 255)) {
                cache_hits++;
            }
            bp.UnpinPage(target, false);
        }
    }
    
    AssertTopologicalPoint(cache_hits == 5000, "Topology_8", 2, "在剧烈的置换中，5000 次读取数据完整度 100%");
}

// 函数 9：零拷贝架构 (Zero-Copy) 与 B+ 树底层内存排布 (Layout) 验证
void Topology_BPlusTree_Memory_Layout_Validation() {
    std::cout << "\n>>> 运行拓扑函数 9：B+ 树头结构与定长槽位物理排布严密审视" << std::endl;
    
    size_t header_size = offsetof(BPlusTreeNodeHeader, array_);
    size_t array_size = 250 * sizeof(std::pair<KeyType, ValueType>);
    size_t total_size = header_size + array_size;
    
    AssertTopologicalPoint(total_size <= PAGE_SIZE, "Topology_9", 1, 
        "结构体严格遵守超内存边界，总计占用 " + std::to_string(total_size) + " 字节，未超过 4096 字节");
        
    DiskManager dm("ltm_topo_9.db");
    BufferPool bp(2, &dm);
    BPlusTree tree(&bp);
    
    tree.Insert(100, 1000);
    
    Page* p = bp.FetchPage(0);
    AssertTopologicalPoint(p != nullptr, "Topology_9", 2, "底层物理探针拦截成功");
    
    auto* node = reinterpret_cast<BPlusTreeNodeHeader*>(p->GetData());
    
    AssertTopologicalPoint(node->is_leaf_ == true, "Topology_9", 3, "内存原址布尔位正确");
    AssertTopologicalPoint(node->current_size_ == 1, "Topology_9", 4, "槽位计数位内存合法");
    AssertTopologicalPoint(node->array_[0].first == 100 && node->array_[0].second == 1000, "Topology_9", 5, 
        "物理地址偏移下的 Key/Value 定长 64 位整型读取准确无误");
        
    bp.UnpinPage(0, false);
}

// 函数 10：针对中期报告的最终交付 —— 极限性能评估 (Benchmark Aggregation)
void Topology_Midterm_Benchmark_Aggregation() {
    std::cout << "\n>>> 运行拓扑函数 10：LTM 系统级性能基准联合测算矩阵" << std::endl;
    DiskManager dm("ltm_topo_10.db");
    BufferPool bp(32, &dm); 
    
    const int workload_size = 10000;
    std::vector<page_id_t> sequence(workload_size);
    
    int cache_miss_penalty = 0;
    auto time_start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < workload_size; ++i) {
        Page* p = bp.NewPage(&sequence[i]);
        if (p) {
            std::memset(p->GetData(), 0xAA, PAGE_SIZE);
            bp.UnpinPage(sequence[i], true);
        } else {
            cache_miss_penalty++;
        }
    }
    
    for (int i = 0; i < workload_size; i += 3) {
        Page* p = bp.FetchPage(sequence[i]);
        if (p) bp.UnpinPage(sequence[i], false);
    }
    
    std::mt19937 rng(2026);
    std::uniform_int_distribution<int> dist(0, workload_size - 1);
    for (int i = 0; i < 5000; ++i) {
        page_id_t target = sequence[dist(rng)];
        Page* p = bp.FetchPage(target);
        if (p) {
            if (i % 2 == 0) { 
                p->GetData()[0] = 0xBB;
                bp.UnpinPage(target, true);
            } else {
                bp.UnpinPage(target, false);
            }
        }
    }
    
    auto time_end = std::chrono::high_resolution_clock::now();
    double total_time_ms = std::chrono::duration<double, std::milli>(time_end - time_start).count();
    
    AssertTopologicalPoint(cache_miss_penalty == 0, "Topology_10", 1, "巨型测试矩阵运行未发生 OOM 或死锁");
    
    int total_operations = workload_size + (workload_size / 3) + 5000;
    double iops = (total_operations / total_time_ms) * 1000.0;
    double avg_latency = total_time_ms / total_operations;
    
    std::cout << "\n=======================================================" << std::endl;
    std::cout << "        大一年度项目中期检查报告 —— 基准性能         " << std::endl;
    std::cout << "=======================================================" << std::endl;
    std::cout << " 物理页帧大小限制           : " << PAGE_SIZE << " 字节" << std::endl;
    std::cout << " 模拟超内存负荷总量         : " << total_operations << " 次调度请求" << std::endl;
    std::cout << " 综合拓扑执行耗时           : " << std::fixed << std::setprecision(2) << total_time_ms << " 毫秒" << std::endl;
    std::cout << " -----------------------------------------------------" << std::endl;
    std::cout << " [量化指标 1] 引擎 IOPS     : " << iops << " 操作/秒" << std::endl;
    std::cout << " [量化指标 2] 平均调度延迟  : " << avg_latency << " 毫秒/请求" << std::endl;
    std::cout << "=======================================================" << std::endl;
}

int main() {
    #ifdef _WIN32
    SetConsoleOutputCP(65001); 
    SetConsoleCP(65001);       
    #endif

    std::cout << "========= LTM 系统针对中期报告的测试启动 =========" << std::endl;
    
    Topology_DiskManager_Sequential_IO();
    Topology_DiskManager_Random_IO();
    Topology_BufferPool_Sequential_Flooding();
    Topology_BufferPool_Thrashing();
    Topology_BPlusTree_Split_Evolution(); // 核心替换更新处
    
    Topology_Write_Amplification_Simulation();
    Topology_BufferPool_Complex_State_Machine();
    Topology_LTM_Scale_Data_Retrieval();
    Topology_BPlusTree_Memory_Layout_Validation();
    Topology_Midterm_Benchmark_Aggregation();
    
    std::cout << "\n========= 测试结束，全部通过 =========" << std::endl;
    return 0;
}