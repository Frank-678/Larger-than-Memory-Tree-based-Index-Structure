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
    std::fstream db_file_;  // File stream for physical I/O
    std::string file_name_; // Path to the database file
    page_id_t next_page_id_; // Monotonically increasing page ID counter
};