#include <iostream>
#include <cstring>
#include "Common.h"

template<class T>
class ObjectPool {
public:
    T* New() {
        T* obj = nullptr;
        // 优先把还回来内存块对象，再次重复利用
        if (free_list_) {
            // 从自由链表头删一个对象
            void* next = next_obj(free_list_);
            obj = (T*)free_list_;
            free_list_ = next;
        } else {
            // 剩余内存不够一个对象大小时，则重新开大块空间
            if (remain_bytes_ < sizeof(T)) {
                remain_bytes_ = 128 * 1024;
                memory_ = (char*)system_alloc(remain_bytes_ >> PAGE_SHIFT);
                // 申请内存失败抛异常
                if (memory_ == nullptr) {
                    throw std::bad_alloc();
                }
            }
            // 从大块内存中切出 obj_size 字节的内存
            obj = (T*)memory_;
            // 如果对象的大小小于指针的大小，那么我们也要每次给对象分配最低指针的大小。因为 FreeList 必须能存得下一个指针的大小
            size_t obj_size = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
            // 调整成员变量
            memory_ += obj_size;
            remain_bytes_ -= obj_size;
        }
        // 定位 new，显示调用 T 的构造函数初始化
        new(obj)T;
        return obj;
    }
    void Delete(T* obj) {
        // 显示调用析构函数清理对象
        obj->~T();
        // 头插
        next_obj(obj) = free_list_;
        free_list_ = obj;
    }

private:
    char* memory_ = nullptr; // 指向大块内存的指针
    size_t remain_bytes_ = 0; // 大块内存在切分过程中剩余字节数
    void* free_list_ = nullptr; // 还回来过程中链接的自由链表的头指针
};