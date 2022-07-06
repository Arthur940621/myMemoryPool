#pragma once

#include "Common.h"

class ThreadCache {
public:
    // 申请和释放内存对象
    void* Allocate(size_t size);
    void Deallocate(void* ptr, size_t size);
    // 从中心缓存获取对象
    void* fetch_from_central_cache(size_t index, size_t size);
    // 释放对象时，链表过长时，回收内存到中心缓存
    void list_too_long(FreeList& list, size_t size);
    ~ThreadCache();
private:
    // 哈希桶
    FreeList free_lists_[NFREELISTS];
};

// TLS thread local storage（TLS 线程本地存储）
static __thread ThreadCache* pTLSThreadCache = nullptr;