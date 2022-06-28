using namespace std;

#include <iostream>

namespace test02
{

class A {
public:
    int id;
    A() : id(0)      { cout << "default ctor. this=" << this << " id=" << id << endl; }
    A(int i) : id(i) { cout << "ctor. this=" << this << " id=" << id << endl; }
    ~A()             { cout << "dtor. this=" << this << " id=" << id << endl; }
};

void test_call_ctor_directly() {
    cout << "\ntest_call_ctor_directly()..........\n";

    string* pstr = new string;
    cout << "str=" << *pstr << endl;

    // 错误，不能主动调用构造函数
    // 类 "std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>" 没有成员 "string"
    // pstr->string::string("test_call_ctor_directly");
    // 正确，可以主动调用析构函数
    // pstr->~string();
    // cout << "str=" << *pstr << endl;

    //------------

    A* pA = new A(1); // ctor. this=0x559da549e2f0 id=1
    cout << pA->id << endl; // 1
    delete pA; // dtor. this=0x55671f3a92f0 id=1

    void* p = ::operator new(sizeof(A)); // 没有调用构造函数
    cout << "p=" << p << endl; // p=0x55b619f812f0
    pA = static_cast<A*>(p);
    pA->~A(); // dtor. this=0x55e32eb9d2f0 id=0
    ::operator delete(pA); // free()
}

}