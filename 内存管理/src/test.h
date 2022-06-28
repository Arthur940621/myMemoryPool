#pragma once

namespace test01
{
void test_primitives();
}

namespace test02
{
void test_call_ctor_directly();
}

namespace test03
{
void test_array_new_and_placement_new();
}

namespace test04
{
void test_overload_global_new();
}

namespace test05
{
void test_overload_operator_new_and_array_new();
}

namespace test06
{
    void test_overload_placement_new();
}

namespace test07
{
    void test_per_class_allocator_1();
}

namespace test08
{
    void test_per_class_allocator_2();
}

namespace test09
{
    void test_static_allocator_3();
}

namespace test10
{
    void test_macros_for_static_allocator();
}


namespace test11
{
    void test_set_new_handler();
}

namespace test12
{
    void test_delete_and_default_for_new();
}

namespace test13
{
    void test_G29_alloc();
}