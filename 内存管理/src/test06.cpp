using namespace std;

#include <iostream>

namespace test06
{

class Foo {
public:
    Foo() { cout << "Foo::Foo()" << endl;  }
    Foo(int i) : m_i(i) {
        cout << "Foo::Foo(int)" << endl;
        throw "Bad()";
    }
    // 一般的 operator new() 的重载
    void* operator new(size_t size) {
        cout << "operator new(size_t size) size=" << size << endl;
        return malloc(size);
    }
    // placement new() 的重载
    void* operator new(size_t size, void* start) {
        cout << "operator new(size_t size, void* start) size=" << size << " start=" << start << endl;
        return start;
    }
    // placement new() 的重载
    void* operator new(size_t size, long extra) {
        cout << "operator new(size_t size, long extra) size=" << size << " extra=" << extra << endl;
        return malloc(size+extra);
    }
    // placement new() 的重载
    void* operator new(size_t size, long extra, char init) {
        cout << "operator new(size_t size, long extra, char init) size=" << size << " extra=" << extra << " init=" << init << endl;
        return malloc(size+extra);
    }

    // 这又是一个 placement new，但第一参数 type 错误，必须是 size_t
    // void* operator new(long extra, char init) {
    // cout << "op-new(long, char)" << endl;
    // return malloc(extra);
    // }

    // operator delete() 的重载
    // 当 ctor 抛出，对应的的 operator(placement) delete 就会被唤起
    void operator delete(void*, size_t) {
        cout << "operator delete(void*, size_t)" << endl;
    }
    void operator delete(void*, void*) {
        cout << "operator delete(void*, void*)" << endl;
    }
    void operator delete(void*, long) {
        cout << "operator delete(void*, long)" << endl;
    }
    void operator delete(void*, long, char) {
        cout << "operator delete(void*, long, char)" << endl;
    }
private:
    int m_i;
};

void test_overload_placement_new() {
    cout << "\ntest_overload_placement_new()..........\n";
    Foo start; // Foo::Foo
    Foo* p1 = new Foo; // op-new(size_t)
    Foo* p2 = new (&start) Foo; // op-new(size_t, void*)
    Foo* p3 = new (100) Foo; // op-new(size_t, long)
    Foo* p4 = new (100, 'a') Foo; // op-new(size_t, long, char)

    try {
        Foo* p5 = new (100) Foo(1); // op-new(size_t, long) op-del(void*, long)
    } catch (const char* error) {
        cout << error << endl;
    }
    try {
        Foo* p6 = new (100, 'a') Foo(1);
    } catch (const char* error) {
        cout << error << endl;
    }
    try {
        Foo* p7 = new (&start) Foo(1);
    } catch (const char* error) {
        cout << error << endl;
    }
    try {
        Foo* p8 = new Foo(1);
    } catch (const char* error) {
        cout << error << endl;
    }
}

}