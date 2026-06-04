#include "pch.h"

static bool test_assert_handler(const char* /*exp*/, const char* /*text*/, const char* /*file*/, unsigned int /*line*/)
{
    Assert::Fail(L"Assert failure during test.");
    return true;
}

TEST_MODULE_INITIALIZE(module_init)
{
    ff::assert_listener(&::test_assert_handler);
}

TEST_MODULE_CLEANUP(module_cleanup)
{
    ff::assert_listener(nullptr);
}
