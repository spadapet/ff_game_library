#include "pch.h"

template<bool ThreadSafe>
static void pool_allocator_test()
{
    typedef std::tuple<int, float> test_data;
    ff::pool_allocator<test_data, ThreadSafe> pool;
    std::vector<test_data*> all;
    all.reserve(256);

    size_t size, allocated;
    pool.get_stats(&size, &allocated);
    Assert::AreEqual<size_t>(size, 0);
    Assert::AreEqual<size_t>(allocated, 0);

    for (int repeat = 0; repeat < 2; repeat++)
    {
        for (int i = 0; i < 256; i++)
        {
            test_data* data = pool.new_obj();
            all.push_back(data);
            *data = test_data(i, (float)i);
        }

        for (int i = 256 - 1; i >= 0; i--)
        {
            Assert::AreEqual(std::get<0>(*all[i]), i);
            Assert::AreEqual(std::get<1>(*all[i]), static_cast<float>(i));
        }

        pool.get_stats(&size, &allocated);
        Assert::AreEqual<size_t>(size, 256);
        Assert::AreEqual<size_t>(allocated, 504);

        for (int i = 0; i < 256; i++)
        {
            pool.delete_obj(all[i]);
        }

        all.clear();

        pool.get_stats(&size, &allocated);
        Assert::AreEqual<size_t>(size, 0);
        Assert::AreEqual<size_t>(allocated, 504);

        pool.reduce_if_empty();

        pool.get_stats(&size, &allocated);
        Assert::AreEqual<size_t>(size, 0);
        Assert::AreEqual<size_t>(allocated, 0);
    }
}

namespace base_test
{
    TEST_CLASS(pool_allocator_test)
    {
    public:
        TEST_METHOD(basic_thread_safe)
        {
            ::pool_allocator_test<false>();
        }

        TEST_METHOD(basic_thread_unsafe)
        {
            ::pool_allocator_test<false>();
        }
    };
}
