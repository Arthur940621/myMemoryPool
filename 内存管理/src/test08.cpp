using namespace std;

#include <iostream>

namespace test08
{

class Airplane {
private:
    // 是类成员变量，仅是 AirplaneRep 结构体声明
    struct AirplaneRep {
        unsigned long miles;
        char type;
    };
private:
    union {
        AirplaneRep rep; // 此针对 used object，16 字节
        Airplane* next; // 此针对 free list，8 字节
    };
public:
    unsigned long getMiles() { return rep.miles; }
    char getType() { return rep.type; }
    void set(unsigned long m, char t) {
        rep.miles = m;
        rep.type = t;
    }
    static void* operator new(size_t size);
    static void  operator delete(void* deadObject, size_t size);
private:
    static const int BLOCK_SIZE;
    static Airplane* headOfFreeList;
};

Airplane* Airplane::headOfFreeList;  
const int Airplane::BLOCK_SIZE = 512;

void* Airplane::operator new(size_t size) {
    // 如果大小错误，转交给 ::operator new()
    if (size != sizeof(Airplane)) {
        return ::operator new(size);
    }
    Airplane* p = headOfFreeList;
    //如果 p 有效，就把 list 头部移往下一个元素
    if (p) {
        headOfFreeList = p->next;
    } else {
        // free list 已空。配置一大块够大的内存
        // 令足够容纳 BLOCK_SIZE 个 Airplanes
        Airplane* newBlock = static_cast<Airplane*>(::operator new(BLOCK_SIZE * sizeof(Airplane)));
        // 组成一个新的 free list: 將小块串在一起，但跳过 #0 元素，因为要将它传回给呼叫者
        for (int i = 1; i < BLOCK_SIZE-1; ++i) {
            newBlock[i].next = &newBlock[i+1];
        }
        // 以 null 结束
        newBlock[BLOCK_SIZE-1].next = 0;
        p = newBlock;
        headOfFreeList = &newBlock[1];
    }
    return p;
}

// operator delete 接收一块内存，如果它的大小正确，就把它加到 free list 的前端
void Airplane::operator delete(void* deadObject, size_t size) {
    if (deadObject == 0) return;
    if (size != sizeof(Airplane)) {
        ::operator delete(deadObject);
        return;
    }
    Airplane *carcass = static_cast<Airplane*>(deadObject);
    carcass->next = headOfFreeList;
    headOfFreeList = carcass;
}


void test_per_class_allocator_2() {
    cout << "\ntest_per_class_allocator_2()..........\n";
    
    cout << sizeof(Airplane) << endl; // 16

    size_t const N = 100;
    Airplane* p[N];

    for (int i = 0; i < N; ++i) {
        p[i] = new Airplane;
    }

    // 随机测试 object 是否正常
    p[1]->set(1000, 'A'); 	
    p[5]->set(2000, 'B');
    p[9]->set(500000, 'C');
    cout << p[1] << ' ' << p[1]->getType() << ' ' << p[1]->getMiles() << endl;
    cout << p[5] << ' ' << p[5]->getType() << ' ' << p[5]->getMiles() << endl;
    cout << p[9] << ' ' << p[9]->getType() << ' ' << p[9]->getMiles() << endl;

    cout << "member operator new:" << endl;
    // 输出前 10 个 pointers，用以比较其间隔
    for (int i = 0; i < 10; ++i) {
        cout << p[i] << endl;
    }
    for (int i = 0; i < N; ++i) {
        delete p[i];
    }

    Airplane* p1[N];
    cout << "global opeartor new:" << endl;
    for (int i = 0; i < N; ++i) {
        p1[i] = ::new Airplane;
    }
    // 输出前 10 个 pointers，用以比较其间隔
    for (int i = 0; i < 10; ++i) {
        cout << p1[i] << endl;
    }
    for (int i = 0; i < N; ++i) {
        ::delete p1[i];
    }

}

}