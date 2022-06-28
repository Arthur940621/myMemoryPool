using namespace std;

#include <iostream>
#include <vector>

namespace test13
{

// 模仿 SGI STL，G2.92 的 std::alloc

#define __THROW_BAD_ALLOC  cerr << "out of memory" << endl; exit(1)

// 一级配置器

template <int inst>
class __malloc_alloc_template {
private:
    static void* oom_malloc(size_t);
    static void* oom_realloc(void *, size_t);
    static void (*__malloc_alloc_oom_handler)();
public:
    static void* allocate(size_t n) {
        void* result = malloc(n);
        if (0 == result) {
            result = oom_malloc(n);
        }
        return result;
    }
    static void deallocate(void *p, size_t) {
        free(p);
    }
    static void* reallocate(void *p, size_t, size_t new_sz) {
        void * result = realloc(p, new_sz); // 直接使用 realloc()
        if (0 == result) {
            result = oom_realloc(p, new_sz);
        }
        return result;
    }
    static void (*set_malloc_handler(void (*f)()))() { // 类似 C++ 的 set_new_handler()
        void (*old)() = __malloc_alloc_oom_handler;
        __malloc_alloc_oom_handler = f;
        return(old);
    }
};

template <int inst>
void (*__malloc_alloc_template<inst>::__malloc_alloc_oom_handler)() = 0;

template <int inst>
void* __malloc_alloc_template<inst>::oom_malloc(size_t n) {
    void (*my_malloc_handler)();
    void* result;
    for (;;) {
        my_malloc_handler = __malloc_alloc_oom_handler;
        if (0 == my_malloc_handler) {
            __THROW_BAD_ALLOC;
        }
        (*my_malloc_handler)();
        result = malloc(n);
        if (result) {
            return(result);
        }
    }
}

template <int inst>
void * __malloc_alloc_template<inst>::oom_realloc(void* p, size_t n) {
    void (*my_malloc_handler)();
    void* result;
    for (;;) {
        my_malloc_handler = __malloc_alloc_oom_handler;
        if (0 == my_malloc_handler) {
            __THROW_BAD_ALLOC;
        }
        (*my_malloc_handler)();
        result = realloc(p, n);
        if (result) {
            return(result);
        }
    }
}

typedef __malloc_alloc_template<0> malloc_alloc;

// 二级配置器

// 实际应该用 static const int x = N 取代 enum { x = N }
enum {__ALIGN = 8}; // 小区块的上调边界
enum {__MAX_BYTES = 128}; // 小区块的上限
enum {__NFREELISTS = __MAX_BYTES/__ALIGN}; // free_lists 个数

// 本例中两个 template 参数没有用上
template <bool threads, int inst>
class __default_alloc_template {
private:
    // 将 bytes 上调至 8 的倍数
    static size_t ROUND_UP(size_t bytes) {
        return (((bytes) + __ALIGN-1) & ~(__ALIGN - 1));
    }
private:
    union obj {
        union obj* free_list_link;
        char client_data[1]; // 没有使用到
    };
private:
    static obj* volatile free_list[__NFREELISTS];
    // 计算分配 bytes 字节应该由 free_list 的第几号 list 提供
    static size_t FREELIST_INDEX(size_t bytes) {
        return (((bytes) + __ALIGN-1)/__ALIGN - 1);
    }

    // 当 list 为空的时候，要进行充值（即申请一大块内存），返回大小为 n 的对象，剩下的添加到大小为 n 的 free_list 中。
    static void* refill(size_t n);
    // 申请一大块内存
    static char* chunk_alloc(size_t size, int& nobjs);

    // pool 就是依靠 start_free 和 end_free 这两个指针围起来的
    static char* start_free;
    static char* end_free;
    static size_t heap_size;
public:
    static void* allocate(size_t n) {
        // my_free_list 变量为指针的指针
        obj* volatile *my_free_list;
        obj* result;
        // 如果要分配的尺寸大于 128，由一级分配器分配
        if (n > (size_t)__MAX_BYTES) {
            return(malloc_alloc::allocate(n));
        }
        // 表示定位到是第几号链表
        my_free_list = free_list + FREELIST_INDEX(n);
        result = *my_free_list;
        if (result == 0) { // list 为空，要申请一大块内存
            void* r = refill(ROUND_UP(n));
            return r;
        }
        // 若往下进行表示 list 内已有可用的区块
        // 将第一块内存块给到客户，并向下移动指针
        *my_free_list = result->free_list_link;
        return (result);
    }

    // deallocate 没有将内存还给操作系统
    static void deallocate(void* p, size_t n) {
        obj* q = (obj*)p;
        // my_free_list 变量为指针的指针
        obj* volatile *my_free_list;
        // 如果要回收的尺寸大于 128，由一级分配器回收
        if (n > (size_t) __MAX_BYTES) {
            malloc_alloc::deallocate(p, n);
            return;
        }
        my_free_list = free_list + FREELIST_INDEX(n);
        q->free_list_link = *my_free_list;
        *my_free_list = q;
    }

    static void * reallocate(void* p, size_t old_sz, size_t new_sz);
};

template <bool threads, int inst>
void* __default_alloc_template<threads, inst>::
refill(size_t n) { // n 已调整为 8 的倍数
    int nobjs = 20; // 预设取 20 个区块，但不一定能够
    char* chunk = chunk_alloc(n, nobjs);
    obj* volatile *my_free_list;
    obj* result;
    obj* current_obj;
    obj* next_obj;
    int i;

    if (1 == nobjs) {
        return(chunk);
    }

    my_free_list = free_list + FREELIST_INDEX(n);
    
    result = (obj*)chunk;
    *my_free_list = next_obj = (obj*)(chunk + n);
    for (i = 1; ; ++i) {
        current_obj = next_obj;
        next_obj = (obj*)((char*)next_obj + n);
        if (nobjs - 1 == i) {
            current_obj->free_list_link = 0;
            break;
        } else {
            current_obj->free_list_link = next_obj;
        }
    }
    return(result);
}

template <bool threads, int inst>
char*
__default_alloc_template<threads, inst>::
chunk_alloc(size_t size, int& nobjs) {
    char* result;
    size_t total_bytes = size * nobjs;
    size_t bytes_left = end_free - start_free;
    if (bytes_left >= total_bytes) { // pool 空间足以满足 20 块需求
        result = start_free;
        start_free += total_bytes; // 调整（降低）pool 水位
        return(result);
    } else if (bytes_left >= size) { // pool 空间只能满足一块（含）以上需求
        nobjs = bytes_left / size; // 改变需求数
        total_bytes = size * nobjs; // 改变需求总量（Bytes）
        result = start_free;
        start_free += total_bytes; // 调整（降低）pool 水位
        return(result);
    } else { // pool 空间不足以满足一块需求
        size_t bytes_to_get = 2 * total_bytes + ROUND_UP(heap_size >> 4);
        // 首先尝试将 pool 做充分利用
        if (bytes_left > 0) {
            obj* volatile *my_free_list = free_list + FREELIST_INDEX(bytes_left);
            // 将 pool 空间编入 free_list
            ((obj*)start_free)->free_list_link = *my_free_list;
            *my_free_list = (obj*)start_free;
        }
        start_free = (char*)malloc(bytes_to_get); // 从 system free-stroe 取这么多，注入 pool
        if (0 == start_free) { // 失败，试从 free_list 找区块
            int i;
            obj* volatile *my_free_list, *p;
            for (i = size; i <= __MAX_BYTES; i += __ALIGN) {
                my_free_list = free_list + FREELIST_INDEX(i);
                p = *my_free_list;
                if (0 != p) { // 该 free_list 内有可用区域，以下释放出一块给 pool
                    *my_free_list = p->free_list_link;
                    // 把 free_list 内的目前第一块当成 pool
                    start_free = (char*)p;
                    end_free = start_free + i;
                    // 递归再试一次
                    return(chunk_alloc(size, nobjs));
                    // 此时的 pool 一定能够提供至少一个区域
                    // 任何残余零头终将编入适当的 free_list
                }
            }
            end_free = 0; // 至此，表示 memory 已经山穷水尽
            start_free = (char*)malloc_alloc::allocate(bytes_to_get); // 改用第一级，看看 oom-handle 能否尽点力
        }
        // 至此，表示已从 system free-store 成功获取了很多 memory
        heap_size += bytes_to_get;
        end_free = start_free + bytes_to_get; // 调整尾端
        return(chunk_alloc(size, nobjs)); // 递归再试一次
    }
}

template <bool threads, int inst>
char* __default_alloc_template<threads,inst>::start_free = 0;

template <bool threads, int inst>
char* __default_alloc_template<threads,inst>::end_free = 0;

template <bool threads, int inst>
size_t __default_alloc_template<threads,inst>::heap_size = 0;

template <bool threads, int inst>
typename __default_alloc_template<threads, inst>::obj* volatile
__default_alloc_template<threads, inst>::free_list[__NFREELISTS]
= {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, };

typedef __default_alloc_template<false, 0> alloc;

template<class T, class Alloc>
class simple_alloc {
public:
    static T* allocate(size_t n) {
        return 0 == n ? 0 : (T*)Alloc::allocate(n * sizeof(T));
    }
    static T* allocate(void) {
        return (T*)Alloc::allocate(sizeof(T));
    }
    static void deallocate(T* p, size_t n) {
        if (0 != n) {
            Alloc::deallocate(p, n * sizeof(T));
        }
    }
    static void deallocate(T *p) {
        Alloc::deallocate(p, sizeof(T));
    }
};

void test_G29_alloc() {
    cout << "\ntest_global_allocator_with_16_freelist()..........\n";
    void* p1 = alloc::allocate(120);
    void* p2 = alloc::allocate(72);
    void* p3 = alloc::allocate(60); // 不是 8 的倍数

    cout << p1 << ' ' << p2 << ' ' << p3 << endl;

    alloc::deallocate(p1,120);
    alloc::deallocate(p2,72);
    alloc::deallocate(p3,60);

    // 以下不能搭配容器来测试，因为新版 g++ 对于 allocator 有更多要求
    // vector<int, simple_alloc<int, alloc>> v;
    // for(int i = 0; i < 1000; ++i) {
    //     v.push_back(i);
    // }
    // for(int i = 700; i < 720; ++i) {
    //     cout << v[i] << ' ';
    // }
}

}