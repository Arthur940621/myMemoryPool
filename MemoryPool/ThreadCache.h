#pragma once

#include "Common.h"

class ThreadCache {

private:
    FreeList _freelist[NLISTS]; // 自由链表
};