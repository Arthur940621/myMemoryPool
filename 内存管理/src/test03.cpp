using namespace std;

#include <iostream>

namespace test03
{

class A {
public:
    int id;
    A() : id(0)      { cout << "default ctor. this=" << this << " id=" << id << endl; }
    A(int i) : id(i) { cout << "ctor. this=" << this << " id=" << id << endl; }
    ~A()             { cout << "dtor. this=" << this << " id=" << id << endl; }
};

void test_array_new_and_placement_new() {
    cout << "\ntest_placement_new()..........\n";

    size_t size = 3;

    {
        // case 1
        // 模拟 memory pool 的作法，array new + placement new。崩潰
        A* buf = reinterpret_cast<A*>(new char[sizeof(A)*size]);
        A* tmp = buf;

        cout << sizeof(A) << endl; // A 类型占 4 个字节

        for (int i = 0; i < size; ++i) {
            new (tmp++) A(i); // 3 次 ctor
        }

        cout << "buf=" << buf << " tmp=" << tmp << endl; // buf=0x55c77511a2c0 tmp=0x55c77511a2cc

        // 错误，这其实是个 char array，看到 delete[] buf，编译器会试图唤起多次 A::~A()，但 array memory layout 中找不到与 array 元素个数相关的信息
        // delete[] buf;

        // dtor just one time
        delete buf; // dtor. this=0x55ff33cfa2c0 id=0
    }

    {
        // case 2
        // 测试 array new
        A* buf = new A[size]; // default ctor 3 次，[0] 先于 [1] 先于 [2])
        A* tmp = buf;

        for (int i = 0; i < size; ++i) {
            new (tmp++) A(i); // 3 次 ctor
        }

        cout << "buf=" << buf << " tmp=" << tmp << endl; // buf=0x55c77511a2c8 tmp=0x55c77511a2d4

        delete[] buf; // dtor 3 次，次序逆反，[2] 先于 [1] 先于 [0]
    }

    {
        // case 3
        class Demo {
        public:
            int i1;
            int i2;
            int i3;
            Demo() : i1(1), i2(2), i3(3) { cout << "default ctor. this=" << this << endl; }
            ~Demo() { cout << "dtor. this=" << this << endl; }
        };

        cout << sizeof(Demo) << endl; // 12
        Demo* p = new Demo[3];
        // 错误，布局乱了
        // delete p;
        delete[] p;
    }
}

}