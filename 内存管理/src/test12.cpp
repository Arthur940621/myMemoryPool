using namespace std;

#include <iostream>

namespace test13
{

class Foo {
public:
    long _x;
    Foo(long x=0) : _x(x) { }

    // static void* operator new[](size_t size) = default;
    // static void  operator delete[](void* pdead, size_t size) = default;

    static void* operator new[](size_t size) = delete;
    static void  operator delete[](void* pdead, size_t size) = delete;
};

class Goo {
public:
    long _x;
    Goo(long x = 0) : _x(x) {  }

    // static void* operator new(size_t size) = default;
    // static void  operator delete(void* pdead, size_t size) = default;

    static void* operator new(size_t size) = delete;
    static void  operator delete(void* pdead, size_t size) = delete;
};

void test_delete_and_default_for_new() {
    cout << "\ntest_delete_and_default_for_new()..........\n";

    Foo* p1 = new Foo(5);
    delete p1;
    // Foo* pF = new Foo[10];
    // delete[] pF;

    // Goo* p2 = new Goo(7);
    // delete p2;
    Goo* pG = new Goo[10];
    delete[] pG;
}

}