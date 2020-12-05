#include "pch.h"

namespace data_test
{
    TEST_CLASS(value_test)
    {
    public:
        TEST_METHOD(int32_value)
        {
            ff::data::value_ptr val = ff::data::value::create<int32_t>(12);
            val.reset();
        }
    };
}
