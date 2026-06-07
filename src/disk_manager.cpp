#include "disk_manager.h"
#include <iostream>

DiskManager::DiskManager(const std::string& file_name) : file_name_(file_name), next_page_id_(0) {
    db_file_.open(file_name_, std::ios::in | std::ios::out | std::ios::binary);
    
    if (!db_file_.is_open()) {
        // File doesn't exist: create it
        db_file_.open(file_name_, std::ios::out | std::ios::in | std::ios::trunc | std::ios::binary);
        if (!db_file_.is_open()) {
            std::cerr << "Fail to open/create file: " << file_name_ << std::endl;
            return;
        }
        next_page_id_ = 0;
    } else {
        // File exists: compute next_page_id from file size
        db_file_.seekg(0, std::ios::end);
        std::streampos file_size = db_file_.tellg();
        next_page_id_ = static_cast<page_id_t>(file_size / PAGE_SIZE);
        db_file_.seekg(0, std::ios::beg);
        db_file_.seekp(0, std::ios::beg);
    }
}

DiskManager::~DiskManager() {
    if (db_file_.is_open()) db_file_.close();
}

void DiskManager::ReadPage(page_id_t page_id, char* page_data) {
    std::streamoff offset = static_cast<std::streamoff>(page_id) * PAGE_SIZE;
    db_file_.seekg(offset);
    db_file_.read(page_data, PAGE_SIZE);
}

void DiskManager::WritePage(page_id_t page_id, const char* page_data) {
    std::streamoff offset = static_cast<std::streamoff>(page_id) * PAGE_SIZE;
    db_file_.seekp(offset);
    db_file_.write(page_data, PAGE_SIZE);
    db_file_.flush();
}

page_id_t DiskManager::AllocatePage() {
    return next_page_id_++;
}