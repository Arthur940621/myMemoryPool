#pragma once

#include "ThreadCache.h"
#include "PageCache.h"

static ObjectPool<ThreadCache> tcPool;

// 每个线程都有自己的 TLS，不可能让用户自己去调用 TLS 然后才能调到 Allocate，而是应该直接给他们提供接口

static void* concurrent_allocate(size_t size) {
    // 当对象大小 > 256KB 时，放到 new_span 里面处理
    if (size > MAX_BYTES) {
        size_t align_size = SizeClass::round_up(size);
        PageCache::get_instance()->page_mtx_.lock();
        Span* span = PageCache::get_instance()->new_span(align_size >> PAGE_SHIFT);
        PageCache::get_instance()->page_mtx_.unlock();
        void* ptr = (void*)(span->page_id_ << PAGE_SHIFT);
        return ptr;
    } else {
        if (pTLSThreadCache == nullptr) {
            pTLSThreadCache = tcPool.New();
        }
        return pTLSThreadCache->Allocate(size);
    }
}

static void concurrent_free(void* ptr) {
    // 别人可能正在对 id_span_map 进行写入操作，应该等别人写完再读（写入操作只在 new_span 函数中, 而调用 new_span 函数前都会加锁)
    PageCache::get_instance()->page_mtx_.lock();
    Span* span = PageCache::get_instance()->map_obj_to_span(ptr);
    PageCache::get_instance()->page_mtx_.unlock();
    size_t size = span->object_size_;
    if (size > MAX_BYTES) { // 大于 NAPES - 1 的情况放到 PageCache 里面处理
        PageCache::get_instance()->page_mtx_.lock();
        PageCache::get_instance()->releas_span_to_page(span);
        PageCache::get_instance()->page_mtx_.unlock();
    } else {
        assert(pTLSThreadCache);
        pTLSThreadCache->Deallocate(ptr, size);
    }
}