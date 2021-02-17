#include "pch.h"

namespace base_test
{
    TEST_CLASS(point_test)
    {
    public:
        TEST_METHOD(construct)
        {
            ff::point_int pt(2, 4);
            Assert::AreEqual(2, pt.x);
            Assert::AreEqual(4, pt.y);

            pt = ff::point_int{};
            Assert::AreEqual(0, pt.x);
            Assert::AreEqual(0, pt.y);

            pt = ff::point_int(1, 2);
            Assert::AreEqual(1, pt.x);
            Assert::AreEqual(2, pt.y);
        }

        TEST_METHOD(operators)
        {
            ff::point_int pt1(2, 4);
            ff::point_int pt2(8, 2);

            Assert::IsTrue((pt1 + pt2) == ff::point_int(10, 6));
            Assert::IsTrue((pt1 - pt2) == ff::point_int(-6, 2));
            Assert::IsTrue((pt1 * pt2) == ff::point_int(16, 8));
            Assert::IsTrue((pt1 / pt2) == ff::point_int(0, 2));

            Assert::IsTrue(pt2 * 2 == ff::point_int(16, 4));
            Assert::IsTrue(pt2 / 2 == ff::point_int(4, 1));
        }

        TEST_METHOD(cast)
        {
            ff::point_int pt1(8, 2);
            ff::point_float pt2(8.5f, 2.25f);
            ff::point_double pt3(8.5, 2.25);

            Assert::IsTrue(pt1.cast<float>() == ff::point_float(8.0f, 2.0f));
            Assert::IsTrue(pt2.cast<int>() == pt1);
            Assert::IsTrue(pt3.cast<int>() == pt1);
        }
    };
}
