#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::inst_;

size_t CentralCache::fetch_range_obj(void*& start, void*& end, size_t batch_num, size_t size) {
    size_t index = SizeClass::index(size);
    span_list_[index].lock(); // 桶锁
    // 在对应哈希桶中获取一个非空的 Span
    Span* span = get_one_span(span_list_[index], size);
    // 获得的页和页中的自由链表不能为空
    assert(span && span->free_list_);
    // 从 Span 中获取 batch_num 个对象，如果不够 batch_num 个，有多少拿多少
    start = span->free_list_;
    end = start;
    size_t actual_num = 1;
    while (--batch_num && next_obj(end) != nullptr) {
        end = next_obj(end);
        ++actual_num;
    }
    span->free_list_ = next_obj(end); // 取完后剩下的对象继续放到自由链表
    next_obj(end) = nullptr; // 取出的一段链表的表尾置空
    span->use_count_ += actual_num; // 更新被分配给 ThreadCache 的计数
    span_list_[index].unlock(); // 解锁
    return actual_num;
}

// 获取一个非空的 Span
Span* CentralCache::get_one_span(SpanList& list, size_t size) {
    // 查看当前的 SpanList 中是否有还有未分配对象的 Span
    Span* it = list.begin();
    while (it != list.end()) {
        if (it->free_list_) {
            return it;
        } else {
            it = it->next_;
        }
    }
    // 在 fetch_range_obj() 里上的锁，先把 CentralCache 的桶锁解掉，这样如果其他线程释放内存对象回来，不会阻塞
    list.unlock();
    // 走到这里说明没有空闲 Span 了，只能找 PageCache 要
    PageCache::get_instance()->page_lock(); // 这里加锁也可以，如果在 new_span 函数里加锁，需要使用递归锁
    Span* span = PageCache::get_instance()->new_span(SizeClass::num_move_page(size));
    span->object_size_ = size;
    PageCache::get_instance()->page_unlock();
    // 对获取 Span 进行切分，不需要加锁，其他线程访问不到这个 Span
    // 计算 Span 的大块内存的起始地址和大块内存的大小
    // page_id_ 记录起始页的页号，起始地址=页号*每页的大小
    char* start = (char*)(span->page_id_ << PAGE_SHIFT);
    // n_ 记录页的数量，终止地址=页数*每页的大小+起始地址
    size_t bytes = (span->n_ << PAGE_SHIFT);
    char* end = start + bytes;

    // 把大块内存切成小块链接起来
    // 先切一块下来去做头，方便尾插
    span->free_list_ = start;
    start += size;
    void* tail = span->free_list_;
    // 尾插
    while (start < end) {
        next_obj(tail) = start;
        tail = start;
        start += size;
    }
    next_obj(tail) = nullptr;
    // 切好 Span 以后，需要把 Span 挂到桶里面去的时候，再加锁
    list.lock();
    list.push_front(span);
    return span;
}

// 将一定数量的对象释放到 Span
void CentralCache::release_list_to_spans(void* start, size_t size) {
    assert(start);
    size_t index = SizeClass::index(size);
    span_list_[index].lock();
    while (start) {
        void* next = next_obj(start);
        // 通过映射找到对应的 Span
        PageCache::get_instance()->page_lock();
        Span* span = PageCache::get_instance()->map_obj_to_span(start);
        PageCache::get_instance()->page_unlock();
        // 将 start 小块内存头插到 Span 结构的自由链表中
        next_obj(start) = span->free_list_;
        span->free_list_ = start;
        --span->use_count_; // 更新分配给 ThreadCache 的计数
        if (span->use_count_ == 0) {
            span_list_[index].erase(span);
            span->free_list_ = nullptr;
            span->next_ = nullptr;
            span->prev_ = nullptr;

            // 释放 Span 给 PageCache 时，使用 PageCache 的锁就可以了
            span_list_[index].unlock();
            PageCache::get_instance()->page_lock();
            PageCache::get_instance()->releas_span_to_page(span);
            PageCache::get_instance()->page_unlock();
            span_list_[index].lock();
        }
        start = next;
    }
    span_list_[index].unlock();
}