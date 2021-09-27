#pragma once

namespace ff::test::dx12
{
    class test_base
    {
    protected:
        void test_initialize();
        void test_cleanup();

    private:
        std::mutex test_mutex;
        std::unique_ptr<std::scoped_lock<std::mutex>> test_lock;
    };
}
