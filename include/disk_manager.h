#pragma once
#include "include/page.h"

class DiskManager {
    public:
    DiskManager() : pages(page_cap) {}

    private:
    static constexpr int page_cap = 200;
    std::vector<page> pages;

    
};