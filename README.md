# 高并发的内存池

## 什么是内存池

内存池（`Memory Pool`）是一种动态内存分配与管理技术，通常情况下，程序员习惯直接使用 `new`，`delete`，`malloc`，`free` 等 `API` 申请和释放内存，这样导致的后果就是: 当程序运行的时间很长的时候，由于所申请的内存块的大小不定，频繁使用时会造成大量的内存碎片从而降低程序和操作系统的性能。

内存池是指程序预先从操作系统申请一块足够大内存，此后，当程序中需要申请内存的时候，不是直接向操作系统申请，而是直接从内存池中获取。同理，当程序释放内存的时候，并不真正将内存返回给操作系统，而是返回内存池。当程序退出时，内存池才将之前申请的内存真正释放。

## 为什么需要使用内存池

在 `C/C++` 中我们通常使用 `malloc`、`free` 或 `new`、`delete` 来动态分配内存。

一方面，因为这些函数涉及到了系统调用，所以频繁的调用必然会导致程序性能的损耗。

另一方面，频繁的分配和释放小块内存会导致大量的内存碎片的产生，当碎片积累到一定的量之后，将无法分配到连续的内存空间，系统不得不进行碎片整理来满足分配到连续的空间，这样不仅会导致系统性能损耗，而且会导致程序对内存的利用率低下。

![001]()

内存碎片又分为外碎片和内碎片，上图演示的是外碎片。外部碎片是一些空闲的连续内存区域太小，这些内存空间不连续，以至于合计的内存足够，但是不能满足一些的内存分配申请需求。内部碎片是由于一些对齐的需求，导致分配出去的空间中一些内存无法被利用。

`C/C++` 中动态申请内存并不是直接去堆上申请的，而是通过 `malloc` 函数去申请的，`C++` 中的 `new` 本质上也是封装了 `malloc` 函数。

`malloc` 就是一个内存池。`malloc()` 相当于向操作系统 `批发` 了一块较大的内存空间，然后 `零售` 给程序用。当全部 `售完` 或程序有大量的内存需求时，再根据实际需求向操作系统 `进货`。

![002]()

`malloc` 的实现方式有很多种，一般不同编译器平台用的都是不同的。

## 定长内存分配器

定长内存池是针对固定大小内存块的申请和释放的问题，因为它申请和释放的内存块大小是固定的，所以不需要考虑内存碎片化的问题。

### 如何实现定长

我们可以利用非类型模板参数来控制向该内存池申请的内存大小，如下面代码，可以控制每次向内存池申请的内存大小为 `N`:

```cpp
template<size_t N>
class ObjectPool { };
```

此外，定长内存池也叫做对象池，在创建对象池时，对象池可以根据传入的对象类型的大小来实现定长，我们可以通过模板参数来实现定长，例如创建定长内存池时传入的对象类型是 `int`，那么该内存池就只支持 `4` 字节大小内存的申请和释放:

```cpp
template<class T>
class ObjectPool { };
```

### 定长内存池向堆申请空间

这里申请空间不用 `malloc`，而是用 `malloc` 的底层，直接向系统要内存，在 `Windows` 下，可以调用 `VirtualAlloc` 函数，在 `Linux` 下，可以调用 `brk` 或 `mmap` 函数:

```cpp
#ifdef _WIN32
    #include <Windows.h>
#else
    #include <sys/mman.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

// 该函数较短，可设置成内联函数提高效率
inline static void* system_alloc(size_t kpage) {
#ifdef _WIN32
    // system_alloc 的第二个参数是字节数，因此要把页数转化为字节数，向堆上申请 kpage 块 8192 字节空间
    void* ptr = VirtualAlloc(0, (kpage << 13), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); // 直接向系统申请空间
    if (ptr == nullptr) {
        throw std::bad_alloc();
    }
#else
    // linux 下 brk mmap 等
    errno;
    // 该内存可读可写（PROT_READ | PROT_WRITE）
    // 私有映射，所做的修改不会反映到物理设备（MAP_PRIVATE）
    // 匿名映射，映射区不与任何文件关联，内存区域的内容会被初始化为 0（MAP_ANONYMOUS）
    int fd = open("/dev/zero", O_RDWR);
    void* ptr = mmap(0, (kpage << 13), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, fd, 0);
    // 成功执行时，mmap() 返回被映射区的指针
    // 失败时，mmap() 返回 MAP_FAILED [其值为(void *)-1]，
    // errno 被设为某个值
    close(fd);
    if (ptr == MAP_FAILED) {
        std::cerr << "MAP_FAILED!" << std::endl;
        std::cerr << "errno: " << strerror(errno) << std::endl;
        throw std::bad_alloc();
    }
#endif

    return ptr;
}
```

### 定长内存池中的成员变量

对于申请的大块内存，我们可以利用指针进行管理，再用一个变量来记录申请的内存中剩余的内存大小。

![003]()

对于释放回来的内存，我们可以利用链表来管理，这就需要一个指向链表的指针。

![004]()

所以定长内存池中设计了三个变量:
- 指向大块内存的指针
- 记录大块内存在切分过程中剩余字节数的变量
- 记录回收内存自由链表的头指针

```cpp
template<class T>
class ObjectPool {
public:
    T* New();
    void Delete(T* obj);

private:
    char* memory_ = nullptr; // 指向大块内存的指针
    size_t remain_bytes_ = 0; // 大块内存在切分过程中剩余字节数
    void* free_list_ = nullptr; // 还回来过程中链接的自由链表的头指针
};
```

### 定长内存池为用户申请空间

当我们为用户申请空间时，优先使用释放回来的内存，即自由链表。将自由链表头节点删除并将该内存返回。

![005]()

如果自由链表当中没有内存块，那么我们就在大块内存中切出定长的内存块进行返回。内存块切出后，及时更新 `_memory` 指针的指向，以及 `_remainBytes` 的值。

![006]()

当大块内存不够切分出一个对象时，调用封装的 `SystemAlloc` 函数向系统申请一大块内存，再进行切分。

为了让释放的内存能够并入自由链表中，我们必须保证切分出来的对象能够存下一个地址，即申请的内存块至少为 `4` 字节（`32`位）或 `8` 字节（`64` 位）:

```cpp
T* New() {
    T* obj = nullptr;
    // 优先把还回来内存块对象，再次重复利用
    if (free_list_) {
        // 从自由链表头删一个对象
        void* next = *((void**)_freeList);
        obj = (T*)free_list_;
        free_list_ = next;
    } else {
        // 剩余内存不够一个对象大小时，则重新开大块空间
        if (remain_bytes_ < sizeof(T)) {
            remain_bytes_ = 128 * 1024;
            memory_ = (char*)system_alloc(remain_bytes_ >> 13);
            // 申请内存失败抛异常
            if (memory_ == nullptr) {
                throw std::bad_alloc();
            }
        }
        // 从大块内存中切出 obj_size 字节的内存
        obj = (T*)memory_;
        // 保证对象能够存下一个地址
        size_t obj_size = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
        // 调整成员变量
        memory_ += obj_size;
        remain_bytes_ -= obj_size;
    }
    // 定位 new，显示调用 T 的构造函数初始化
    new(obj)T;
    return obj;
}
```

注意:

`void* next = *((void**)_freeList);`

`_freeList` 本来是一个一维指针，指向自由链表的起始地址，其指向的内容就是下一块内存的地址（嵌入式指针），先将其强制转换为二维指针，经过强制转换后的 `_freeList` 指针本身指向的地址是没有变化的，即 `(void**)_freeList` 也指向 `_freeList` 的所指向的地址，并且这个地址内存放的是指针，再将其解引用，得到的是一个地址，该地址就是下一块内存的地址。换句话说，`next` 应该等于 `_freeList` 指向的内存，该内存存储的是下一块内存的地址，这样，`next` 就指向了下一块内存。

测试:

```cpp
#include <iostream>

using namespace std;

struct Test {
    struct Test* next;
};

int main() {
    cout << "sizeof(Test) = " << sizeof(Test) << endl;
    // t（指针）->t1（对象）->t2（对象）->t3（对象）
    Test t1, t2, t3, *t = &t1;;
    t1.next = &t2;
    t2.next = &t3;
    t3.next = nullptr;
    cout << "t addr = " << t << endl
         << "t1 addr = " << &t1 << endl
         << "t2 addr = " << &t2 << endl
         << "t3 addr = " << &t3 << endl;

    Test* next = *((Test**)t);
    cout << "next addr == t2 addr: " << (next == &t2 ? "true" : "false") << endl;
    return 0;
}
```

`size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);`

如果需要分配的对象内存不足一个指针的大小，则分配一个指针的大小。

### 定长内存池管理回收的内存

我们用链表管理回收的内存，为了方便使用和节省空间，我们用内存块的前 `4` 个字节（`32` 位）或 `8` 个字节（64 位）记录下一个内存块的起始地址，如下图所示。

![007]()

当回收内存块时，将内存块头插入自由链表即可。

![008]()

```cpp
void Delete(T* obj) {
    // 显示调用析构函数清理对象
    obj->~T();
    // 头插
    *(void**)obj = free_list_;
    free_list_ = obj;
}
```

如何让一个指针在 `32` 位平台下解引用后能向后访问 `4` 个字节，在 `64` 位平台下解引用后能向后访问 `8` 个字节呢？

这里我们利用二级指针，因为二级指针存储的是一级指针的地址，而一级指针会在不同的平台下呈现出不同的大小，二级指针解引用会向后访问一级指针的大小。将其写成函数:

```cpp
static void*& next_obj(void* obj) {
    return *(void**)obj;
}
```

### 定长内存池代码

ObjectPool 类:

[ObjectPool](./ObjectPool/ObjectPool.h)

`ObjectPool` 测试:

[TestObjectPool](./ObjectPool/TestObjectPool.cpp)

测试结果:

```
sizeof(std::string*) = 8
new cost time:2168
object pool cost time:1515
```

从结果中我们可以看出，设计的定长内存池要比 `malloc` 和 `free` 快一些。但是定长内存池只适用于申请和释放固定大小的内存，而 `malloc` 和 `free` 可以申请和释放任意大小的内存。

为了解决定长内存池的局限性，谷歌设计了 `tcmalloc`，下面模拟实现 `tcmalloc` 简易版本。

## 高并发内存池整体框架设计

在多线程申请内存的场景下，必然存在激烈的锁竞争问题。我们实现的内存池需要考虑以下几方面的问题:
- 性能问题
- 多线程环境下，锁竞争问题
- 内存碎片问题

`concurrent memory pool` 主要由以下 `3` 个部分构成：
- `thread cache`: 线程缓存是每个线程独有的，用于小于 `256KB` 的内存的分配，线程从这里申请内存不需要加锁，每个线程独享一个 `cache`，这也就是这个并发线程池高效的地方
- `central cache`: 中心缓存是所有线程所共享，`thread cache` 是按需从 `central cache` 中获取的对象。`central cache` 合适的时机回收 `thread cache` 中的对象，避免一个线程占用了太多的内存，而其他线程的内存吃紧，达到内存分配在多个线程中更均衡的按需调度的目的。`central cache` 是存在竞争的，所以从这里取内存对象是需要加锁，首先这里用的是桶锁，其次只有 `thread cache` 没有内存对象时才会找 `central cache`，所以这里竞争不会很激烈
- `page cache`: 页缓存是在 `central cache` 缓存上面的一层缓存，存储的内存是以页为单位存储及分配的，`central cache` 没有内存对象时，从 `page cache` 分配出一定数量的 `page`，并切割成定长大小的小块内存，分配给 `central cache`。当一个 `span` 的几个跨度页的对象都回收以后，`page cache` 会回收 `central cache` 满足条件的 `span` 对象，并且合并相邻的页，组成更大的页，缓解内存碎片的问题

![009]()

### `thread cache`

#### `thread cache` 整体框架

`thread cache` 是哈希桶结构，每个桶是一个按桶位置映射大小的内存块对象的自由链表。每个线程都会有一个 `thread cache` 对象，这样每个线程在这里获取对象和释放对象时是无锁的。

![010]()

当线程要申请内存时，通过计算得到对齐后的字节数，从而找到对应的哈希桶，如果哈希桶中的自由链表不为空，就从自由链表中头删一块内存返回。如果哈希桶中的自由链表为空，就需要向下一层的 `central cache` 申请内存。

`thread cache` 代码框架如下:

```cpp
class ThreadCache {
public:
    // 申请和释放内存对象
    void* Allocate(size_t size);
    void Deallocate(void* ptr, size_t size);
    
    // 从中心缓存获取对象
    void* fetch_from_central_cache(size_t index, size_t size);

    // 释放对象时，链表过长时，回收内存到中心缓存
    void list_too_long(FreeList& list, size_t size);
private:
    // 哈希桶
    FreeList free_lists_[NFREELIST];
};

// TLS thread local storage（TLS 线程本地存储）
#ifdef _WIN32
    static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;
#else
    static __thread ThreadCache* pTLSThreadCache = nullptr;
#endif
```

哈希桶中的自由链表是单链表结构，和上文实现的定长内存池一样，通过内存块的前 `4` 位或 `8` 位地址连接下一内存块:

```cpp
static void*& next_obj(void* obj) {
    return *(void**)obj;
}

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

    // 从自由链表头部获取 n 个内存块
    void pop_range(void*& start, void*& end, size_t n) {
        assert(n >= size_);
        start = free_list_;
        end = start;

        // 确定获取内存块链表结尾
        while (--n) {
            end = next_obj(end);
        }

        free_list_ = next_obj(end);
        next_obj(end) = nullptr;
        size_ -= n;
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
```

#### `thread cache` 哈希桶映射对齐规则

对象大小的对齐映射并不是均匀的，而是成倍增长的。对象大小的对齐映射固定不变的话，如果映射值较小，就会创建大量的哈希桶，例如 `256kb` 如果按照 `8byte` 划分，则会创建 `32768` 个哈希桶。如果映射值较大，又会造成大量的空间浪费，产生内碎片问题。

为了减少空间浪费率和创建哈希桶的内存开销，我们设计了如下映射关系:

|字节数|对齐数|哈希桶下标|
|-|-|-|
|`[1, 128]`|`8`|`[0, 16)`|
|`[128+1, 1024]`|`16`|`[16, 72)`|
|`[1024+1, 8x1024]`|`128`|`[72, 128)`|
|`[8x1024+1, 64x1024]`|`1024`|`[128, 184)`|
|`[64x1024+1, 256x1024]`|`8x1024`|`[184, 208)`|

例如，字节数为 `5` 字节，向 `8` 字节对齐，分配 `8` 字节，哈希桶下标为 `0`。字节数为 `10000`，向 `1024` 字节对齐，分配 `10x1024=10240` 字节，哈希桶下标为 `138`。

空间浪费率:

空间浪费率为浪费的字节数除以对齐后的字节数，以 `129~1024` 这个区间为例，该区域的对齐数是 `16`，那么最大浪费的字节数就是 `15`，而最小对齐后的字节数就是这个区间内的前 `16` 个数（`129~144`）所对齐到的字节数，也就是 `144`，那么该区间的最大浪费率就是 `15÷144≈10.42%`。

计算对象大小的对齐映射数:

计算对象大小的对齐映射数时，我们可以先判断该字节属于哪个区间，再调用子函数完成映射:

```cpp
static size_t round_up_(size_t bytes, size_t align_num) {
    if (bytes % align_num != 0) {
        return (bytes / align_num + 1) * align_num;
    } else {
        return bytes;
    }
}

// 获取向上对齐后的字节数
static inline size_t round_up(size_t bytes) {
    if (bytes <= 128) {
        return round_up_(bytes, 8);
    } else if (bytes <= 1024) {
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
```

子函数也可以利用位运算，位运算的速度是比乘法和除法更快:

```cpp
static size_t round_up_(size_t bytes, size_t align_num) {
    return (((bytes) + align_num-1) & ~(align_num - 1));
}
```

计算内存映射的哈希桶:

获取字节对应的哈希桶下标时，也是先判断它在哪个区间，再调用子函数去找。

```cpp
size_t index_(size_t bytes, size_t align_shift) {
    if (bytes % (1 << align_shift) == 0) {
        return bytes / (1 << align_shift) - 1;
    } else {
        return bytes / (1 << align_shift);
    }
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
```

映射哈希桶的子函数也可使用位运算:

```cpp
size_t index_(size_t bytes, size_t align_shift) {
    return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
}
```

#### `thread cache` 申请内存

- 当申请的内存 `size<=256KB` 时，先获取到线程本地存储的 `thread cache` 对象，再通过计算找到 `size` 映射的哈希桶下标 `i`
- 查看下标为 `i` 的哈希桶中的自由链表是否为空，如果哈希桶中的自由链表不为空，就从自由链表中头删一块内存返回
- 如果哈希桶中的自由链表为空，则批量从 `central cache` 中获取一定数量的对象，插入到自由链表并返回一个对象

```cpp
void* ThreadCache::Allocate(size_t size) {
    assert(size <= MAX_BYTES);
    size_t align_size = SizeClass::round_up(size);
    // 计算映射的哈希桶下标
    size_t index = SizeClass::index(size);
    if (!free_lists_[index].empty()) {
        return free_lists_[index].pop();
    } else {
        return fetch_from_central_cache(index, align_size);
    }
}
```

`thread cache` 向 `central cache` 获取内存:

这里会用到慢开始反馈调节算法，开始不会一次向 `central cache` 一次批量要太多，因为要太多了可能用不完，如果不断申请这个 `size` 大小的内存，那么 `batch_num` 就会不断增长，直到上限。

```cpp
// 一次 ThreadCache 应该向 CentralCache 申请的对象的个数（根据对象的大小计算）
static size_t num_move_size(size_t size) {
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

// thread cache 向 central cache 获取内存
void* ThreadCache::fetch_from_central_cache(size_t index, size_t size) {
#ifdef _WIN32
	size_t batch_num = min(_freelists[index].MaxSize(), SizeClass::num_move_size(size));
#else
	size_t batch_num = std::min(free_lists_[index].max_size(), SizeClass::num_move_size(size));
#endif
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
    } else {
        // 将申请的一段内存头插入对应的自由链表
        free_lists_[index].push_range(next_obj(start), end, actual_num - 1);
    }
    
    return start;
}
```

#### `threadcacheTLS` 无锁访问

要实现每个线程无锁的访问属于自己的 `thread cache`，我们需要用到线程局部存储 `TLS`，这是一种变量的存储方法，使用该存储方法的变量在它所在的线程是全局可访问的，但是不能被其他线程访问到，这样就保持了数据的线程独立性:

```cpp
#ifdef _WIN32
    static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;
#else
    static __thread ThreadCache* pTLSThreadCache = nullptr;
#endif
```

但不是每个线程被创建时就立马有了属于自己的 `thread cache`，而是当该线程调用相关申请内存的接口时才会创建自己的 `thread cache`，因此在申请内存的函数中会包含以下逻辑:

```cpp
// 通过 TLS，每个线程无锁的获取自己专属的 ThreadCache 对象
if (pTLSThreadCache == nullptr) {
    pTLSThreadCache = new ThreadCache;
}
```

TLS 测试:

```cpp
#include <iostream>
#include <thread>
#include <chrono>

using namespace std;

#ifdef _WIN32
    static _declspec(thread) int pTLSInt = 0;
#else
    static __thread int pTLSInt = 0;
#endif

const int N = 10;

void worker(int i) {
    this_thread::sleep_for(chrono::seconds(i));
    ++(pTLSInt);
    cout << "thread: " << i << " pTLSInt: " << pTLSInt << endl;
}

int main() {
    thread th[N];
    for (int i = 0; i != N; ++i) {
        th[i] = thread(worker, i);
    }
    for (int i = 0; i != N; ++i) {
        th[i].join();
    }
    return 0;
}
```

每个线程都拥有自己的 `pTLSInt`，对其自增不会影响别的线程。




































### 项目介绍

![001]()

第一层是 `Thread Cache`，线程缓存是每个线程独有的，在这里设计的是用于小于 `64k` 的内存分配，线程在这里申请不需要加锁，每一个线程都有自己独立的 `cache`。

第二层是 `Central Cache`，在这里是所有线程共享的，它起着承上启下的作用，`Thread Cache` 是按需要从 `Central Cache` 中获取对象，它就要起着平衡多个线程按需调度的作用，既可以将内存对象分配给 `Thread Cache` 来的每个线程，又可以将线程归还回来的内存进行管理。`Central Cache` 是存在竞争的，所以在这里取内存对象的时候是需要加锁的，但是锁的力度可以控制得很小。

第三层是 `Page Cache`，存储的是以页为单位存储及分配的，`Central Cache` 没有内存对象（`Span`）时，从 `Page cache` 分配出一定数量的 `Page`，并切割成定长大小的小块内存，分配给 `Central Cache`。`Page Cache` 会回收 `Central Cache` 满足条件的 `Span`（使用计数为 `0`）对象，并且合并相邻的页，组成更大的页，缓解内存碎片的问题。

- 怎么实现每个线程都拥有自己唯一的线程缓存呢？
- 为了避免加锁带来的效率，在 `Thread Cache` 中使用 `thread local storage`（`tls`）保存每个线程本地的 `Thread Cache` 的指针，这样 `Thread Cache` 在申请释放内存是不需要锁的。因为每一个线程都拥有了自己唯一的一个全局变量。

线程本地储存（`TLS`）又叫线程局部存储，是线程私有的全局变量。

普通的全局变量在多线程中是共享的，当一个线程对其进行更改时，所有线程都可以看到这个改变。

而线程私有的全局变量不同，线程私有全局变量是线程的私有财产，每个线程都有自己的一份副本，某个线程对其所做的修改只会修改到自己的副本，并不会对其他线程的副本造成影响。

`TLS` 分为静态的和动态的:
- 静态的 `TLS` 是: 直接定义
- 动态的 `TLS` 是: 调用系统的 `API` 去创建的
- 项目里面用到的是静态的 `TLS`

### 设计 `Thread Cache`


































