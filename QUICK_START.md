# 快速开始指南

## 一键执行测试流程

### Windows 用户

**方式 1：直接运行批处理脚本**

```cmd
run.bat
```

**方式 2：手动编译运行**

```cmd
g++ -std=c++17 -O2 -I include src/*.cpp -o build\ltm_index.exe
build\ltm_index.exe
python benchmark_visualizer.py
```

### Linux / macOS 用户

```bash
bash scripts/run.sh
```

或手动：

```bash
g++ -std=c++17 -O2 -I include src/*.cpp -o build/ltm_index
./build/ltm_index
python benchmark_visualizer.py
```

---

## 文档索引

| 文档 | 内容 |
|------|------|
| [README.md](./README.md) | B+ 树三层架构设计、各模块实现细节、构建指南 |
| [README_BENCHMARK_ANALYSIS.md](./README_BENCHMARK_ANALYSIS.md) | 7 项基准测试的深度分析：多层次多角度、完整数据表、嵌入式图表、方差分析 |
| [QUICK_START.md](./QUICK_START.md) | 快速开始指南（本文档） |

## 预期输出

脚本会自动执行以下步骤，生成完整的数据与图表：

```
==============================================================
   LTM B+ Tree - Data Generation & Visualization
==============================================================

[1] Checking environment...         CMake, C++ compiler, Python found
[2] Creating data directories...    Ready
[3] Building C++ project...         Compilation succeeded
[4] Running tests and generating data...
    Correctness: 94/94 passed
    Benchmarks:
    ✓ lookup_iops_by_scale.csv       (3/2 次重复)
    ✓ cache_pressure_by_pool_size.csv (3 次重复)
    ✓ tree_height_evolution.csv
    ✓ sequential_vs_random.csv       (3/2 次重复)
    ✓ write_amplification.csv
    ✓ mixed_workload.csv
    ✓ working_set_pressure.csv       (3 次重复)
[5] Generating visualization charts...
    ✓ iops_comparison.png
    ✓ cache_efficiency.png
    ✓ tree_height_evolution.png
    ✓ io_pattern_comparison.png
    ✓ waf_analysis.png
    ✓ working_set_pressure.png
    ✓ dashboard_summary.png

==============================================================
   Pipeline completed successfully!  0 failed
==============================================================
```

---

## 输出文件位置

所有生成的文件都在 **`data/`** 目录中。

### CSV 数据文件

| 文件 | 说明 |
|------|------|
| `lookup_iops_by_scale.csv` | 随机查找吞吐量 vs 数据规模（含重复运行列） |
| `cache_pressure_by_pool_size.csv` | 缓存压力：固定工作集 200 页，变 Pool 大小 |
| `tree_height_evolution.csv` | B+ 树高度随数据量的演化 |
| `sequential_vs_random.csv` | 顺序 vs 随机访问（含访问模式列） |
| `write_amplification.csv` | 写放大倍数随更新粒度变化 |
| `mixed_workload.csv` | 混合负载（30% 插入 + 50% 查找 + 20% 更新） |
| `working_set_pressure.csv` | 固定 Pool=10，变工作集大小 |

### PNG 图表（150dpi）

| 图表 | 内容 |
|------|------|
| `iops_comparison.png` | IOPS 对比（纯查找 + 混合负载，含重复运行） |
| `cache_efficiency.png` | 缓存压力趋势（标注工作集阈值） |
| `tree_height_evolution.png` | 树高演化 + 理论曲线 |
| `io_pattern_comparison.png` | 顺序 vs 随机（含重复运行 + 标准差色带） |
| `waf_analysis.png` | 写放大柱状图（标注 SSD/HDD 阈值） |
| `working_set_pressure.png` | 工作集压力曲线（标注 Pool 边界） |
| `dashboard_summary.png` | 4 图汇总仪表板 |

---

## 系统需求

### 最小配置

| 项目 | 要求 |
|------|------|
| **C++ 编译器** | GCC 7.0+ / MSVC 2017+ / Clang 5.0+（需 C++17） |
| **CMake** | 3.10+（可选，可用 g++ 直接编译） |
| **Python** | 3.6+（pandas, matplotlib, numpy） |
| **磁盘空间** | ≥ 500 MB（用于 1M 规模测试） |
| **内存** | ≥ 2 GB |
| **运行时间** | ~5-8 分钟（含 7 个基准测试 + 重复测量） |

---

## 执行流程详解

### 1️⃣ 编译阶段

```
源文件：
  src/main.cpp                → 入口点
  src/correctness_tests.cpp   → 7 个正确性测试
  src/benchmark_tests.cpp     → 7 个基准测试
  src/bplus_tree.cpp          → B+ 树核心
  src/buffer_pool.cpp         → 缓冲池核心
  src/disk_manager.cpp        → 磁盘管理器核心

编译命令：
  g++ -std=c++17 -O2 -I include src/*.cpp -o build/ltm_index.exe
```

### 2️⃣ 数据生成阶段

程序执行顺序：正确性测试（94 个断言点）→ 基准测试（7 个基准）

```
Benchmark_Lookup_Performance()       → lookup_iops_by_scale.csv
Benchmark_Cache_Pressure()           → cache_pressure_by_pool_size.csv
Benchmark_Tree_Height_Evolution()    → tree_height_evolution.csv
Benchmark_Sequential_vs_Random()     → sequential_vs_random.csv
Benchmark_Write_Amplification()      → write_amplification.csv
Benchmark_Mixed_Workload()           → mixed_workload.csv
Benchmark_WorkingSet_Throughput()    → working_set_pressure.csv
```

### 3️⃣ 可视化生成阶段

```
Python 读取 7 个 CSV 文件
  → plot_with_repeats() 绘制重复测量数据
  → matplotlib 生成 7 个图表
  → 所有图表保存为 data/*.png（150dpi）
```

---

## 更多用法

### 编译不运行

```bash
cd build && cmake .. && cmake --build . --config Release
```

### 仅生成数据（跳过编译）

```bash
build/ltm_index.exe      # Windows
./build/ltm_index        # Linux/Mac
```

### 仅生成图表（跳过数据生成）

```bash
python benchmark_visualizer.py
```

### 修改测试参数

编辑 `src/benchmark_tests.cpp`，修改各函数的参数向量。例如修改缓存压力测试的 Pool 大小：

```cpp
// 默认值
const std::vector<int> pool_sizes = {4, 8, 16, 32, 64, 128, 256};
// 改为更大范围
const std::vector<int> pool_sizes = {2, 4, 8, 16, 32, 64, 128, 256, 512};
```

重新编译运行即可生效。

### 调整重复测量次数

编辑 `src/benchmark_tests.cpp` 顶部的常量：

```cpp
static constexpr int REPEATS = 3;        // 小规模重复次数
static constexpr int REPEATS_LARGE = 2;  // 大规模重复次数
```
