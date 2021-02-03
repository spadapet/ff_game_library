#include "pch.h"

namespace graphics_test
{
    TEST_CLASS(viewport_test)
    {
    public:
        TEST_METHOD(with_padding)
        {
            ff::viewport view(ff::point_int(1920, 1080), ff::rect_int(8, 8, 8, 8));

            Assert::IsTrue(view.view(ff::point_int(100, 200)) == ff::rect_int(8, 76, 92, 123));
            Assert::IsTrue(view.view(ff::point_int(100, 12)) == ff::rect_int(39, 0, 60, 12));
        }
    };
}
