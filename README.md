# Larger-than-Memory B+ Tree Index Engine

基于 B+ 树的磁盘感知索引引擎，专为 **Larger-than-Memory** 场景设计。当索引数据超过物理内存容量时，通过缓冲池与磁盘管理器的配合，透明地在内存与磁盘之间调度页面，使上层 B+ 树算法无需感知页面在内存还是磁盘上。

---

## 近期优化

### 1. LRU 缓存管理改用侵入式链表

原实现使用 `std::list<frame_id_t>` 管理 LRU 顺序，每次缓存命中时调用 `lru_list_.remove(frame_id)` 执行线性遍历。当缓冲池较大时，本应纳秒级完成的缓存命中操作退化为耗时的全局扫描，且 `std::list` 的节点插入删除伴随堆内存分配（malloc/free），在微秒级测时中引入方差抖动。

优化方案：预分配 `LRUNode[pool_size_]` 数组，利用 `frame_id` 天然为数组下标的特性，通过 `DetachFromLRU(frame_id)` 和 `AppendToLRU(frame_id)` 进行纯索引操作的链表剥离与重组。所有 LRU 操作从 O(N) 降为 O(1)，且**完全消除堆内存分配**。

### 2. AllocatePage 同步扩展物理文件

原有 `AllocatePage()` 仅在内存中递增计数器，不对物理文件做扩展。若程序在分配后、脏页写回前崩溃，重启后通过 `file_size / PAGE_SIZE` 恢复的 `next_page_id_` 将发生回退，导致新分配的页 ID 与历史记录碰撞，破坏 B+ 树拓扑结构。

优化方案：`AllocatePage` 在分配逻辑 ID 的同时，向物理文件对应偏移写入单字节以完成空间预留，借助操作系统稀疏文件机制将 I/O 开销降至最低。新增崩溃恢复正确性测试，验证重启后页 ID 的连续性与数据完整性。

### 3. fstream 状态机防御性保护

I/O 操作失败后 C++ 标准库会将流内部的 `failbit` 置位，导致后续所有读写调用被静默拦截。原实现未对 `std::fstream` 的状态进行重置。

优化方案：所有 `ReadPage`/`WritePage`/`AllocatePage` 操作前调用 `db_file_.clear()` 重置流状态，`ReadPage` 添加 `offset >= file_size_` 边界检查并返回空数据而非静默传播错误。

---

## 三层架构

```
┌──────────────────────────────────────────────────────────┐
│                     BPlusTree                             │
│  键值对插入 / 查找 / 更新，节点分裂，树高维护             │
│  只与 BufferPool 交互，不感知磁盘                          │
├──────────────────────────────────────────────────────────┤
│                     BufferPool                            │
│  LRU 淘汰策略，pin/unpin 语义，脏页跟踪                    │
│  页表映射：page_id → (内存帧, pin计数, 脏标记)              │
│  透明缓存：对上层隐藏磁盘 I/O                               │
├──────────────────────────────────────────────────────────┤
│                     DiskManager                           │
│  4KB 固定页式读写，单调递增页 ID 分配                      │
│  文件级持久化，计算页面偏移量定位 I/O                       │
└──────────────────────────────────────────────────────────┘
```

每层只与相邻层交互，职责单一：

| 层 | 依赖 | 职责 |
|----|------|------|
| `BPlusTree` | → `BufferPool` | 树算法：搜索、插入、分裂 |
| `BufferPool` | → `DiskManager` | 缓存管理：页面调度、淘汰、并发控制 |
| `DiskManager` | → 文件系统 | 物理 I/O：4KB 页式读写 |

---

## 基础参数

| 参数 | 值 | 说明 |
|------|-----|------|
| **页大小** | 4096 字节 (4KB) | 与文件系统簇大小对齐 |
| **每页槽位数** | 250 | 固定长度 `std::pair<int64_t, int64_t>` |
| **节点扇出** | 250 | 内部节点最多 250 个子指针 |
| **理论树高** | log₂₅₀(N) | 百万级数据只需 3 层 I/O |

单页内 250 个槽位的计算公式：

```
sizeof(Header) + 250 * sizeof(pair<int64_t, int64_t>) ≤ 4096
Header = (is_leaf, current_size, max_size, next_page_id) = 4 + 4 + 4 + 4 = 16 字节
Slots = 250 * 16 = 4000 字节
总计 = 16 + 4000 = 4016 ≤ 4096  ✓
```

---

## 第 1 层：DiskManager — 磁盘管理器

### 职责

- 以 4KB 固定大小为单位进行文件读写
- 维护单调递增的页 ID 分配器
- 通过文件偏移量直接定位页面

### 核心接口

```cpp
class DiskManager {
    void ReadPage(page_id_t page_id, char* page_data);   // 读一整页
    void WritePage(page_id_t page_id, const char* page_data); // 写一整页
    page_id_t AllocatePage();  // 返回新页 ID，单调递增
};
```

### 设计要点

**页式 I/O 模型**

无论实际修改多少字节，磁盘始终按整页（4KB）读写。页 ID 到文件偏移的映射：

```
偏移量 = page_id × 4096
```

**启动恢复**

打开已有文件时，通过文件大小计算已有页面数，恢复 `next_page_id_`：

```cpp
file_size = 文件字节数
next_page_id_ = file_size / PAGE_SIZE
```

`AllocatePage()` 在返回新页 ID 的同时，向物理文件末尾写入单字节以预留空间（利用操作系统稀疏文件机制避免全量零填充）。这保证了即使进程在分配后、写回前崩溃，重启后 `next_page_id_` 仍能正确恢复，不会出现页 ID 回退碰撞。

所有 I/O 函数（`ReadPage`、`WritePage`、`AllocatePage`）执行前均调用 `db_file_.clear()` 重置流状态，防止前次 I/O 失败遗留的 `failbit` 导致后续操作静默失效。`ReadPage` 同时进行 `offset >= file_size_` 边界检查，越界读取时返回空数据而非使流进入错误状态。

### 数据结构

| 成员 | 类型 | 说明 |
|------|------|------|
| `db_file_` | `std::fstream` | 数据库文件流 |
| `file_name_` | `std::string` | 文件路径 |
| `next_page_id_` | `page_id_t` | 下一个可用页 ID，从 0 开始单调递增 |
| `file_size_` | `std::streampos` | 缓存的物理文件大小（字节） |

---

## 第 2 层：BufferPool — 缓冲池

### 职责

- 在内存中缓存最近访问的磁盘页面
- 对上层隐藏磁盘 I/O 细节（透明缓存）
- 使用 LRU 策略淘汰不常用的页面
- 通过 pin/unpin 机制防止正在使用的页面被淘汰
- 追踪脏页，淘汰时自动写回磁盘

### 核心接口

```cpp
class BufferPool {
    Page* FetchPage(page_id_t page_id);    // 获取页面（缓存命中→直接返回，未命中→从磁盘加载）
    Page* NewPage(page_id_t* out_page_id); // 分配并获取一个新页面
    bool UnpinPage(page_id_t page_id, bool is_dirty); // 释放页面（标记是否脏）
};
```

### 设计要点

**缓存命中路径**

```
FetchPage(page_id)
  → page_table_ 命中?
    → 是: pin_count++，从 LRU 链表移除，返回内存指针
    → 否: FindVictim() 寻找淘汰页面 → 从磁盘加载 → 加入 page_table_ → 返回
```

**LRU 淘汰算法**

`lru_nodes_` 是一个预分配的 `LRUNode[pool_size_]` 数组，通过数组下标（即 `frame_id`）定位节点，使用 `prev`/`next` 索引维护双向链表顺序：

- **链表头**（`lru_head_`）：最久未使用的页面（下次淘汰候选）
- **链表尾**（`lru_tail_`）：最近刚 unpin 的页面（刚使用完）

缓存命中时调用 `DetachFromLRU(frame_id)` 以 O(1) 时间将节点从 LRU 链表剥离（防止被淘汰）；Unpin 后调用 `AppendToLRU(frame_id)` 以 O(1) 时间将节点追加至链表尾部。所有操作均为纯数组索引运算，无堆内存分配。

淘汰时从头扫描 LRU 链表，找到第一个 `pin_count == 0` 的帧：

```cpp
for (int cur = lru_head_; cur != -1; cur = lru_nodes_[cur].next) {
    if (pages_[cur].pin_count_ == 0) {
        if (pages_[cur].is_dirty_)
            disk_manager_->WritePage(...);  // 脏页回写
        page_table_.erase(page_id);
        return 该帧;
    }
}
```

**Pin/Unpin 语义**

- `FetchPage`/`NewPage` 成功后，pin_count 自动 +1
- `UnpinPage` 使 pin_count -1
- pin_count > 0 的页面不会被淘汰，保证上层正在使用的页面安全
- 当 pin_count 降为 0 时，页面加入 LRU 链表尾部，成为可淘汰候选

**脏页跟踪**

- `UnpinPage(page_id, is_dirty=true)` 标记页面为脏
- 脏页在淘汰时自动写回磁盘（延迟写）
- 同一页多次写脏后只回写一次

**空闲帧管理**

构造函数中将所有帧按顺序链入空闲链表（`free_head_` → `free_tail_`）。`FindVictim()` 优先调用 `PopFromFree()` 从空闲列表头部取出帧，空闲列表用尽后才触发 LRU 淘汰。

### Page 结构

```cpp
class Page {
    char data_[4096];          // 4KB 数据缓冲区
    page_id_t page_id_;        // 当前加载的页 ID
    int pin_count_;            // pin 计数，>0 禁止淘汰
    bool is_dirty_;            // 自上次回写后是否有修改
};
```

### BufferPool 数据结构

| 成员 | 类型 | 说明 |
|------|------|------|
| `pages_` | `Page[]` | 堆上分配的物理页面数组（固定大小 `pool_size_`） |
| `page_table_` | `unordered_map<page_id_t, frame_id_t>` | 页号到帧号的映射 |
| `lru_nodes_` | `LRUNode[]` | 预分配的侵入式链表节点数组，下标即 frame_id |
| `free_head_` | `int` | 空闲链表头节点索引（-1 表示空） |
| `free_tail_` | `int` | 空闲链表尾节点索引 |
| `lru_head_` | `int` | LRU 链表头节点索引（最久未使用，-1 表示空） |
| `lru_tail_` | `int` | LRU 链表尾节点索引（最近刚 unpin） |

---

## 第 3 层：BPlusTree — B+ 树

### 职责

- 提供标准的键值对插入和查找接口
- 维护 B+ 树的平衡结构（自动分裂）
- 使用 BufferPool 读写节点，不直接接触磁盘

### 核心接口

```cpp
class BPlusTree {
    bool Insert(const KeyType& key, const ValueType& value);  // 插入键值对
    bool GetValue(const KeyType& key, ValueType* result);     // 按 key 查找
    int GetHeight() const;                                     // 返回树高
};
```

### 节点结构

每个 B+ 树节点对应一个 4KB 物理页：

```cpp
struct BPlusTreeNodeHeader {
    bool is_leaf_;                              // 是否为叶子节点
    int32_t current_size_;                      // 当前已使用的槽位数
    int32_t max_size_;                          // 最大槽位数（固定 250）
    page_id_t next_page_id_;                    // 叶子层下一个兄弟节点（仅叶子使用）
    std::pair<KeyType, ValueType> array_[250];  // 固定长度槽位数组
};
```

**内部节点 vs 叶子节点**

| | 叶子节点 | 内部节点 |
|--|---------|---------|
| `is_leaf_` | true | false |
| `array_[i]` | key → value | key → child_page_id |
| `next_page_id_` | 指向右兄弟叶子 | 无效 (INVALID_PAGE_ID) |
| `array_[0]` | 最小键值对 | 最左子指针（key 部分忽略） |

内部节点中，`array_[0].second` 存储最左子树的页 ID，`array_[0].first` 不使用。对于 `i ≥ 1`，`array_[i].first` 是子树中的最小 key，`array_[i].second` 是对应子树的页 ID。

### 查找算法 (GetValue)

```
1. 从根节点开始
2. 对每个内部节点：
   a. 二分查找找到目标 key 所在的子节点
   b. Unpin 当前节点，Fetch 子节点（提前释放父节点锁——crabbing 模式）
3. 达到叶子节点后，二分查找目标 key
4. 返回找到的值或 false
```

二分查找的关键实现：

```cpp
int FindChildIndex(BPlusTreeNodeHeader* node, const KeyType& key) {
    // 在内部节点的 array_[1..current_size_-1] 中二分查找
    // 返回最大的 i 使得 array_[i].first ≤ key
    // 返回 0 表示 key 小于所有键（走最左子树）
}
```

查找过程的时间复杂度为 O(log₂₅₀(N) × log₂₅₀(250)) = O(log N × log 250)，即树高乘以节点内二分查找。

### 插入算法 (Insert)

```
1. 空树 → 创建根叶子节点，直接插入，返回

2. 从根向下遍历，记录路径（path vector）：
   - 每经过一个内部节点，将其页 ID 压入 path
   - 直到到达目标叶子节点

3. 在叶子节点中检查重复键（B+ 树不允许重复 key）

4. 在排序数组中插入新键值对（使用 InsertAt 保持有序）

5. 如果节点未满（current_size_ < max_size_），直接返回

6. 如果节点已满 → 分裂 (split)：
   a. 将节点后半部分移入新节点
   b. 叶子分裂：复制最小 key 到父节点
   c. 内部节点分裂：上推最小 key 到父节点
   d. 如果 path 非空，取父节点继续检查是否满
   e. 如果 path 为空（分裂传播到根），创建新根节点
```

分裂过程的关键代码路径：

```
Leaf split (is_leaf=true):
  split_idx = current_size_ / 2
  move_count = current_size_ - split_idx
  新节点复制 array_[split_idx .. current_size_-1]
  原节点保留 array_[0 .. split_idx-1]
  up_key = 新节点的最小 key（array_[0].first）
  新节点的 next_page_id_ = 原节点的 next_page_id_
  原节点的 next_page_id_ = 新节点

Internal node split (is_leaf=false):
  同上分裂，但不维护兄弟指针（内部节点无 next_page_id_）
  
Root split (path为空):
  创建新根节点，is_leaf_=false
  array_[0] = {0, 原节点}
  array_[1] = {up_key, 新节点}
  current_size_ = 2
```

### 树高演化

扇出 = 250，因此：

| 数据量 N | 树高 | 计算 |
|----------|------|------|
| 1 ~ 250 | 1 | 根叶子节点即可容纳 |
| 251 ~ 62,500 | 2 | 根+叶子，250² = 62,500 |
| 62,501 ~ 15,625,000 | 3 | 根+中间层+叶子，250³ ≈ 1,560 万 |
| 15,625,001 ~ 3.9×10⁹ | 4 | 250⁴ ≈ 39 亿 |

相比二叉搜索树（log₂(N) ≈ 20 层@1M），B+ 树在对磁盘友好的同时将 I/O 次数从 20 次降低到 3 次。

### Crabbing 协议

查找和插入过程中，父节点 unpin 后才 fetch 子节点：

```cpp
// 父节点用完即放，减少缓存压力
buffer_pool_->UnpinPage(curr_id, false);
curr_id = next_id;
page = buffer_pool_->FetchPage(curr_id);
```

这样在任意时刻，调用线程最多持有 1 个页面的 pin，防止死锁并最大限度减少对 LRU 的干扰。

---

## 关键设计决策

### 为什么选择 4KB 页？

1. **与文件系统对齐**：大多数文件系统的簇大小为 4KB，避免读-改-写
2. **与磁盘扇区对齐**：现代 SSD 的页大小多为 4KB/8KB/16KB
3. **内部分裂代价适中**：分裂一个 4KB 节点只需复制 ~2KB 数据

### 为什么每页 250 个槽位？

| 槽位数 | 槽位总大小 | +Header | 剩余 |
|--------|-----------|---------|------|
| 249 | 249×16=3984 | 4000 | 96 |
| **250** | **250×16=4000** | **4016** | **80** |
| 251 | 251×16=4016 | 4032 | 64 |

250 是 4KB 页内刚好容纳的最大整数，空间利用率最高。

### 为什么用 LRU 而非 Clock / LFU？

- B+ 树访问具有明显的时间局部性（同一节点短时间内可能被反复访问）
- LRU 实现简单，在页式缓存中表现良好
- Clock 算法在处理大工作集时与 LRU 性能接近，但实现更复杂
- LFU 不适合 B+ 树场景（根节点访问频率远高于叶子，但叶子被淘汰代价更高）

### 为什么不需要并发控制？

本实现为单线程设计。多线程场景下需要在 BufferPool 层面加锁（每个页面的 latche），在 BPlusTree 层面加耦合锁（crabbing 协议天然支持并发）。这是后续扩展的方向。

### 为什么要区分 NewPage 和 FetchPage？

- `NewPage` 分配**全新的页面**（之前不存在），直接使用空闲帧无需读磁盘
- `FetchPage` 加载**已有页面**，可能需要从磁盘读取
- 对 B+ 树而言，新建节点调 `NewPage`，访问已有节点调 `FetchPage`

---

## 文件结构

```
项目根目录/
├── include/
│   ├── common.h              # 类型定义、常量
│   ├── page.h                # Page 类（4KB 缓冲区）
│   ├── disk_manager.h        # DiskManager 声明
│   ├── buffer_pool.h         # BufferPool 声明
│   └── bplus_tree.h          # BPlusTree 声明 + BPlusTreeNodeHeader
├── src/
│   ├── main.cpp              # 入口点：运行测试和基准
│   ├── disk_manager.cpp      # 磁盘 I/O 实现
│   ├── buffer_pool.cpp       # LRU 缓存实现
│   ├── bplus_tree.cpp        # B+ 树算法实现
│   ├── correctness_tests.cpp # 正确性验证
│   └── benchmark_tests.cpp   # 性能基准测试
├── benchmark_visualizer.py   # Python 可视化脚本
├── run.bat                   # Windows 一键运行
├── scripts/run.sh            # Linux/Mac 一键运行
├── CMakeLists.txt            # CMake 构建配置
├── build/                    # 编译输出目录
└── data/                     # 生成的数据与图表
```

### 源文件依赖

```
main.cpp
  ├── correctness_tests.cpp   (通过 test_helpers.h 断言)
  └── benchmark_tests.cpp     (调用 BPlusTree API)
         ├── bplus_tree.cpp   (依赖 BufferPool)
         │     └── buffer_pool.cpp (依赖 DiskManager)
         │           └── disk_manager.cpp
         └── ... (直接使用 BufferPool 的基准)
```

---

## 构建与运行

### 编译器要求

- GCC 7.0+ / Clang 5.0+ / MSVC 2017+（需 C++17 支持）
- CMake 3.10+（可选，可用 g++ 直接编译）

### 方式 1：一键脚本

```bash
# Windows
run.bat

# Linux / macOS
bash scripts/run.sh
```

### 方式 2：手动编译

```bash
g++ -std=c++17 -O2 -I include src/*.cpp -o build/ltm_index
```

`-O2` 优化级别对性能影响显著——基准测试中 `-O2` 比 `-O0` 快 5-10 倍。

### 方式 3：CMake

```bash
cd build
cmake ..
cmake --build . --config Release
```

### 方式 4：完整基准流水线

```bash
# Windows
run.bat
```

该脚本依次执行：
1. CMake 构建（Release）
2. 正确性测试（98 个断言点）
3. 性能基准测试（7 个测试，重复机制：1 次预热 + 3 次稳态测量，大规模为 1+2）
4. 自动生成 CSV 数据到 `data/` 目录
5. 调用 `benchmark_visualizer.py` 生成 PNG 图表到同一目录

**预热隔离（Warmup Isolation）**：所有重复测量的基准测试在首次计时前均执行预热循环（Cache
Warm-up），使 CPU 睿频、分支预测器和缓存行进入饱和稳态。图表中的均值与标准差基于稳态
（Warmup 之后）的计算结果，冷启动暂态以虚线单独绘制以揭示硬件预热物理现象。

### 可视化

```bash
python benchmark_visualizer.py
```

读取 `data/` 目录下的 CSV 文件，生成 PNG 图表（150dpi）到同一目录。

---

## Benchmark 分析

所有基准测试的详细数据分析（含完整数据表、嵌入式图表、多层次多角度解析）请参见：

- [**README_BENCHMARK_ANALYSIS.md**](./README_BENCHMARK_ANALYSIS.md) — 含 7 个基准的全部数据与深度分析
- [**QUICK_START.md**](./QUICK_START.md) — 快速开始与操作指南
