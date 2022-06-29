#pragma once

#include <thread>
#include <mutex>

const size_t NLISTS = 184; // 数组元素总的有多少个，由对齐规则计算得来

class FreeList {
public:
private:
    void* _list = nullptr;
    size_t _size = 0; // 记录有多少个对象
    size_t _max_size = 1;
};

