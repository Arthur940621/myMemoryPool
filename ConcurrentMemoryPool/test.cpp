#include "ConcurrentAllocate.h"
#include <atomic>
#include <vector>

using namespace std;

const int thread_num = 4;

void worker(int loop) {
    vector<void*> vec;
    for (size_t i = 0; i < loop; i++) {
        vec.push_back(concurrent_allocate((16 + i) % 4096 + 1));
    }
    for (size_t i = 0; i < loop; i++) {
        concurrent_free(vec[i]);
    }
}

mutex mtx;

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