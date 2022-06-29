#include <vector>
#include "ObjectPool.h"

void test_ObjectPool() {
    // 申请释放的轮次
    const size_t Rounds = 5;
    // 每轮申请释放多少次
    const size_t N = 10000;
    
    std::cout << "sizeof(std::string*) = " << sizeof(std::string*) << std::endl; // 8
    
    std::vector<std::string*> v1;
    v1.reserve(N);

    size_t begin1 = clock();
    for (size_t i = 0; i != Rounds; ++i) {
        for (int j = 0; j != N; ++j) {
            v1.push_back(new std::string());
        }
        for (int j = 0; j != N; ++j) {
            delete v1[j];
        }
        v1.clear();
    }
    size_t end1 = clock();

    /*-------------------------------------------*/

    std::vector<std::string*> v2;
    v2.reserve(N);
    ObjectPool<std::string> str_pool;

    size_t begin2 = clock();
    for (size_t i = 0; i != Rounds; ++i) {
        for (int j = 0; j != N; ++j) {
            v2.push_back(str_pool.New());
        }
        for (int j = 0; j != N; ++j) {
            str_pool.Delete(v2[j]);
        }
        v2.clear();
    }
    size_t end2 = clock();

    /*-------------------------------------------*/

    std::cout << "new cost time:" << end1 - begin1 << std::endl;
    std::cout << "object pool cost time:" << end2 - begin2 <<std:: endl;
}

int main() {
    test_ObjectPool();
    return 0;
}