# pragma once

#include <unordered_map>
#include "Common.h"
#include "ObjectPool.h"

class PageCache {
public:
    static PageCache* get_instance() {
        return &inst_;
    }
    // 将 PAGE_ID 映射到 Span* 上，这样可以通过页号直接找到对应的 Span* 的位置
    Span* map_obj_to_span(void* obj);
    // 释放空闲（use_count_ 减为 0）的 Span 回到 Pagecache，并合并相邻的 Span
    void releas_span_to_page(Span* span);
    // 向堆申请一个 Span
    Span* new_span(size_t k);
    std::mutex page_mtx_;
private:
    PageCache() = default;
    PageCache(const PageCache&) = delete;
    PageCache& operator=(const PageCache&) = delete;
    static PageCache inst_;
    SpanList span_list_[NPAGES];
    ObjectPool<Span> span_pool_;
    // 建立页号和地址间的映射
    std::unordered_map<PAGE_ID, Span*> id_span_map_;
};