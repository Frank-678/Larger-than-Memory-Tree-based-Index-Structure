#include "disk_manager.h"
#include <iostream>
#include <cstring>

DiskManager::DiskManager(const std::string& file_name) : file_name_(file_name), next_page_id_(0), file_size_(0) {
    db_file_.open(file_name_, std::ios::in | std::ios::out | std::ios::binary);

    if (!db_file_.is_open()) {
        // File doesn't exist: create it
        db_file_.open(file_name_, std::ios::out | std::ios::in | std::ios::trunc | std::ios::binary);
        if (!db_file_.is_open()) {
            std::cerr << "Fail to open/create file: " << file_name_ << std::endl;
            return;
        }
        next_page_id_ = 0;
        file_size_ = 0;
    } else {
        // File exists: compute next_page_id from file size
        db_file_.seekg(0, std::ios::end);
        file_size_ = db_file_.tellg();
        next_page_id_ = static_cast<page_id_t>(file_size_ / PAGE_SIZE);
        db_file_.seekg(0, std::ios::beg);
        db_file_.seekp(0, std::ios::beg);
    }
}

DiskManager::~DiskManager() {
    if (db_file_.is_open()) db_file_.close();
}

void DiskManager::ReadPage(page_id_t page_id, char* page_data) {
    // 1. Defensive state reset — prevent stale failbit from silently
    //    disabling subsequent I/O on this stream.
    db_file_.clear();

    std::streamoff offset = static_cast<std::streamoff>(page_id) * PAGE_SIZE;

    // 2. Strict boundary check.  AllocatePage is the sole owner of physical
    //    file extension; ReadPage must NEVER extend the file.  If the B+ tree
    //    requests a page ID that has no backing storage it is a logic error
    //    in the upper layer.
    if (offset >= file_size_) {
        std::cerr << "[DiskManager] ERROR: ReadPage out-of-bounds. page_id="
                  << page_id << ", file_size=" << file_size_ << std::endl;
        std::memset(page_data, 0, PAGE_SIZE);
        return;
    }

    db_file_.seekg(offset);
    db_file_.read(page_data, PAGE_SIZE);

    // 3. Last-resort I/O failure guard.
    if (!db_file_.good()) {
        std::cerr << "[DiskManager] I/O error on ReadPage. page_id="
                  << page_id << std::endl;
        db_file_.clear();
        std::memset(page_data, 0, PAGE_SIZE);
    }
}

void DiskManager::WritePage(page_id_t page_id, const char* page_data) {
    db_file_.clear();  // Defensive reset — never let a prior failbit propagate.

    std::streamoff offset = static_cast<std::streamoff>(page_id) * PAGE_SIZE;
    db_file_.seekp(offset);
    db_file_.write(page_data, PAGE_SIZE);
    db_file_.flush();

    // Update cached file size if we wrote past the previous end.
    if (offset + PAGE_SIZE > file_size_) {
        file_size_ = offset + PAGE_SIZE;
    }
}

page_id_t DiskManager::AllocatePage() {
    page_id_t new_pid = next_page_id_++;
    std::streamoff offset = static_cast<std::streamoff>(new_pid) * PAGE_SIZE;

    db_file_.clear();

    // Extend the physical file by writing a single byte at the end of the
    // new page's boundary.  This avoids a full 4 KB zero-fill write while
    // still reserving the logical address range.  On most OSes this creates
    // a sparse (hole) block — the actual 4 KB is allocated on first real
    // write — keeping allocation latency at a minimum.
    db_file_.seekp(offset + PAGE_SIZE - 1);
    db_file_.write("", 1);
    db_file_.flush();

    file_size_ = offset + PAGE_SIZE;
    return new_pid;
}
