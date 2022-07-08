#pragma once

#include <iostream>
#include <cstring>
#include <cassert>
#include <thread>
#include <mutex>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

// 申请的内存块小于 MAX_BYTES，就从 ThreadCache 申请，大于 MAX_BYTES，就直接从 PageCache 中申请
static const size_t MAX_BYTES = 256 * 1024;
// 一个 ThreadCache 中自由链表的个数
static const size_t NFREELISTS = 208;
// PageCache 中的页数范围从 1~128，0 下标处不挂东西
static const size_t NPAGES = 129;
// 页大小转换偏移量，Linux 下一页为 2^12bytes=4KB
static const size_t PAGE_SHIFT = 12;

// 页编号类型，64 位是 8byte
typedef unsigned long long PAGE_ID;

// 该函数较短，可设置成内联函数提高效率
inline static void* system_alloc(size_t kpage) {
    errno;
    // 该内存可读可写（PROT_READ | PROT_WRITE）
    // 私有映射，所做的修改不会反映到物理设备（MAP_PRIVATE）
    // 匿名映射，映射区不与任何文件关联，内存区域的内容会被初始化为 0（MAP_ANONYMOUS）
    int fd = open("/dev/zero", O_RDWR);
    void* ptr = mmap(0, (kpage << PAGE_SHIFT), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, fd, 0);
    // 成功执行时，mmap() 返回被映射区的指针
    // 失败时，mmap() 返回 MAP_FAILED
    // errno 被设为某个值
    close(fd);
    if (ptr == MAP_FAILED) {
        std::cerr << "MAP_FAILED!" << std::endl;
        std::cerr << "errno: " << strerror(errno) << std::endl;
        ptr = nullptr;
    }
    if (ptr == nullptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

inline static void system_free(void* ptr, size_t object_size) {
    munmap(ptr, object_size);
}

// 返回 obj 对象当中用于存储下一个对象的地址的引用
static inline void*& next_obj(void* obj) {
    return *(void**)obj;
}

// 管理切分好的定长对象的自由链表，每个 ThreadCache 里面有很多个 FreeList
class FreeList {
public:
    // 将释放的对象头插到自由链表
    void push(void* obj) {
        assert(obj);
        // 头插
        next_obj(obj) = free_list_;
        free_list_ = obj;
        ++size_;
    }
    // 从自由链表头部获取一个对象
    void* pop() {
        assert(free_list_);
        // 头删
        void* obj = free_list_;
        free_list_ = next_obj(obj);
        --size_;
        return obj;
    }
    // 将释放的 n 个内存块头插入自由链表
    void push_range(void* start, void* end, size_t n) {
        next_obj(end) = free_list_;
        free_list_ = start;
        size_ += n;
    }
    // 将自由链表清空
    void* clear() {
        size_ = 0;
        void* list = free_list_;
        free_list_ = nullptr;
        return list;
    }
    // 判断自由链表是否为空
    bool empty() {
        return free_list_ == nullptr;
    }
    // 记录当前一次申请内存块的数量
    size_t& max_size() {
        return max_size_;
    }
    // 自由链表中内存块的数量
    size_t size() {
        return size_;
    }
private:
    void* free_list_ = nullptr; // 指向自由链表的指针
    size_t max_size_ = 1; // 一次申请内存块的数量
    size_t size_ = 0; // 记录自由链表中内存块数量
};

// 管理空间范围划分与对齐、映射关系的类

class SizeClass {
    // [1,128]              8byte对齐       freelist[0,16)
    // [128+1,1024]         16byte对齐      freelist[16,72)
    // [1024+1,8*1024]      128byte对齐     freelist[72,128)
    // [8*1024+1,64*1024]   1024byte对齐    freelist[128,184)
    // [64*1024+1,256*1024] 8*1024byte对齐  freelist[184,208)
public:
    // align_num 是对齐数
    static inline size_t round_up_(size_t bytes, size_t align_num) {
        return (((bytes) + align_num-1) & ~(align_num - 1));
    }
    // 获取向上对齐后的字节数
    static inline size_t round_up(size_t bytes) {
        assert(bytes <= MAX_BYTES);
        if (bytes <= 128) {
            return round_up_(bytes, 8);
        } else if (bytes <= 8 * 128) {
            return round_up_(bytes, 16);
        } else if (bytes <= 8 * 1024) {
            return round_up_(bytes, 128);
        } else if (bytes <= 64 * 1024) {
            return round_up_(bytes, 1024);
        } else if (bytes <= 256 * 1024) {
            return round_up_(bytes, 8 * 1024);
        } else {
            assert(false);
            return -1;
        }
    }
    // bytes 是字节数，align_shift是 该字节数所遵守的对齐数，以位运算中需要左移的位数表示
    static inline size_t index_(size_t bytes, size_t align_shift) {
        return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
    }
    // 计算映射的哪一个自由链表桶
    static inline size_t index(size_t bytes) {
        assert(bytes <= MAX_BYTES);
        // 每个区间有多少个链
        static int group_array[4] = { 16, 56, 56, 56 };
        if (bytes <= 128) {
            return index_(bytes, 3); // 3 是 2^3，这里传的是使用位运算要达到对齐数需要左移的位数
        } else if (bytes <= 1024) {
            return index_(bytes - 128, 4) + group_array[0];
        } else if (bytes <= 8 * 1024) {
            return index_(bytes - 1024, 7) + group_array[0] + group_array[1];
        } else if (bytes <= 64 * 1024) {
            return index_(bytes - 8 * 1024, 10) + group_array[0] + group_array[1] + group_array[2];
        } else if (bytes <= 256 * 1024) {
            return index_(bytes - 64 * 1024, 13) + group_array[0] + group_array[1] + group_array[2] + group_array[3];
        } else {
            assert(false);
            return -1;
        }
    }
    // 根据传入的桶的下标，计算出该桶所管理的自由链表中的对象大小
    static inline size_t bytes(size_t index) {
        static size_t group[4] = { 16, 56, 56, 56 };
        static size_t total_group[4] = { 16, 72, 128, 184 };
        if (index < total_group[0]) {
            return (index + 1) * 8;
        } else if (index < total_group[1]) {
            return group[0] * 8 + (index + 1 - total_group[0]) * 16;
        } else if (index < total_group[2]) {
            return group[0] * 8 + group[1] * 16 + (index + 1 - total_group[1]) * 128;
        } else if (index < total_group[3]) {
            return group[0] * 8 + group[1] * 16 + group[2] * 128 + (index + 1- total_group[2]) * 1024;
        } else {
            return group[0] * 8 + group[1] * 16 + group[2] * 128 + group[3] * 1024 + (index + 1 - total_group[3]) * 8 * 1024;
        }
    }
    // 一次 ThreadCache 应该向 CentralCache 申请的对象的个数
    static inline size_t num_move_size(size_t size) {
        assert(size > 0);
        // [2, 512]，一次批量移动多少个对象的（慢启动）上限值
        // 小对象一次批量上限高
        // 大对象一次批量上限低
        int num = MAX_BYTES / size;
        if (num < 2) {
            num = 2;
        }
        if (num > 512) {
            num = 512;
        }
        return num;
    }
    // 计算一次向系统获取几个页
    static inline size_t num_move_page(size_t size) {
        size_t num = num_move_size(size);
        size_t npage = num * size >> PAGE_SHIFT;;
        if (npage == 0) {
            npage = 1;
        }
        return npage;
    }
};

struct Span { // 这个结构类似于 ListNode，因为它是构成 SpanList 的单个结点
    PAGE_ID page_id_ = 0; // 大块内存起始页的页号，一个 Span 包含多个页
    size_t n_ = 0; // 页的数量
    size_t use_count_ = 0; // 将切好的小块内存分给 ThreadCache，use_count_ 记录分出去了多少个小块内存
    bool is_used_ = false;
    size_t object_size_ = 0; // 存储当前的 Span 所进行服务的对象的大小
    Span* next_ = nullptr; // 双向链表的结构
    Span* prev_ = nullptr;
    void* free_list_ = nullptr; // 大块内存切成小块并连接起来，这样当 ThreadCache 要的时候直接给它小块的，回收的时候也方便管理
};

// 带头的双向循环链表，将每个桶的位置处的多个 Span 连接起来
class SpanList {
public:
    // 构造
    SpanList() {
        head_ = new Span;
        head_->next_ = head_;
        head_->prev_ = head_;
    }
    // 只需要获取到 begin() 和 end()，然后定义一个 Span* 指针就可以完成 SpanList 的遍历
    Span* begin() {
        return head_->next_; // 带头节点
    }
    Span* end() {
        return head_;
    }
    // 插入新页
    void insert(Span* pos, Span* new_span) {
        assert(pos && new_span);
        Span* prev = pos->prev_;
        new_span->next_ = pos;
        new_span->prev_ = prev;
        pos->prev_ = new_span;
        prev->next_ = new_span;
    }
    // 头插
    void push_front(Span* span) {
        insert(begin(), span);
    }
    // 尾插
    void push_back(Span* span) {
        insert(end(), span);
    }
    // 删除
    void erase(Span* pos) {
        assert(pos != nullptr && pos != head_);
        Span* prev = pos->prev_;
        Span* next = pos->next_;
        prev->next_ = next;
        next->prev_ = prev;
    }
    // 头删
    Span* pop_front() {
        Span* front = begin();
        erase(front);
        return front;
    }
    // 尾删
    Span* pop_back() {
        Span* back = end();
        erase(back);
        return back;
    }
    // 判空
    bool empty() {
        return head_->next_ == head_;
    }
    std::mutex mtx_; // 桶锁: 进到桶里的时候才会加锁
private:
    Span* head_ = nullptr;
};