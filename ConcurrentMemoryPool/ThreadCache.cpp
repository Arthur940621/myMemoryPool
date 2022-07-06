#include "ThreadCache.h"
#include "CentralCache.h"

void* ThreadCache::Allocate(size_t size) {
    assert(size <= MAX_BYTES);
    size_t align_size = SizeClass::round_up(size);
    // 计算映射的哈希桶下标
    size_t index = SizeClass::index(size);
    // 如果对应的自由链表桶不为空，直接从桶中取出内存块
    if (!free_lists_[index].empty()) {
        return free_lists_[index].pop();
    } else { // 如果为空，则从 CentralCache 中获取内存块
        return fetch_from_central_cache(index, align_size);
    }
}

void* ThreadCache::fetch_from_central_cache(size_t index, size_t size) {
    size_t batch_num = std::min(free_lists_[index].max_size(), SizeClass::num_move_size(size));
    // 慢开始算法
    if (free_lists_[index].max_size() == batch_num) {
        free_lists_[index].max_size() += 1;
    }
    void* start = nullptr;
    void* end = nullptr;
    // 向 CentralCache 申请一段内存
    size_t actual_num = CentralCache::get_instance()->fetch_range_obj(start, end, batch_num, size);
    assert(actual_num > 0);
    if (actual_num == 1) {
        assert(start == end);
        return start;
    } else {
        // 将申请的一段内存头插入对应的自由链表
        free_lists_[index].push_range(next_obj(start), end, actual_num - 1);
        return start;
    }
}

void ThreadCache::Deallocate(void* ptr, size_t size) {
    assert(ptr && size <= MAX_BYTES);
    // 找到映射的自由链表桶，将对象插入
    size_t index = SizeClass::index(size);
    free_lists_[index].push(ptr);

    // 当自由链表下面挂着的小块内存的数量大于等于一次批量申请的小块内存的数量时，将 size() 大小的小块内存全部返回给 CentralCache 的 Span 上
    if (free_lists_[index].size() >= free_lists_[index].max_size()) {
        list_too_long(free_lists_[index], size);
    }
}

void ThreadCache::list_too_long(FreeList& list, size_t size) {
    void* start = nullptr;
    void* end = nullptr;
    // 将该段自由链表从哈希桶中切分出来
    list.pop_range(start, end, list.max_size());
    CentralCache::get_instance()->release_list_to_spans(start, size);
}

// 线程结束之前，ThreadCache 当中可能留有一些小块内存，要将这些内存返回给 CentralCache
ThreadCache::~ThreadCache() {
    for (size_t i = 0; i < NFREELISTS; ++i) {
        if (!free_lists_[i].empty()) {
            list_too_long(free_lists_[i], SizeClass::bytes(i));
        }
    }
}
