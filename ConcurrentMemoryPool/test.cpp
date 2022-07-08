#include "ConcurrentAllocate.h"
#include <atomic>
#include <vector>

using namespace std;

const int thread_num = 2;

void worker(int loop) {
    vector<void*> vec;
    for (size_t i = 0; i < loop; i++) {
        size_t bytes = rand() % MAX_BYTES + 1;
        vec.push_back(concurrent_allocate(bytes));
    }
    for (size_t i = 0; i < loop; i++) {
        concurrent_free(vec[i]);
    }
}

#include <fstream>
int main() {
    thread th[thread_num];
    const int loop = 10000;
    for (int i = 0; i != thread_num; ++i) {
        th[i] = thread(worker, loop);
    }
    for (int i = 0; i != thread_num; ++i) {
        th[i].join();
    }

    return 0;
}