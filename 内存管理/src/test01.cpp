using namespace std;

#include <iostream>
#include <complex>
#include <ext/pool_allocator.h> // 使用 std::allocator 以外的 allocator

namespace test01
{

void test_primitives() {
    cout << "\ntest_primitives()..........\n";

    void* p1 = malloc(512); // 512 bytes
    free(p1);

    complex<int>* p2 = new complex<int>; // one object
    delete p2;

    void* p3 = ::operator new(512); // 512 bytes
    ::operator delete(p3);

#ifdef __GNUC__
    printf("hello gcc %d\n", __GNUC__);
    // 以下两个函数都是 non-static，要通过 object 调用。分配 7 个 int
    int* p4 = allocator<int>().allocate(7);
    allocator<int>().deallocate(p4, 7);

    // 以下两个函数都是 non-static，要通过 object 调用。分配 9 个 int
    int* p5 = __gnu_cxx::__pool_alloc<int>().allocate(9);
    __gnu_cxx::__pool_alloc<int>().deallocate(p5, 9);
#endif
}

}