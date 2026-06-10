#pragma once
#include "common.h"
#include <fstream>
#include <string>

class DiskManager {
public:
    DiskManager(const std::string& file_name);
    ~DiskManager();

    // Must read/write exactly PAGE_SIZE bytes.  Caller owns the buffer.
    void ReadPage(page_id_t page_id, char* page_data);
    void WritePage(page_id_t page_id, const char* page_data);
    page_id_t AllocatePage();

private:
    std::fstream db_file_;   // File stream for physical I/O
    std::string file_name_;  // Path to the database file
    page_id_t next_page_id_; // Monotonically increasing page ID counter
    std::streampos file_size_; // Cached physical file size (bytes)
};
