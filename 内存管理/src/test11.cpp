using namespace std;

#include <iostream>
#include <cassert>

namespace test12
{

void noMoreMemory() {
    cerr << "out of memory" << endl;
    abort();
}

void test_set_new_handler() {
    cout << "\ntest_set_new_handler()..........\n";
    set_new_handler(noMoreMemory);

    int* p = new int[100000000000000];
    assert(p);
}

}