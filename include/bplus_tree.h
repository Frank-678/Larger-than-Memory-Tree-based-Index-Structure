#pragma once
#include "include/common.h"
#include "include/disk_manager.h"

class BplusTree {
    public:
    BplusTree(int key, DiskManager disk);
    void insert();
    private:
    
};