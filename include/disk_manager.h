#pragma once
#include "common.h"
#include <fstream>
#include <string>

class DiskManager {
public:
    DiskManager(const std::string& file_name);
    ~DiskManager();

    // 必须整页读写，必须传入申请好的 4096 字节 buffer
    void ReadPage(page_id_t page_id, char* page_data);
    void WritePage(page_id_t page_id, const char* page_data);
    page_id_t AllocatePage();

private:
    std::fstream db_file_; // C++ 标准库中的文件流对象，其作用等价于 C 语言中的 FILE *，用于执行底层的物理文件读写操作
    std::string file_name_; // 当前绑定的本地物理文件的路径名称
    page_id_t next_page_id_; // 系统分配新页面时，提供递增的、不重复的逻辑页号
};