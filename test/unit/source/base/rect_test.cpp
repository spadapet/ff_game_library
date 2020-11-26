#include "pch.h"

namespace base_test
{
    TEST_CLASS(rect_test)
    {
    public:
        TEST_METHOD(construct)
        {
            ff::rect_int rt(2, 4, 8, 16);
            Assert::AreEqual(2, rt.left);
            Assert::AreEqual(4, rt.top);
            Assert::AreEqual(8, rt.right);
            Assert::AreEqual(16, rt.bottom);

            rt = ff::rect_int::zeros();
            Assert::AreEqual(0, rt.left);
            Assert::AreEqual(0, rt.top);
            Assert::AreEqual(0, rt.right);
            Assert::AreEqual(0, rt.bottom);
            Assert::IsTrue(rt == ff::rect_int::zeros());
        }

        TEST_METHOD(operators)
        {
            ff::rect_int rt1(2, 4, 8, 16);
            ff::point_int pt2(8, 4);

            Assert::IsTrue((rt1 + pt2) == ff::rect_int(10, 8, 16, 20));
            Assert::IsTrue((rt1 - pt2) == ff::rect_int(-6, 0, 0, 12));
            Assert::IsTrue((rt1 * pt2) == ff::rect_int(16, 16, 64, 64));
            Assert::IsTrue((rt1 / pt2) == ff::rect_int(0, 1, 1, 4));

            Assert::IsTrue(rt1 * 2 == ff::rect_int(4, 8, 16, 32));
            Assert::IsTrue(rt1 / 2 == ff::rect_int(1, 2, 4, 8));
        }

        TEST_METHOD(cast)
        {
            ff::rect_int rt1(8, 2, 16, 4);
            ff::rect_float rt2(8.5f, 2.25f, 16.125f, 4.75f);
            ff::rect_double rt3(8.5, 2.25, 16.125, 4.75);

            Assert::IsTrue(rt1.cast<float>() == ff::rect_float(8.0f, 2.0f, 16.0f, 4.0f));
            Assert::IsTrue(rt2.cast<int>() == rt1);
            Assert::IsTrue(rt3.cast<int>() == rt1);
        }
    };
}
