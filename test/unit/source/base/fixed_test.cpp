#include "pch.h"

namespace base_test
{
    TEST_CLASS(fixed_test)
    {
    public:
        TEST_METHOD(int32_fixed8_type)
        {
            Assert::IsTrue(ff::int32_fixed8_t(3.25) + ff::int32_fixed8_t(5.75) == ff::int32_fixed8_t(9));
            Assert::IsTrue(ff::int32_fixed8_t(9) - ff::int32_fixed8_t(5.75) == ff::int32_fixed8_t(3.25));
            Assert::IsTrue(ff::int32_fixed8_t(3.25) - ff::int32_fixed8_t(5.75) == ff::int32_fixed8_t(-2.5));

            Assert::IsTrue(ff::int32_fixed8_t(3) * ff::int32_fixed8_t(5) == ff::int32_fixed8_t(15));
            Assert::IsTrue(ff::int32_fixed8_t(3.5) * ff::int32_fixed8_t(5) == ff::int32_fixed8_t(17.5));
            Assert::IsTrue(ff::int32_fixed8_t(17.5) / ff::int32_fixed8_t(3.5) == ff::int32_fixed8_t(5));

            Assert::IsTrue(ff::int32_fixed8_t(-3) * ff::int32_fixed8_t(-5) == ff::int32_fixed8_t(15));
            Assert::IsTrue(ff::int32_fixed8_t(-3.5) * ff::int32_fixed8_t(-5) == ff::int32_fixed8_t(17.5));
            Assert::IsTrue(ff::int32_fixed8_t(-17.5) / ff::int32_fixed8_t(-3.5) == ff::int32_fixed8_t(5));

            Assert::IsTrue(ff::int32_fixed8_t(-3) * ff::int32_fixed8_t(5) == ff::int32_fixed8_t(-15));
            Assert::IsTrue(ff::int32_fixed8_t(-3.5) * ff::int32_fixed8_t(5) == ff::int32_fixed8_t(-17.5));
            Assert::IsTrue(ff::int32_fixed8_t(-17.5) / ff::int32_fixed8_t(3.5) == ff::int32_fixed8_t(-5));

            Assert::IsTrue(ff::int32_fixed8_t(-2.125) + ff::int32_fixed8_t(0.125) == ff::int32_fixed8_t(-2));
            Assert::IsTrue(ff::int32_fixed8_t(-2.125) - ff::int32_fixed8_t(0.125) == ff::int32_fixed8_t(-2.25));

            ff::int32_fixed8_t i(4.75);
            Assert::IsTrue(i == ff::int32_fixed8_t(4.75));
            Assert::IsTrue((i += .25) == ff::int32_fixed8_t(5));
            Assert::IsTrue((i -= .75) == ff::int32_fixed8_t(4.25));
            Assert::IsTrue((i -= 8) == ff::int32_fixed8_t(-3.75));
            Assert::IsTrue((i -= .25) == ff::int32_fixed8_t(-4));

            Assert::IsTrue(ff::int32_fixed8_t(22.5) / 5 == ff::int32_fixed8_t(4.5));
            Assert::IsTrue(ff::int32_fixed8_t(22.5) * 5 == ff::int32_fixed8_t(112.5));

            Assert::IsTrue(ff::int32_fixed8_t(2.3).abs() == ff::int32_fixed8_t(2.3));
            Assert::IsTrue(ff::int32_fixed8_t(-2.3).abs() == ff::int32_fixed8_t(2.3));
            Assert::IsTrue(ff::int32_fixed8_t(2.3).floor() == ff::int32_fixed8_t(2));
            Assert::IsTrue(ff::int32_fixed8_t(2.3).ceil() == ff::int32_fixed8_t(3));
            Assert::IsTrue(ff::int32_fixed8_t(2.3).trunc() == ff::int32_fixed8_t(2));
            Assert::IsTrue(ff::int32_fixed8_t(-2.3).floor() == ff::int32_fixed8_t(-3));
            Assert::IsTrue(ff::int32_fixed8_t(-2.3).ceil() == ff::int32_fixed8_t(-2));
            Assert::IsTrue(ff::int32_fixed8_t(-2.3).trunc() == ff::int32_fixed8_t(-2));

            Assert::IsTrue(std::max(ff::int32_fixed8_t(70), ff::int32_fixed8_t(-70)) == ff::int32_fixed8_t(70));
            Assert::IsTrue(std::min(ff::int32_fixed8_t(70), ff::int32_fixed8_t(-70)) == ff::int32_fixed8_t(-70));
        }
    };
}
