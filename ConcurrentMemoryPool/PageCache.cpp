#include "PageCache.h"

PageCache PageCache::inst_; // 静态成员类外定义

Span* PageCache::map_obj_to_span(void* obj) {
    // 右移 12 位，找到对应的 id
    PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT;
    auto ret = id_span_map_.find(id);
    if (ret != id_span_map_.end()) {
        return ret->second;
    } else {
        assert(false);
        return nullptr;
    }
}

Span* PageCache::new_span(size_t k) {
    // 加锁，防止多个线程同时到 PageCache 中申请 Span
    // 这里必须是给全局加锁，不能单独的给每个桶加锁
    // 如果对应桶没有 Span，是需要向系统申请的
    // 可能存在多个线程同时向系统申请内存的可能
    assert(k > 0);
    // 如果申请的页大于 128，直接去堆上申请
    if (k >= NPAGES) {
        void* ptr = system_alloc(k);
        Span* span = span_pool_.New();
        span->page_id_ = (PAGE_ID)ptr >> PAGE_SHIFT;
        span->n_ = k;
        span->object_size_ = k << PAGE_SHIFT;
        // 建立页号和 Span* 的映射
        // 将申请的大块内存块的第一个页号插入进去就可以
        // 因为申请的内存大于 MAX_BYTES，是直接还给 PageCache，不需要其他页到这个 Span 的映射
        id_span_map_[span->page_id_] = span;
        return span;
    }
    // 先检查第 k 个桶里面有没有 Span
    if (!span_list_[k].empty()) {
        // 第 k 个桶里面有 Span 直接头切一个块
        Span* k_span = span_list_[k].pop_front();
        // 建立 id 和 Span 的映射，方便 CentralCache 回收小块内存时，查找对应的 Span
        for (PAGE_ID i = 0; i < k_span->n_; ++i) {
            id_span_map_[k_span->page_id_ + i] = k_span;
        }
        return k_span;
    }
    // 检查一下后面的桶里面有没有 Span，如果有可以把他它进行切分
    for (size_t i = k + 1; i < NPAGES; i++) {
        if (!span_list_[i].empty()) {
            Span* n_span = span_list_[i].pop_front();
            // new 一个 Span 用于存放其中一个切分好的 Span
            Span* k_span = span_pool_.New();
            // 在 n_span 的头部切一个 k 页下来，k 页 Span 返回
            k_span->page_id_ = n_span->page_id_;
            k_span->n_ = k;
            n_span->page_id_ += k;
            n_span->n_ -= k;
            // n_span 再挂到对应映射的位置
            span_list_[n_span->n_].push_front(n_span);
            // 存储 n_span 的首尾页号跟 n_span 映射，方便 PageCahce 回收内存时进行的合并查找
            id_span_map_[n_span->page_id_] = n_span;
            id_span_map_[n_span->page_id_ + n_span->n_ - 1] = n_span;
            // 建立 id 和 Span 的映射，方便 CentralCache 回收小块内存时，查找对应的 Span
            for (PAGE_ID i = 0; i < k_span->n_; ++i) {
                id_span_map_[k_span->page_id_ + i] = k_span;
            }
            k_span->is_used_ = true;
            return k_span;
        }
    }
    // 走到这个位置就说明后面没有大页的 Span 了
    // 这时就去找堆要一个 128 页的 Span
    Span* big_span = span_pool_.New();
    void* ptr = system_alloc(NPAGES - 1);
    big_span->page_id_ = (PAGE_ID)ptr >> PAGE_SHIFT;
    big_span->n_ = NPAGES - 1;
    span_list_[big_span->n_].push_front(big_span);
    // 调用自己，下次将 128 页进行拆分
    return new_span(k);
}

void PageCache::releas_span_to_page(Span* span) {
    // 该 Span 管理的空间是向堆申请的
    if (span->n_ > NPAGES - 1) {
        void* ptr = (void*)(span->page_id_ << PAGE_SHIFT);
        system_free(ptr, span->object_size_);
        span_pool_.Delete(span);
        return;
    }

    // 对 Span 前后的页，尝试进行合并，缓解内存碎片问题
    // 向前合并
    while (1) {
        // 与 Span 链表相连的，上一个 Span 的页号
        PAGE_ID prev_id = span->page_id_ - 1;
        auto ret = id_span_map_.find(prev_id);
        // 前面的页号没有，不合并
        // 前面的 Span 没有被申请过（如果在映射表当中，就证明被申请过）
        if (ret == id_span_map_.end()) {
            break;
        }
        // 前面相邻页的 Span 在使用，不合并
        // 这里不能使用 prev_span 的 use_count 作为判断依据，因为 use_count 的值的变化存在间隙（在切分 Span 时）
        Span* prev_span = ret->second;
        if (prev_span->is_used_ == true) {
            break;
        }
        // 合并出超过 128 页的 Span 没办法管理，不合并
        if (prev_span->n_ + span->n_ > NPAGES - 1) {
            break;
        }

        span->page_id_ = prev_span->page_id_;
        span->n_ += prev_span->n_;

        span_list_[prev_span->n_].erase(prev_span);
        span_pool_.Delete(prev_span);
        prev_span = nullptr;
    }

    // 向后合并
    while (1) {
        PAGE_ID next_id = span->page_id_ + span->n_;
        auto ret = id_span_map_.find(next_id);
        if (ret == id_span_map_.end()) {
            break;
        }
        Span* next_span = ret->second;
        if (next_span->is_used_ == true) {
            break;
        }
        if (next_span->n_ + span->n_ > NPAGES - 1) {
            break;
        }

        span->n_ += next_span->n_;

        span_list_[next_span->n_].erase(next_span);
        span_pool_.Delete(next_span);
        next_span = nullptr;
    }
    // 将和并后的 Span 插入到 PageCache 对应的哈希桶中
    span->is_used_ = false;
    span_list_[span->n_].push_front(span);
    id_span_map_[span->page_id_] = span;
    id_span_map_[span->page_id_ + span->n_ - 1] = span;
}