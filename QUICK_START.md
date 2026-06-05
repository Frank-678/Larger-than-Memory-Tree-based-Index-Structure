# ⚡ 快速开始指南

## 一键执行完整数据生成与可视化流程

### 🪟 Windows 用户

**方式 1：直接运行批处理脚本**

```cmd
run.bat
```

**方式 2：在 PowerShell 中运行**

```powershell
.\run.bat
```

### 🐧 Linux / 🍎 macOS 用户

```bash
bash scripts/run.sh
```

---

## 预期输出

脚本会自动执行以下步骤，生成完整的数据与图表：

```
==============================================================
   LTM B+ Tree - Data Generation & Visualization
==============================================================

[1] Checking environment...         ✓ CMake, C++ compiler, Python found
[2] Creating data directories...    ✓ Ready
[3] Building C++ project...         ✓ Compilation succeeded
[4] Running tests and generating data...
    ✓ lookup_iops_by_scale.csv
    ✓ cache_hitrate_by_pool_size.csv
    ✓ tree_height_evolution.csv
    ✓ sequential_vs_random.csv
    ✓ dirty_pages_analysis.csv
    ✓ lru_eviction_analysis.csv
    ✓ mixed_workload.csv
[5] Generating visualization charts...
    ✓ cache_efficiency.png
    ✓ dashboard_summary.png
    ✓ dirty_pages_timeline.png
    ✓ io_pattern_comparison.png
    ✓ lru_eviction_analysis.png
    ✓ tree_height_evolution.png

==============================================================
   ✓ Pipeline completed successfully!
==============================================================
```

---

## 📂 输出文件位置

所有生成的文件都在 **`data/`** 目录中：

### 📊 CSV 数据文件（可用 Excel、Python、R 分析）
- `lookup_iops_by_scale.csv` — 查询性能随数据量变化
- `cache_hitrate_by_pool_size.csv` — 缓冲池大小与命中率
- `tree_height_evolution.csv` — B+ 树结构演化
- `sequential_vs_random.csv` — 顺序 vs 随机访问性能对比
- `dirty_pages_analysis.csv` — 脏页产生与清理成本
- `lru_eviction_analysis.csv` — LRU 淘汰频率分析
- `mixed_workload.csv` — 混合工作负载性能测试

### 📈 PNG 图表（150dpi，适合论文插入）
- `dashboard_summary.png` — 性能指标总览
- `cache_efficiency.png` — 缓冲池效率分析
- `tree_height_evolution.png` — 树高演化曲线
- `io_pattern_comparison.png` — I/O 模式对比
- `dirty_pages_timeline.png` — 脏页动态变化
- `lru_eviction_analysis.png` — LRU 淘汰分析

---

## 🔧 系统需求

### 最小配置
| 项目 | 要求 |
|------|------|
| **C++ 编译器** | GCC 7.0+ / MSVC 2017+ / Clang 5.0+ |
| **CMake** | 3.10+ |
| **Python** | 3.6+ |
| **磁盘空间** | ≥ 500MB（用于 1M 规模测试） |
| **内存** | ≥ 2GB（用于 1M 规模测试） |
| **运行时间** | ~5-10 分钟（包括 1M 规模基准） |

### 依赖检查
脚本会自动检查所有依赖。如果缺少某个工具，会显示错误信息：
```
ERROR: CMake not found!
ERROR: C++ compiler not found!
ERROR: Python not found!
```

---

## 📝 执行流程详解

### 1️⃣ 编译阶段（~30 秒）
```
[3] Building C++ project...
  └─ CMake configure
  └─ Compile 7 benchmark functions
  └─ Link & generate ltm_index.exe
```

### 2️⃣ 数据生成阶段（~3-5 分钟）
```
[4] Running tests and generating data...
  ├─ Topology_BPlusTree_Lookup_Performance()      → lookup_iops_by_scale.csv
  ├─ Topology_BufferPool_Hit_Rate_Analysis()      → cache_hitrate_by_pool_size.csv
  ├─ Topology_BPlusTree_Height_Evolution()        → tree_height_evolution.csv
  ├─ Topology_Sequential_vs_Random_Access()       → sequential_vs_random.csv
  ├─ Topology_Dirty_Pages_Analysis()              → dirty_pages_analysis.csv
  ├─ Topology_LRU_Eviction_Analysis()             → lru_eviction_analysis.csv
  └─ Topology_Mixed_Workload_Test()               → mixed_workload.csv
```

### 3️⃣ 可视化生成阶段（~10 秒）
```
[5] Generating visualization charts...
  └─ Python 读取 7 个 CSV 文件
  └─ matplotlib 生成 6 个图表
  └─ 所有图表保存为 PNG
```

---

## 🚀 进阶用法

### 修改测试规模

编辑 `src/main.cpp`，修改各函数的 `scales` 向量：

```cpp
// 例：修改 Topology_BPlusTree_Height_Evolution()
const std::vector<int> scales = {100, 500, 1000, 5000, 10000, 50000, 100000};
// 改为：
const std::vector<int> scales = {1000, 10000, 100000, 1000000};  // 大规模测试

// 重新运行
run.bat  // Windows
bash scripts/run.sh  # Linux/Mac
```

### 只运行某个功能

**仅编译：**
```cmd
cd build
cmake ..
cmake --build . --config Release
```

**仅生成数据（跳过重新编译）：**
```cmd
cd "项目根目录"
.\build\Release\ltm_index.exe
```

**仅生成图表（跳过数据生成）：**
```cmd
python benchmark_visualizer.py
```

---

## ❓ 常见问题

### Q1: 脚本执行失败，提示 "CMake not found"
**A:** 请安装 CMake：
- Windows: 从 https://cmake.org/download/ 下载安装
- macOS: `brew install cmake`
- Linux: `sudo apt install cmake`

### Q2: 运行时间很长，是否正常？
**A:** 是的，1M 规模的基准测试需要几分钟。可以修改 `src/main.cpp` 中的规模参数来加速。

### Q3: 图表怎么导入到 Word/PowerPoint？
**A:** 直接拖拽 PNG 文件到文档中即可（150dpi 分辨率适合学位论文）。

### Q4: CSV 数据怎么在 Excel 中打开？
**A:** 
1. 打开 Excel
2. 文件 → 打开 → 选择 `data/*.csv`
3. 导入向导中选择逗号作为分隔符

### Q5: 如何在 Python 中分析数据？
**A:** 参考示例：
```python
import pandas as pd
import matplotlib.pyplot as plt

# 读取数据
df = pd.read_csv('data/lookup_iops_by_scale.csv')
print(df)

# 绘制自定义图表
plt.plot(df['Scale'], df['Lookup_IOPS'], marker='o')
plt.xlabel('Data Scale')
plt.ylabel('IOPS')
plt.show()
```

---

## 📖 更多信息

完整文档请查看 **`README_CN.md`**

---

**版本**：1.0  
**最后更新**：2025年6月5日
