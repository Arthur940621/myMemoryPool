using namespace std;

#include <iostream>
#include <complex>

namespace test09
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

// 检测还有多少内存块可用
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

class Foo {
public:
    long L;
    string str;
    static allocator myAlloc;
public:
    Foo(long l) : L(l) { }
    static void* operator new(size_t size) {
        return myAlloc.allocate(size);
    }
    static void  operator delete(void* pdead, size_t size) {
        return myAlloc.deallocate(pdead, size);
    }
};
allocator Foo::myAlloc;

class Goo {
public:
    complex<double> c;
    string str;
    static allocator myAlloc;
public:
    Goo(const complex<double>& x) : c(x) { }
    static void* operator new(size_t size) {
        return myAlloc.allocate(size);
    }
    static void  operator delete(void* pdead, size_t size) {
        return myAlloc.deallocate(pdead, size);
    }
};
allocator Goo::myAlloc;

void test_static_allocator_3() {
    cout << "\ntest_static_allocator()..........\n";
    {
        cout << "sizeof(Foo)=" << sizeof(Foo) << endl;
        Foo* p[100];
        for (int i = 0; i < 23; ++i) {
            p[i] = new Foo(i);
            cout << p[i] << ' ' << p[i]->L << endl;
        }
        Foo::myAlloc.check();

        for (int i = 0; i < 23; ++i) {
            delete p[i];
        }
        Foo::myAlloc.check();
    }

    {
        cout << "sizeof(Goo)=" << sizeof(Goo) << endl;
        Goo* p[100];
        
        for (int i = 0; i < 17; ++i) {
            p[i] = new Goo(complex<double>(i,i));
            cout << p[i] << ' ' << p[i]->c << endl;
        }
        Goo::myAlloc.check();

        for (int i = 0; i < 17; ++i) {
            delete p[i];
        }
        Goo::myAlloc.check();
    }

    {
        allocator a;
        a.allocate(1);
        a.check();
    }
}

}