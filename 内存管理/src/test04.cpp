using namespace std;

#include <iostream>
#include <list>
#include <vector>
#include <ext/pool_allocator.h> 

void* myAlloc(size_t size) {
    return malloc(size);
}

void myFree(void* ptr) {
    return free(ptr);
}

// 设计一个可以累计分配量和释放量的 operator new() / operator delete().
static long long countNew = 0;
static long long countDel = 0;
static long long countArrayNew = 0;
static long long countArrayDel = 0;
static long long timesNew = 0;

// // 不可被声明于 namespace 内

// inline void* operator new(size_t size) {
//     cout << "global new(ptr), " << size << endl;
//     countNew += size;
//     ++timesNew;
//     return myAlloc(size);
// }

// inline void* operator new[](size_t size) {
//     cout << "global new[](ptr), " << size << endl;
//     countArrayNew += size;
//     return myAlloc(size);
// }

// // 二选一
// // inline void operator delete(void* ptr, size_t size) {
// //     cout << "global delete(ptr, size), " << ptr << ", " << size << endl;	
// //     countDel += size;
// //     myFree(ptr);
// // }

// // inline void operator delete[](void* ptr, size_t size) {
// //     cout << "global delete[](ptr, size), " << ptr << ", " << size << endl;
// //     countArrayDel += size;
// //     myFree(ptr);		
// // }

// inline void operator delete(void* ptr) {
//     cout << "global delete(ptr), " << ptr << endl;	
//     myFree(ptr);
// }

// inline void operator delete[](void* ptr) {
//     cout << "global delete[](ptr, size), " << ptr << endl;
//     myFree(ptr);		
// }

template <typename T>
using listPool = list<T, __gnu_cxx::__pool_alloc<T>>;

namespace test04
{

void test_overload_global_new() {
    cout << "\ntest_overload_global_new()..........\n";
    // 测试时 main() 中的其他测试全都 remark，独留本测试
    {
        cout << "::countNew=" << ::countNew << endl;
        cout << "::countDel=" << ::countDel << endl;
        cout << "::timesNew=" << ::timesNew << endl;

        string* p = new string("Hello World!\n");
        delete p;
        cout << "::countNew=" << ::countNew << endl;
        cout << "::countDel=" << ::countDel << endl;
        cout << "::timesNew=" << ::timesNew << endl;

        p = new string[3];
        delete [] p;
        cout << "::countArrayNew=" << ::countArrayNew << endl;
        cout << "::countArrayDel=" << ::countArrayDel << endl;
    
        // 测试: global operator new 也会给容器带来影响
        vector<int> vec(10);
        vec.push_back(1);
        vec.push_back(1);
        vec.push_back(1);
        cout << "::countNew=" << ::countNew << endl;
        cout << "::timesNew=" << ::timesNew << endl;

        list<int> lst;
        lst.push_back(1);
        lst.push_back(1);
        lst.push_back(1);
        cout << "::countNew=" << ::countNew << endl;
        cout << "::timesNew=" << ::timesNew << endl;
    }

    {
        countNew = 0;
        timesNew = 0;

        listPool<double> lst;
        for (int i = 0; i < 1000000; ++i) {
            lst.push_back(i);
        }
        cout << "::countNew=" << ::countNew << endl;
        cout << "::timesNew=" << ::timesNew << endl;
    }
}

}