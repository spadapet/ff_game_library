#include "pch.h"
#include "test_base.h"

void ff::test::dx12::test_base::test_initialize()
{
    auto test_lock = std::make_unique<std::scoped_lock<std::mutex>>(this->test_mutex);

    Assert::IsNull(this->test_lock.get());
    this->test_lock = std::move(test_lock);
}

void ff::test::dx12::test_base::test_cleanup()
{
    Assert::IsNotNull(this->test_lock.get());
    this->test_lock.reset();
}
