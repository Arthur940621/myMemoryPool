using namespace std;

#include <iostream>
#include <complex>

namespace test10
{

class allocator {
private:
    struct obj {
        struct obj* next; // embedded pointer
    };
public:
    void* allocate(size_t);
    void deallocate(void*, size_t);
    void check();
private:
    obj* freeStore = nullptr;
    const int CHUNK = 5;
};

void* allocator::allocate(size_t size) {
    obj* p;
    if (!freeStore) {
        size_t chunk = CHUNK * size;
        freeStore = p = (obj*)malloc(chunk);

        for (int i = 0; i < (CHUNK-1); ++i) {
            p->next = (obj*)((char*)p + size);
            p = p->next;
        }
        p->next = nullptr;
    }
    p = freeStore;
    freeStore = freeStore->next;
    return p;
}

void allocator::deallocate(void* p, size_t) {
    ((obj*)p)->next = freeStore;
    freeStore = (obj*)p;
}

void allocator::check() {
    obj* p = freeStore;
    int count = 0;
    while (p) {
        cout << p << endl;
        p = p->next;
        count++;
    }
    cout << count << endl;
}

#define DECLARE_POOL_ALLOC() \
public: \
    void* operator new(size_t size) { return myAlloc.allocate(size); } \
    void operator delete(void* p) { myAlloc.deallocate(p, 0); } \
protected: \
    static allocator myAlloc;

#define IMPLEMENT_POOL_ALLOC(class_name) \
allocator class_name::myAlloc;

class Foo {
    DECLARE_POOL_ALLOC()
public:
    long L;
    string str;
public:
    Foo(long l) : L(l) { }
};
IMPLEMENT_POOL_ALLOC(Foo)

class Goo {
    DECLARE_POOL_ALLOC()
public:
    complex<double> c;
    string str;
public:
    Goo(const complex<double>& x) : c(x) { }
};
IMPLEMENT_POOL_ALLOC(Goo)

void test_macros_for_static_allocator() {
    cout << "\ntest_macro_for_static_allocator()..........\n";

    Foo* pF[100];
    Goo* pG[100];

    cout << "sizeof(Foo)=" << sizeof(Foo) << endl;
    cout << "sizeof(Goo)=" << sizeof(Goo) << endl;

    for (int i = 0; i < 23; ++i) {
        pF[i] = new Foo(i);
        pG[i] = new Goo(complex<double>(i,i));
        cout << pF[i] << ' ' << pF[i]->L << "\t";
        cout << pG[i] << ' ' << pG[i]->c << "\n";
    }

    for (int i = 0; i < 23; ++i) {
        delete pF[i];
        delete pG[i];
    }
}

}