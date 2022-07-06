#pragma once

#include "Common.h"

// 由于全局只能有一个 CentralCache 对象，所以这里设计为单例模式
class CentralCache {
public:
    static CentralCache* get_instance() {
        return &inst_;
    }
    // 获取一个非空的 Span
    Span* get_one_span(SpanList& list, size_t size);
    // 从 CentralCache 获取一定数量的对象给 ThreadCache
    size_t fetch_range_obj(void*& start, void*& end, size_t batch_num, size_t size);
    // 将一定数量的对象释放到 Span
    void release_list_to_spans(void* start, size_t size);
private:
    CentralCache() = default;
    CentralCache(const CentralCache&) = delete;
    CentralCache& operator=(const CentralCache&) = delete;
    static CentralCache inst_; // 仅声明，定义在 .cpp 里面
    SpanList span_list_[NFREELISTS];
};