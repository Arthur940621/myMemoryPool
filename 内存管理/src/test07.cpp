using namespace std;

#include <iostream>

namespace test07
{

class Screen {
public:
    Screen(int x = 0) : i(x) { };
    int get() { return i; }

    void* operator new(size_t);
    // void operator delete(void*); // 二选一
    void operator delete(void*, size_t);
    
private:
    Screen* next;
    static Screen* freeStore;
    static const int screenChunk;
    int i;
};

Screen* Screen::freeStore = 0;
const int Screen::screenChunk = 24;

void* Screen::operator new(size_t size) {
    Screen *p;
    if (!freeStore) {
        // linked list 是空的，所以获取一大块 memory
        // 调用 global operator new
        size_t chunk = screenChunk * size;
        freeStore = p = reinterpret_cast<Screen*>(new char[chunk]);
        for (; p!= &freeStore[screenChunk-1]; ++p) {
            p->next = p + 1;
        }
        p->next = 0;
    }
    p = freeStore;
    freeStore= freeStore->next;
    return p;
}

void Screen::operator delete(void* p, size_t) {
    // 将 deleted object 收回插入 free list 前端
    (static_cast<Screen*>(p))->next = freeStore;
    freeStore = static_cast<Screen*>(p);
}

void test_per_class_allocator_1() {
    cout << "\ntest_per_class_allocator_1()..........\n";
    
    cout << sizeof(Screen) << endl; // 16

    size_t const N = 100;
    Screen* p[N];

    for (int i = 0; i != N; ++i) {
        p[i] = new Screen(i);
    }

    // 输出前 10 个 pointers，比较其间隔
    for (int i = 0; i != 10; ++i) {
        cout << p[i] << endl;
    }

    for (int i = 0; i != N; ++i) {
        delete p[i];
    }
    
}

}