using namespace std;

#include <iostream>

namespace test05
{

class Foo {
public:
    int _id;
    long _data;
    string _str;
    static void* operator new(size_t size);
    static void operator delete(void* deadObject, size_t size);
    static void* operator new[](size_t size);
    static void operator delete[](void* deadObject, size_t size);
    Foo() : _id(0)      { cout << "default ctor. this=" << this << " id=" << _id << endl;  }
    Foo(int i) : _id(i) { cout << "ctor. this=" << this << " id=" << _id << endl;  }
    // virtual
    ~Foo()              { cout << "dtor. this=" << this << " id=" << _id << endl;  }
    // 不加 virtual dtor，sizeof = 48，因为 4(int) + 8(long) + 32(string) = 44，再进行对齐，new Foo[5] => operator new[]() 的 size 参数是 48 * 5 + 4 = 248
    // 加了 virtual dtor，sizeof = 56，因为有虚函数表指针，new Foo[5] => operator new[]() 的 size 参数是 56 * 5 + 4 = 288
};

void* Foo::operator new(size_t size) {
    Foo* p = (Foo*)malloc(size);
    cout << "Foo::operator new() size=" << size << " return: " << p << endl;
    return p;
}

void Foo::operator delete(void* pdead, size_t size) {
    cout << "Foo::operator delete() pdead=" << pdead << " size=" << size << endl;
    free(pdead);
}

void* Foo::operator new[](size_t size) {
    Foo* p = (Foo*)malloc(size);
    cout << "Foo::operator new[]() size=" << size << " return: " << p << endl;
    return p;
}

void Foo::operator delete[](void* pdead, size_t size) {
    cout << "Foo::operator delete[]() pdead=" << pdead << " size=" << size << endl;
    free(pdead);
}

void test_overload_operator_new_and_array_new() {
    cout << "\ntest_overload_operator_new_and_array_new()..........\n";

    cout << "sizeof(Foo)=" << sizeof(Foo) << endl;

    {
        Foo* p = new Foo(7);
        delete p;

        Foo* pArray = new Foo[5];
        delete [] pArray;
    }

    {
        cout << "testing global expression ::new and ::new[]..........\n";
        // 这会绕过 overloaded new()，delete()，new[]()，delete[]()
        // 但 ctor，dtor 都会被正常呼叫

        Foo* p = ::new Foo(7);
        ::delete p;

        Foo* pArray = ::new Foo[5];
        ::delete [] pArray;
    }
}

}