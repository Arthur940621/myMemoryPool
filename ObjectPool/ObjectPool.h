#include <iostream>
#include <cstring>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

// 该函数较短，可设置成内联函数提高效率
inline static void* system_alloc(size_t kpage) {
    errno;
    // 该内存可读可写（PROT_READ | PROT_WRITE）
    // 私有映射，所做的修改不会反映到物理设备（MAP_PRIVATE）
    // 匿名映射，映射区不与任何文件关联，内存区域的内容会被初始化为 0（MAP_ANONYMOUS）
    int fd = open("/dev/zero", O_RDWR);
    void* ptr = mmap(0, (kpage << 12), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, fd, 0);
    // 成功执行时，mmap() 返回被映射区的指针
    // 失败时，mmap() 返回 MAP_FAILED [其值为(void *)-1]，
    // errno 被设为某个值
    close(fd);
    if (ptr == MAP_FAILED) {
        std::cerr << "MAP_FAILED!" << std::endl;
        std::cerr << "errno: " << strerror(errno) << std::endl;
        throw std::bad_alloc();
    }
    return ptr;
}

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

    static void*& next_obj(void* obj) {
        return *(void**)obj;
    }
};