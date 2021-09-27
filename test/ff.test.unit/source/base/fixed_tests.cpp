#include "pch.h"

namespace ff::test::base
{
    TEST_CLASS(fixed_tests)
    {
    public:
        TEST_METHOD(i32f8_type)
        {
            Assert::IsTrue(ff::fixed_int(3.25) + ff::fixed_int(5.75) == ff::fixed_int(9));
            Assert::IsTrue(ff::fixed_int(9) - ff::fixed_int(5.75) == ff::fixed_int(3.25));
            Assert::IsTrue(ff::fixed_int(3.25) - ff::fixed_int(5.75) == ff::fixed_int(-2.5));

            Assert::IsTrue(ff::fixed_int(3) * ff::fixed_int(5) == ff::fixed_int(15));
            Assert::IsTrue(ff::fixed_int(3.5) * ff::fixed_int(5) == ff::fixed_int(17.5));
            Assert::IsTrue(ff::fixed_int(17.5) / ff::fixed_int(3.5) == ff::fixed_int(5));

            Assert::IsTrue(ff::fixed_int(-3) * ff::fixed_int(-5) == ff::fixed_int(15));
            Assert::IsTrue(ff::fixed_int(-3.5) * ff::fixed_int(-5) == ff::fixed_int(17.5));
            Assert::IsTrue(ff::fixed_int(-17.5) / ff::fixed_int(-3.5) == ff::fixed_int(5));

            Assert::IsTrue(ff::fixed_int(-3) * ff::fixed_int(5) == ff::fixed_int(-15));
            Assert::IsTrue(ff::fixed_int(-3.5) * ff::fixed_int(5) == ff::fixed_int(-17.5));
            Assert::IsTrue(ff::fixed_int(-17.5) / ff::fixed_int(3.5) == ff::fixed_int(-5));

            Assert::IsTrue(ff::fixed_int(-2.125) + ff::fixed_int(0.125) == ff::fixed_int(-2));
            Assert::IsTrue(ff::fixed_int(-2.125) - ff::fixed_int(0.125) == ff::fixed_int(-2.25));

            ff::fixed_int i(4.75);
            Assert::IsTrue(i == ff::fixed_int(4.75));
            Assert::IsTrue((i += .25) == ff::fixed_int(5));
            Assert::IsTrue((i -= .75) == ff::fixed_int(4.25));
            Assert::IsTrue((i -= 8) == ff::fixed_int(-3.75));
            Assert::IsTrue((i -= .25) == ff::fixed_int(-4));

            Assert::IsTrue(ff::fixed_int(22.5) / 5 == ff::fixed_int(4.5));
            Assert::IsTrue(ff::fixed_int(22.5) * 5 == ff::fixed_int(112.5));

            Assert::IsTrue(ff::fixed_int(2.3).abs() == ff::fixed_int(2.3));
            Assert::IsTrue(ff::fixed_int(-2.3).abs() == ff::fixed_int(2.3));
            Assert::IsTrue(ff::fixed_int(2.3).floor() == ff::fixed_int(2));
            Assert::IsTrue(ff::fixed_int(2.3).ceil() == ff::fixed_int(3));
            Assert::IsTrue(ff::fixed_int(2.3).trunc() == ff::fixed_int(2));
            Assert::IsTrue(ff::fixed_int(-2.3).floor() == ff::fixed_int(-3));
            Assert::IsTrue(ff::fixed_int(-2.3).ceil() == ff::fixed_int(-2));
            Assert::IsTrue(ff::fixed_int(-2.3).trunc() == ff::fixed_int(-2));

            Assert::IsTrue(std::max(ff::fixed_int(70), ff::fixed_int(-70)) == ff::fixed_int(70));
            Assert::IsTrue(std::min(ff::fixed_int(70), ff::fixed_int(-70)) == ff::fixed_int(-70));
        }
    };
}
