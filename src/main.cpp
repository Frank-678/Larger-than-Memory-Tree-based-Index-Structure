#ifdef _WIN32
#include <windows.h>
#endif    
#include <iostream>
#include <cassert>
#include "disk_manager.h"
#include "buffer_pool.h"
#include "bplus_tree.h"

int main() {
    #ifdef _WIN32
    SetConsoleOutputCP(65001); // 强制控制台输出为 UTF-8
    SetConsoleCP(65001);       // 强制控制台输入为 UTF-8
    #endif

    std::cout << "========= 测试，启动！ =========" << std::endl;

    // 1. 初始化物理环境。分配极小的物理内存（例如 3 页），测试超内存置换能力
    DiskManager disk_manager("data_test.db");
    BufferPool buffer_pool(3, &disk_manager);
    BPlusTree tree(&buffer_pool);

    // 2. 根节点直接插入测试
    std::cout << "[TEST] 尝试向 B+ 树根节点插入数据..." << std::endl;
    bool insert_status = tree.Insert(1001, 8888);
    assert(insert_status == true);

    // 3. 物理读出验证
    std::cout << "[TEST] 尝试通过强转指针从内存池提取数据..." << std::endl;
    ValueType result_val;
    bool find_status = tree.GetValue(1001, &result_val);
    
    assert(find_status == true);
    assert(result_val == 8888);

    std::cout << "========= 所有基准测试通过，底层物理调用链完好！ =========" << std::endl;
    return 0;
}