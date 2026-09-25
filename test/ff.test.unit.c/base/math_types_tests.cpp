#include "pch.h"
#include <DirectXMath.h>

namespace ff::test::base
{
    // The module-wide assert listener fails any test that trips an assert, which makes the
    // FF_ASSERT_RET_VAL guards unreachable from a test. Swapping in a counting listener for the
    // duration of one test lets the guards be verified instead of merely trusted.
    struct scoped_assert_counter
    {
        scoped_assert_counter()
        {
            scoped_assert_counter::count = 0;
            this->previous = ff_assert_listener(&scoped_assert_counter::handler);
        }

        ~scoped_assert_counter()
        {
            ff_assert_listener(this->previous);
        }

        static bool handler(const char*, const char*, const char*, unsigned int)
        {
            scoped_assert_counter::count++;
            return true;
        }

        static inline int count = 0;
        ff_assert_listener_func previous = nullptr;

        // FF_ASSERT_MSG compiles out in Release, so no listener runs there. The guard still
        // takes its early-out path, so only the assert count differs between configurations.
        static int expected_count(int debug_count)
        {
#ifdef _DEBUG
            return debug_count;
#else
            return 0;
#endif
        }
    };

    // The C matrix code is hand-written because DirectXMath is C++ only. These tests use
    // DirectXMath as an independent oracle: if the two disagree, the C code is wrong.
    static void assert_matrix_equal(const ff_matrix& actual, const DirectX::XMMATRIX& expected)
    {
        DirectX::XMFLOAT4X4 expected_values;
        DirectX::XMStoreFloat4x4(&expected_values, expected);

        for (size_t row = 0; row < 4; row++)
        {
            for (size_t column = 0; column < 4; column++)
            {
                float a = actual.m[row * 4 + column];
                float e = expected_values.m[row][column];
                Assert::IsTrue(std::abs(a - e) < 0.0001f);
            }
        }
    }

    TEST_CLASS(point_tests)
    {
    public:
        TEST_METHOD(make_and_equal)
        {
            ff_point_float a = ff_point_float_make(3.0f, 4.0f);
            Assert::AreEqual(3.0f, a.x);
            Assert::AreEqual(4.0f, a.y);
            Assert::IsTrue(ff_point_float_equal(a, ff_point_float_make(3.0f, 4.0f)));
            Assert::IsFalse(ff_point_float_equal(a, ff_point_float_make(3.0f, 5.0f)));
            Assert::IsFalse(ff_point_float_equal(a, ff_point_float_make(5.0f, 4.0f)));
        }

        TEST_METHOD(zero)
        {
            Assert::IsTrue(ff_point_float_equal(ff_point_float_zero(), ff_point_float_make(0.0f, 0.0f)));
            Assert::IsTrue(ff_point_int_equal(ff_point_int_zero(), ff_point_int_make(0, 0)));
        }

        TEST_METHOD(arithmetic)
        {
            ff_point_float a = ff_point_float_make(3.0f, 4.0f);
            ff_point_float b = ff_point_float_make(1.0f, 2.0f);

            Assert::IsTrue(ff_point_float_equal(ff_point_float_add(a, b), ff_point_float_make(4.0f, 6.0f)));
            Assert::IsTrue(ff_point_float_equal(ff_point_float_subtract(a, b), ff_point_float_make(2.0f, 2.0f)));
            Assert::IsTrue(ff_point_float_equal(ff_point_float_multiply(a, b), ff_point_float_make(3.0f, 8.0f)));
            Assert::IsTrue(ff_point_float_equal(ff_point_float_scale(a, 2.0f), ff_point_float_make(6.0f, 8.0f)));
        }

        TEST_METHOD(int_arithmetic)
        {
            ff_point_int a = ff_point_int_make(3, 4);
            ff_point_int b = ff_point_int_make(1, 2);

            Assert::IsTrue(ff_point_int_equal(ff_point_int_add(a, b), ff_point_int_make(4, 6)));
            Assert::IsTrue(ff_point_int_equal(ff_point_int_subtract(a, b), ff_point_int_make(2, 2)));
        }

        TEST_METHOD(conversions)
        {
            Assert::IsTrue(ff_point_float_equal(ff_point_int_to_float(ff_point_int_make(3, 4)), ff_point_float_make(3.0f, 4.0f)));
            Assert::IsTrue(ff_point_float_equal(ff_point_size_to_float(ff_point_size_make(3, 4)), ff_point_float_make(3.0f, 4.0f)));
            Assert::IsTrue(ff_point_int_equal(ff_point_float_to_int(ff_point_float_make(3.9f, -4.9f)), ff_point_int_make(3, -4)));
        }

        TEST_METHOD(float_to_int_truncates_toward_zero)
        {
            Assert::IsTrue(ff_point_int_equal(ff_point_float_to_int(ff_point_float_make(-0.9f, 0.9f)), ff_point_int_zero()));
        }

        TEST_METHOD(int_round_trip)
        {
            ff_point_int original = ff_point_int_make(-7, 12);
            Assert::IsTrue(ff_point_int_equal(ff_point_float_to_int(ff_point_int_to_float(original)), original));
        }

        TEST_METHOD(arithmetic_with_negatives)
        {
            ff_point_float a = ff_point_float_make(-3.0f, 4.0f);
            ff_point_float b = ff_point_float_make(2.0f, -5.0f);
            Assert::IsTrue(ff_point_float_equal(ff_point_float_add(a, b), ff_point_float_make(-1.0f, -1.0f)));
            Assert::IsTrue(ff_point_float_equal(ff_point_float_subtract(a, b), ff_point_float_make(-5.0f, 9.0f)));
            Assert::IsTrue(ff_point_float_equal(ff_point_float_multiply(a, b), ff_point_float_make(-6.0f, -20.0f)));
            Assert::IsTrue(ff_point_float_equal(ff_point_float_scale(a, -2.0f), ff_point_float_make(6.0f, -8.0f)));
        }

        TEST_METHOD(add_and_subtract_are_inverses)
        {
            ff_point_float a = ff_point_float_make(3.0f, 4.0f);
            ff_point_float b = ff_point_float_make(1.5f, -2.5f);
            Assert::IsTrue(ff_point_float_equal(ff_point_float_subtract(ff_point_float_add(a, b), b), a));

            ff_point_int c = ff_point_int_make(3, 4);
            ff_point_int d = ff_point_int_make(-1, 2);
            Assert::IsTrue(ff_point_int_equal(ff_point_int_subtract(ff_point_int_add(c, d), d), c));
        }

        TEST_METHOD(scale_by_zero_and_one)
        {
            ff_point_float a = ff_point_float_make(3.0f, 4.0f);
            Assert::IsTrue(ff_point_float_equal(ff_point_float_scale(a, 1.0f), a));
            Assert::IsTrue(ff_point_float_equal(ff_point_float_scale(a, 0.0f), ff_point_float_zero()));
        }
    };

    TEST_CLASS(rect_tests)
    {
    public:
        TEST_METHOD(make_and_equal)
        {
            ff_rect_float r = ff_rect_float_make(1.0f, 2.0f, 5.0f, 8.0f);
            Assert::AreEqual(1.0f, r.left);
            Assert::AreEqual(2.0f, r.top);
            Assert::AreEqual(5.0f, r.right);
            Assert::AreEqual(8.0f, r.bottom);
            Assert::IsTrue(ff_rect_float_equal(r, ff_rect_float_make(1.0f, 2.0f, 5.0f, 8.0f)));
            Assert::IsFalse(ff_rect_float_equal(r, ff_rect_float_make(1.0f, 2.0f, 5.0f, 9.0f)));
        }

        TEST_METHOD(size_and_area)
        {
            ff_rect_float r = ff_rect_float_make(1.0f, 2.0f, 5.0f, 8.0f);
            Assert::AreEqual(4.0f, ff_rect_float_width(r));
            Assert::AreEqual(6.0f, ff_rect_float_height(r));
            Assert::AreEqual(24.0f, ff_rect_float_area(r));
            Assert::IsTrue(ff_point_float_equal(ff_rect_float_size(r), ff_point_float_make(4.0f, 6.0f)));
            Assert::IsTrue(ff_point_float_equal(ff_rect_float_center(r), ff_point_float_make(3.0f, 5.0f)));
        }

        TEST_METHOD(corners)
        {
            ff_rect_float r = ff_rect_float_make(1.0f, 2.0f, 5.0f, 8.0f);
            Assert::IsTrue(ff_point_float_equal(ff_rect_float_top_left(r), ff_point_float_make(1.0f, 2.0f)));
            Assert::IsTrue(ff_point_float_equal(ff_rect_float_bottom_right(r), ff_point_float_make(5.0f, 8.0f)));
        }

        TEST_METHOD(from_size_and_corners)
        {
            ff_rect_float a = ff_rect_float_from_size(ff_point_float_make(1.0f, 2.0f), ff_point_float_make(4.0f, 6.0f));
            Assert::IsTrue(ff_rect_float_equal(a, ff_rect_float_make(1.0f, 2.0f, 5.0f, 8.0f)));

            ff_rect_float b = ff_rect_float_from_corners(ff_point_float_make(1.0f, 2.0f), ff_point_float_make(5.0f, 8.0f));
            Assert::IsTrue(ff_rect_float_equal(b, a));
        }

        TEST_METHOD(empty)
        {
            Assert::IsFalse(ff_rect_float_empty(ff_rect_float_make(0.0f, 0.0f, 1.0f, 1.0f)));
            Assert::IsTrue(ff_rect_float_empty(ff_rect_float_make(0.0f, 0.0f, 0.0f, 1.0f)));
            Assert::IsTrue(ff_rect_float_empty(ff_rect_float_make(0.0f, 0.0f, 1.0f, 0.0f)));
            Assert::IsTrue(ff_rect_float_empty(ff_rect_float_make(2.0f, 0.0f, 1.0f, 1.0f)));
            Assert::IsTrue(ff_rect_float_empty(ff_rect_float_zero()));
        }

        TEST_METHOD(contains)
        {
            ff_rect_float r = ff_rect_float_make(0.0f, 0.0f, 10.0f, 10.0f);
            Assert::IsTrue(ff_rect_float_contains(r, ff_point_float_make(0.0f, 0.0f)));
            Assert::IsTrue(ff_rect_float_contains(r, ff_point_float_make(5.0f, 5.0f)));
            Assert::IsFalse(ff_rect_float_contains(r, ff_point_float_make(10.0f, 5.0f)));
            Assert::IsFalse(ff_rect_float_contains(r, ff_point_float_make(5.0f, 10.0f)));
            Assert::IsFalse(ff_rect_float_contains(r, ff_point_float_make(-1.0f, 5.0f)));
        }

        TEST_METHOD(intersects_and_intersection)
        {
            ff_rect_float a = ff_rect_float_make(0.0f, 0.0f, 10.0f, 10.0f);
            ff_rect_float b = ff_rect_float_make(5.0f, 5.0f, 15.0f, 15.0f);
            ff_rect_float c = ff_rect_float_make(20.0f, 20.0f, 30.0f, 30.0f);

            Assert::IsTrue(ff_rect_float_intersects(a, b));
            Assert::IsFalse(ff_rect_float_intersects(a, c));
            Assert::IsTrue(ff_rect_float_equal(ff_rect_float_intersection(a, b), ff_rect_float_make(5.0f, 5.0f, 10.0f, 10.0f)));
            Assert::IsTrue(ff_rect_float_empty(ff_rect_float_intersection(a, c)));
        }

        TEST_METHOD(touching_rects_do_not_intersect)
        {
            ff_rect_float a = ff_rect_float_make(0.0f, 0.0f, 10.0f, 10.0f);
            ff_rect_float b = ff_rect_float_make(10.0f, 0.0f, 20.0f, 10.0f);
            Assert::IsFalse(ff_rect_float_intersects(a, b));
        }

        TEST_METHOD(boundary)
        {
            ff_rect_float a = ff_rect_float_make(0.0f, 0.0f, 10.0f, 10.0f);
            ff_rect_float b = ff_rect_float_make(5.0f, -5.0f, 15.0f, 8.0f);
            Assert::IsTrue(ff_rect_float_equal(ff_rect_float_boundary(a, b), ff_rect_float_make(0.0f, -5.0f, 15.0f, 10.0f)));
        }

        TEST_METHOD(normalize)
        {
            ff_rect_float r = ff_rect_float_make(10.0f, 8.0f, 2.0f, 1.0f);
            Assert::IsTrue(ff_rect_float_equal(ff_rect_float_normalize(r), ff_rect_float_make(2.0f, 1.0f, 10.0f, 8.0f)));
        }

        TEST_METHOD(offset_inflate_deflate)
        {
            ff_rect_float r = ff_rect_float_make(1.0f, 2.0f, 5.0f, 8.0f);
            Assert::IsTrue(ff_rect_float_equal(ff_rect_float_offset(r, 1.0f, 2.0f), ff_rect_float_make(2.0f, 4.0f, 6.0f, 10.0f)));
            Assert::IsTrue(ff_rect_float_equal(ff_rect_float_inflate(r, 1.0f, 1.0f), ff_rect_float_make(0.0f, 1.0f, 6.0f, 9.0f)));
            Assert::IsTrue(ff_rect_float_equal(ff_rect_float_deflate(r, 1.0f, 1.0f), ff_rect_float_make(2.0f, 3.0f, 4.0f, 7.0f)));
        }

        TEST_METHOD(move_top_left_keeps_size)
        {
            ff_rect_float r = ff_rect_float_make(1.0f, 2.0f, 5.0f, 8.0f);
            ff_rect_float moved = ff_rect_float_move_top_left(r, ff_point_float_make(10.0f, 20.0f));
            Assert::IsTrue(ff_rect_float_equal(moved, ff_rect_float_make(10.0f, 20.0f, 14.0f, 26.0f)));
            Assert::IsTrue(ff_point_float_equal(ff_rect_float_size(moved), ff_rect_float_size(r)));
        }

        TEST_METHOD(scale)
        {
            ff_rect_float r = ff_rect_float_make(1.0f, 2.0f, 5.0f, 8.0f);
            Assert::IsTrue(ff_rect_float_equal(ff_rect_float_scale(r, ff_point_float_make(2.0f, 3.0f)), ff_rect_float_make(2.0f, 6.0f, 10.0f, 24.0f)));
        }

        TEST_METHOD(int_helpers_and_conversions)
        {
            ff_rect_int r = ff_rect_int_make(1, 2, 5, 8);
            Assert::AreEqual(4, ff_rect_int_width(r));
            Assert::AreEqual(6, ff_rect_int_height(r));
            Assert::IsTrue(ff_rect_int_equal(r, ff_rect_int_make(1, 2, 5, 8)));
            Assert::IsTrue(ff_rect_int_empty(ff_rect_int_zero()));
            Assert::IsTrue(ff_rect_float_equal(ff_rect_int_to_float(r), ff_rect_float_make(1.0f, 2.0f, 5.0f, 8.0f)));
            Assert::IsTrue(ff_rect_float_equal(ff_rect_size_to_float(ff_rect_size_make(1, 2, 5, 8)), ff_rect_float_make(1.0f, 2.0f, 5.0f, 8.0f)));
            Assert::IsTrue(ff_rect_int_equal(ff_rect_float_to_int(ff_rect_float_make(1.9f, 2.9f, 5.9f, 8.9f)), ff_rect_int_make(1, 2, 5, 8)));
        }

        TEST_METHOD(negative_and_inverted_rects)
        {
            ff_rect_float r = ff_rect_float_make(-10.0f, -8.0f, -2.0f, -1.0f);
            Assert::AreEqual(8.0f, ff_rect_float_width(r));
            Assert::AreEqual(7.0f, ff_rect_float_height(r));
            Assert::IsFalse(ff_rect_float_empty(r));
            Assert::IsTrue(ff_point_float_equal(ff_rect_float_center(r), ff_point_float_make(-6.0f, -4.5f)));

            ff_rect_float inverted = ff_rect_float_make(5.0f, 2.0f, 1.0f, 8.0f);
            Assert::AreEqual(-4.0f, ff_rect_float_width(inverted));
            Assert::IsTrue(ff_rect_float_empty(inverted));
        }

        TEST_METHOD(normalize_is_idempotent_and_preserves_normal_rects)
        {
            ff_rect_float normal = ff_rect_float_make(1.0f, 2.0f, 5.0f, 8.0f);
            Assert::IsTrue(ff_rect_float_equal(ff_rect_float_normalize(normal), normal));

            ff_rect_float once = ff_rect_float_normalize(ff_rect_float_make(10.0f, 8.0f, 2.0f, 1.0f));
            Assert::IsTrue(ff_rect_float_equal(ff_rect_float_normalize(once), once));
        }

        TEST_METHOD(intersection_of_identical_rects_is_itself)
        {
            ff_rect_float r = ff_rect_float_make(1.0f, 2.0f, 5.0f, 8.0f);
            Assert::IsTrue(ff_rect_float_equal(ff_rect_float_intersection(r, r), r));
            Assert::IsTrue(ff_rect_float_equal(ff_rect_float_boundary(r, r), r));
            Assert::IsTrue(ff_rect_float_intersects(r, r));
        }

        TEST_METHOD(contained_rect_intersection_and_boundary)
        {
            ff_rect_float outer = ff_rect_float_make(0.0f, 0.0f, 10.0f, 10.0f);
            ff_rect_float inner = ff_rect_float_make(2.0f, 2.0f, 8.0f, 8.0f);
            Assert::IsTrue(ff_rect_float_equal(ff_rect_float_intersection(outer, inner), inner));
            Assert::IsTrue(ff_rect_float_equal(ff_rect_float_boundary(outer, inner), outer));
        }

        TEST_METHOD(intersection_and_boundary_are_commutative)
        {
            ff_rect_float a = ff_rect_float_make(0.0f, 0.0f, 10.0f, 10.0f);
            ff_rect_float b = ff_rect_float_make(5.0f, -5.0f, 15.0f, 8.0f);
            Assert::IsTrue(ff_rect_float_equal(ff_rect_float_intersection(a, b), ff_rect_float_intersection(b, a)));
            Assert::IsTrue(ff_rect_float_equal(ff_rect_float_boundary(a, b), ff_rect_float_boundary(b, a)));
            Assert::AreEqual(ff_rect_float_intersects(a, b), ff_rect_float_intersects(b, a));
        }

        TEST_METHOD(inflate_and_deflate_round_trip)
        {
            ff_rect_float r = ff_rect_float_make(1.0f, 2.0f, 5.0f, 8.0f);
            Assert::IsTrue(ff_rect_float_equal(ff_rect_float_deflate(ff_rect_float_inflate(r, 3.0f, 4.0f), 3.0f, 4.0f), r));
            Assert::IsTrue(ff_rect_float_equal(ff_rect_float_offset(ff_rect_float_offset(r, 3.0f, 4.0f), -3.0f, -4.0f), r));
        }

        TEST_METHOD(offset_preserves_size)
        {
            ff_rect_float r = ff_rect_float_make(1.0f, 2.0f, 5.0f, 8.0f);
            Assert::IsTrue(ff_point_float_equal(ff_rect_float_size(ff_rect_float_offset(r, 7.0f, -3.0f)), ff_rect_float_size(r)));
        }

        TEST_METHOD(deflate_past_center_produces_empty)
        {
            ff_rect_float r = ff_rect_float_make(0.0f, 0.0f, 4.0f, 4.0f);
            Assert::IsTrue(ff_rect_float_empty(ff_rect_float_deflate(r, 3.0f, 3.0f)));
        }

        TEST_METHOD(zero_size_rect_is_empty_but_has_a_position)
        {
            ff_rect_float r = ff_rect_float_from_size(ff_point_float_make(5.0f, 6.0f), ff_point_float_zero());
            Assert::IsTrue(ff_rect_float_empty(r));
            Assert::AreEqual(0.0f, ff_rect_float_area(r));
            Assert::IsTrue(ff_point_float_equal(ff_rect_float_top_left(r), ff_point_float_make(5.0f, 6.0f)));
        }
    };

    TEST_CLASS(color_tests)
    {
    public:
        TEST_METHOD(rgba_round_trip)
        {
            ff_color c = ff_color_rgba(0.25f, 0.5f, 0.75f, 1.0f);
            Assert::IsTrue(c.type == ff_color_type_rgba);
            Assert::AreEqual(0.25f, c.rgba.r);
            Assert::AreEqual(0.5f, c.rgba.g);
            Assert::AreEqual(0.75f, c.rgba.b);
            Assert::AreEqual(1.0f, c.rgba.a);
            Assert::AreEqual(1.0f, ff_color_alpha(c));
        }

        TEST_METHOD(palette_round_trip)
        {
            ff_color c = ff_color_palette(7, 0.5f);
            Assert::IsTrue(c.type == ff_color_type_palette);
            Assert::AreEqual(7, c.palette.index);
            Assert::AreEqual(0.5f, c.palette.alpha);
            Assert::AreEqual(0.5f, ff_color_alpha(c));
        }

        TEST_METHOD(equality)
        {
            Assert::IsTrue(ff_color_equal(ff_color_rgba(1.0f, 0.0f, 0.0f, 1.0f), ff_color_red()));
            Assert::IsFalse(ff_color_equal(ff_color_red(), ff_color_blue()));
            Assert::IsTrue(ff_color_equal(ff_color_palette(3, 1.0f), ff_color_palette(3, 1.0f)));
            Assert::IsFalse(ff_color_equal(ff_color_palette(3, 1.0f), ff_color_palette(4, 1.0f)));
        }

        TEST_METHOD(rgba_and_palette_never_equal)
        {
            // Both cases occupy the same storage, so equality has to consider the type tag.
            Assert::IsFalse(ff_color_equal(ff_color_rgba(0.0f, 0.0f, 0.0f, 0.0f), ff_color_palette(0, 0.0f)));
        }

        TEST_METHOD(to_shader_rgba_passes_through)
        {
            ff_color_shader s = ff_color_to_shader(ff_color_rgba(0.25f, 0.5f, 0.75f, 0.5f), nullptr);
            Assert::AreEqual(0.25f, s.r);
            Assert::AreEqual(0.5f, s.g);
            Assert::AreEqual(0.75f, s.b);
            Assert::AreEqual(0.5f, s.a);
        }

        TEST_METHOD(to_shader_palette_encodes_index)
        {
            ff_color_shader s = ff_color_to_shader(ff_color_palette(64, 0.5f), nullptr);
            Assert::AreEqual(0.25f, s.r);
            Assert::AreEqual(0.0f, s.g);
            Assert::AreEqual(0.0f, s.b);
            Assert::AreEqual(0.5f, s.a);
        }

        TEST_METHOD(to_shader_palette_index_zero_is_transparent)
        {
            ff_color_shader s = ff_color_to_shader(ff_color_palette(0, 1.0f), nullptr);
            Assert::AreEqual(0.0f, s.a);
        }

        TEST_METHOD(to_shader_palette_applies_remap)
        {
            uint8_t remap[256];

            for (size_t i = 0; i < 256; i++)
            {
                remap[i] = (uint8_t)i;
            }

            remap[1] = 64;

            ff_color_shader s = ff_color_to_shader(ff_color_palette(1, 1.0f), remap);
            Assert::AreEqual(0.25f, s.r);
            Assert::AreEqual(1.0f, s.a);
        }

        TEST_METHOD(to_shader_remap_to_zero_is_transparent)
        {
            uint8_t remap[256] = { 0 };
            ff_color_shader s = ff_color_to_shader(ff_color_palette(5, 1.0f), remap);
            Assert::AreEqual(0.0f, s.a);
        }

        TEST_METHOD(named_colors)
        {
            Assert::IsTrue(ff_color_equal(ff_color_white(), ff_color_rgba(1.0f, 1.0f, 1.0f, 1.0f)));
            Assert::IsTrue(ff_color_equal(ff_color_black(), ff_color_rgba(0.0f, 0.0f, 0.0f, 1.0f)));
            Assert::AreEqual(0.0f, ff_color_alpha(ff_color_none()));
            Assert::IsTrue(ff_color_none_palette().type == ff_color_type_palette);
        }

        TEST_METHOD(named_colors_are_distinct_and_opaque)
        {
            ff_color colors[] =
            {
                ff_color_white(), ff_color_black(), ff_color_red(), ff_color_green(),
                ff_color_blue(), ff_color_yellow(), ff_color_cyan(), ff_color_magenta(),
            };

            for (size_t i = 0; i < std::size(colors); i++)
            {
                Assert::IsTrue(colors[i].type == ff_color_type_rgba);
                Assert::AreEqual(1.0f, ff_color_alpha(colors[i]));

                for (size_t j = i + 1; j < std::size(colors); j++)
                {
                    Assert::IsFalse(ff_color_equal(colors[i], colors[j]));
                }
            }
        }

        TEST_METHOD(secondary_colors_are_sums_of_primaries)
        {
            Assert::IsTrue(ff_color_equal(ff_color_yellow(), ff_color_rgba(1.0f, 1.0f, 0.0f, 1.0f)));
            Assert::IsTrue(ff_color_equal(ff_color_cyan(), ff_color_rgba(0.0f, 1.0f, 1.0f, 1.0f)));
            Assert::IsTrue(ff_color_equal(ff_color_magenta(), ff_color_rgba(1.0f, 0.0f, 1.0f, 1.0f)));
        }

        TEST_METHOD(to_shader_ignores_remap_for_out_of_range_index)
        {
            // The bounds check guards a read of the 256-entry remap table, so an out-of-range
            // index must pass through unremapped rather than indexing past the end.
            uint8_t remap[256] = { 0 };

            ff_color_shader high = ff_color_to_shader(ff_color_palette(300, 1.0f), remap);
            Assert::AreEqual(300 / 256.0f, high.r);
            Assert::AreEqual(1.0f, high.a);

            ff_color_shader negative = ff_color_to_shader(ff_color_palette(-5, 1.0f), remap);
            Assert::AreEqual(-5 / 256.0f, negative.r);
            Assert::AreEqual(1.0f, negative.a);
        }

        TEST_METHOD(to_shader_palette_max_index)
        {
            ff_color_shader s = ff_color_to_shader(ff_color_palette(255, 1.0f), nullptr);
            Assert::AreEqual(255 / 256.0f, s.r);
            Assert::AreEqual(1.0f, s.a);
        }

        TEST_METHOD(to_shader_rgba_ignores_remap)
        {
            uint8_t remap[256] = { 0 };
            ff_color_shader s = ff_color_to_shader(ff_color_rgba(0.25f, 0.5f, 0.75f, 1.0f), remap);
            Assert::AreEqual(0.25f, s.r);
            Assert::AreEqual(0.5f, s.g);
            Assert::AreEqual(0.75f, s.b);
        }

        TEST_METHOD(to_shader_palette_leaves_green_and_blue_zero)
        {
            // The shader reads only r and a for a palette color; g and b must not carry stale
            // data from the union's rgba view.
            ff_color_shader s = ff_color_to_shader(ff_color_palette(128, 1.0f), nullptr);
            Assert::AreEqual(0.0f, s.g);
            Assert::AreEqual(0.0f, s.b);
        }
    };

    TEST_CLASS(matrix_tests)
    {
    public:
        TEST_METHOD(identity_matches_directxmath)
        {
            assert_matrix_equal(ff_matrix_identity(), DirectX::XMMatrixIdentity());
        }

        TEST_METHOD(translation_matches_directxmath)
        {
            assert_matrix_equal(ff_matrix_translation(3.0f, -4.0f, 5.0f), DirectX::XMMatrixTranslation(3.0f, -4.0f, 5.0f));
        }

        TEST_METHOD(scaling_matches_directxmath)
        {
            assert_matrix_equal(ff_matrix_scaling(2.0f, 3.0f, 4.0f), DirectX::XMMatrixScaling(2.0f, 3.0f, 4.0f));
        }

        TEST_METHOD(multiply_matches_directxmath)
        {
            ff_matrix a = ff_matrix_translation(3.0f, -4.0f, 5.0f);
            ff_matrix b = ff_matrix_scaling(2.0f, 3.0f, 4.0f);

            DirectX::XMMATRIX xa = DirectX::XMMatrixTranslation(3.0f, -4.0f, 5.0f);
            DirectX::XMMATRIX xb = DirectX::XMMatrixScaling(2.0f, 3.0f, 4.0f);

            assert_matrix_equal(ff_matrix_multiply(a, b), DirectX::XMMatrixMultiply(xa, xb));
        }

        TEST_METHOD(multiply_is_not_commutative)
        {
            ff_matrix a = ff_matrix_translation(3.0f, -4.0f, 5.0f);
            ff_matrix b = ff_matrix_scaling(2.0f, 3.0f, 4.0f);
            Assert::IsFalse(ff_matrix_equal(ff_matrix_multiply(a, b), ff_matrix_multiply(b, a)));
        }

        TEST_METHOD(identity_is_multiplicative_identity)
        {
            ff_matrix a = ff_matrix_translation(3.0f, -4.0f, 5.0f);
            Assert::IsTrue(ff_matrix_equal(ff_matrix_multiply(a, ff_matrix_identity()), a));
            Assert::IsTrue(ff_matrix_equal(ff_matrix_multiply(ff_matrix_identity(), a), a));
        }

        TEST_METHOD(transpose_matches_directxmath)
        {
            ff_matrix a = ff_matrix_multiply(ff_matrix_translation(3.0f, -4.0f, 5.0f), ff_matrix_scaling(2.0f, 3.0f, 4.0f));
            DirectX::XMMATRIX xa = DirectX::XMMatrixMultiply(DirectX::XMMatrixTranslation(3.0f, -4.0f, 5.0f), DirectX::XMMatrixScaling(2.0f, 3.0f, 4.0f));
            assert_matrix_equal(ff_matrix_transpose(a), DirectX::XMMatrixTranspose(xa));
        }

        TEST_METHOD(transpose_is_its_own_inverse)
        {
            ff_matrix a = ff_matrix_translation(3.0f, -4.0f, 5.0f);
            Assert::IsTrue(ff_matrix_equal(ff_matrix_transpose(ff_matrix_transpose(a)), a));
        }

        TEST_METHOD(transform_point)
        {
            ff_matrix m = ff_matrix_multiply(ff_matrix_scaling(2.0f, 3.0f, 1.0f), ff_matrix_translation(10.0f, 20.0f, 0.0f));
            ff_point_float p = ff_matrix_transform_point(m, ff_point_float_make(1.0f, 1.0f));
            Assert::AreEqual(12.0f, p.x);
            Assert::AreEqual(23.0f, p.y);
        }

        TEST_METHOD(equal)
        {
            Assert::IsTrue(ff_matrix_equal(ff_matrix_identity(), ff_matrix_identity()));
            Assert::IsFalse(ff_matrix_equal(ff_matrix_identity(), ff_matrix_scaling(2.0f, 1.0f, 1.0f)));
        }

        TEST_METHOD(multiply_is_associative)
        {
            ff_matrix a = ff_matrix_translation(3.0f, -4.0f, 5.0f);
            ff_matrix b = ff_matrix_scaling(2.0f, 3.0f, 4.0f);
            ff_matrix c = ff_matrix_translation(-1.0f, 2.0f, 0.0f);

            ff_matrix left = ff_matrix_multiply(ff_matrix_multiply(a, b), c);
            ff_matrix right = ff_matrix_multiply(a, ff_matrix_multiply(b, c));
            assert_matrix_equal(left, DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(
                DirectX::XMMatrixTranslation(3.0f, -4.0f, 5.0f),
                DirectX::XMMatrixScaling(2.0f, 3.0f, 4.0f)),
                DirectX::XMMatrixTranslation(-1.0f, 2.0f, 0.0f)));

            for (size_t i = 0; i < 16; i++)
            {
                Assert::AreEqual(left.m[i], right.m[i], 0.0001f);
            }
        }

        TEST_METHOD(transpose_of_identity_is_identity)
        {
            Assert::IsTrue(ff_matrix_equal(ff_matrix_transpose(ff_matrix_identity()), ff_matrix_identity()));
        }

        TEST_METHOD(translation_composes_additively)
        {
            ff_matrix a = ff_matrix_translation(3.0f, -4.0f, 5.0f);
            ff_matrix b = ff_matrix_translation(1.0f, 2.0f, -5.0f);
            Assert::IsTrue(ff_matrix_equal(ff_matrix_multiply(a, b), ff_matrix_translation(4.0f, -2.0f, 0.0f)));
        }

        TEST_METHOD(scaling_composes_multiplicatively)
        {
            ff_matrix a = ff_matrix_scaling(2.0f, 3.0f, 4.0f);
            ff_matrix b = ff_matrix_scaling(0.5f, 2.0f, 0.25f);
            Assert::IsTrue(ff_matrix_equal(ff_matrix_multiply(a, b), ff_matrix_scaling(1.0f, 6.0f, 1.0f)));
        }

        TEST_METHOD(transform_point_by_identity_is_unchanged)
        {
            ff_point_float p = ff_point_float_make(3.0f, -4.0f);
            Assert::IsTrue(ff_point_float_equal(ff_matrix_transform_point(ff_matrix_identity(), p), p));
        }

        TEST_METHOD(transform_point_matches_directxmath)
        {
            ff_matrix m = ff_matrix_multiply(ff_matrix_scaling(2.0f, 3.0f, 1.0f), ff_matrix_translation(10.0f, 20.0f, 0.0f));
            ff_point_float actual = ff_matrix_transform_point(m, ff_point_float_make(1.5f, -2.5f));

            DirectX::XMMATRIX xm = DirectX::XMMatrixMultiply(
                DirectX::XMMatrixScaling(2.0f, 3.0f, 1.0f),
                DirectX::XMMatrixTranslation(10.0f, 20.0f, 0.0f));
            DirectX::XMFLOAT2 expected;
            DirectX::XMStoreFloat2(&expected, DirectX::XMVector2TransformCoord(DirectX::XMVectorSet(1.5f, -2.5f, 0.0f, 1.0f), xm));

            Assert::AreEqual(expected.x, actual.x, 0.0001f);
            Assert::AreEqual(expected.y, actual.y, 0.0001f);
        }

        TEST_METHOD(equal_is_exact_not_approximate)
        {
            ff_matrix a = ff_matrix_identity();
            ff_matrix b = ff_matrix_identity();
            b.m[7] = 0.0000001f;
            Assert::IsFalse(ff_matrix_equal(a, b));
        }

        TEST_METHOD(equal_detects_a_difference_in_every_element)
        {
            // A loop that stopped early or compared the wrong index would pass a spot check, so
            // every one of the 16 elements is perturbed in turn.
            for (size_t i = 0; i < 16; i++)
            {
                ff_matrix a = ff_matrix_identity();
                ff_matrix b = ff_matrix_identity();
                b.m[i] += 1.0f;
                Assert::IsFalse(ff_matrix_equal(a, b));
            }
        }
    };

    TEST_CLASS(view_matrix_tests)
    {
    public:
        TEST_METHOD(target_size_defaults)
        {
            ff_dx12_target_size size = ff_dx12_target_size_make(800, 600);
            Assert::AreEqual((size_t)800, size.pixel_size.x);
            Assert::AreEqual((size_t)600, size.pixel_size.y);
            Assert::IsTrue(size.rotation == ff_dx12_rotation_none);
            Assert::AreEqual(1.0, size.dpi_scale);
        }

        TEST_METHOD(maps_world_corners_to_clip_space)
        {
            ff_dx12_target_size size = ff_dx12_target_size_make(800, 600);
            ff_rect_float view = ff_rect_float_make(0.0f, 0.0f, 800.0f, 600.0f);
            ff_rect_float world = ff_rect_float_make(0.0f, 0.0f, 100.0f, 50.0f);

            ff_matrix m = {};
            Assert::IsTrue(ff_dx12_view_matrix(size, view, world, false, &m));

            // Top-left of the world maps to the top-left of clip space, bottom-right to the
            // opposite corner. Y is flipped because clip space is y-up and the world is y-down.
            ff_point_float top_left = ff_matrix_transform_point(m, ff_point_float_make(0.0f, 0.0f));
            Assert::AreEqual(-1.0f, top_left.x, 0.0001f);
            Assert::AreEqual(1.0f, top_left.y, 0.0001f);

            ff_point_float bottom_right = ff_matrix_transform_point(m, ff_point_float_make(100.0f, 50.0f));
            Assert::AreEqual(1.0f, bottom_right.x, 0.0001f);
            Assert::AreEqual(-1.0f, bottom_right.y, 0.0001f);

            ff_point_float center = ff_matrix_transform_point(m, ff_point_float_make(50.0f, 25.0f));
            Assert::AreEqual(0.0f, center.x, 0.0001f);
            Assert::AreEqual(0.0f, center.y, 0.0001f);
        }

        TEST_METHOD(offset_world_rect)
        {
            ff_dx12_target_size size = ff_dx12_target_size_make(800, 600);
            ff_rect_float view = ff_rect_float_make(0.0f, 0.0f, 800.0f, 600.0f);
            ff_rect_float world = ff_rect_float_make(10.0f, 20.0f, 110.0f, 70.0f);

            ff_matrix m = {};
            Assert::IsTrue(ff_dx12_view_matrix(size, view, world, false, &m));

            ff_point_float top_left = ff_matrix_transform_point(m, ff_point_float_make(10.0f, 20.0f));
            Assert::AreEqual(-1.0f, top_left.x, 0.0001f);
            Assert::AreEqual(1.0f, top_left.y, 0.0001f);
        }

        TEST_METHOD(matches_legacy_directxmath_composition)
        {
            ff_dx12_target_size size = ff_dx12_target_size_make(800, 600);
            ff_rect_float view = ff_rect_float_make(0.0f, 0.0f, 800.0f, 600.0f);
            ff_rect_float world = ff_rect_float_make(10.0f, 20.0f, 110.0f, 70.0f);

            ff_matrix m = {};
            Assert::IsTrue(ff_dx12_view_matrix(size, view, world, false, &m));

            DirectX::XMFLOAT4X4A rotate_0(
                2, 0, 0, 0,
                0, -2, 0, 0,
                0, 0, 1, 0,
                -1, 1, 0, 1);

            DirectX::XMMATRIX expected =
                DirectX::XMMatrixTranslation(-10.0f, -20.0f, 0.0f) *
                DirectX::XMMatrixScaling(1 / 100.0f, 1 / 50.0f, 1) *
                DirectX::XMLoadFloat4x4A(&rotate_0);

            assert_matrix_equal(m, expected);
        }

        TEST_METHOD(rejects_empty_world_rect)
        {
            ff_dx12_target_size size = ff_dx12_target_size_make(800, 600);
            ff_rect_float view = ff_rect_float_make(0.0f, 0.0f, 800.0f, 600.0f);

            ff_matrix m = {};
            Assert::IsFalse(ff_dx12_view_matrix(size, view, ff_rect_float_make(0.0f, 0.0f, 0.0f, 50.0f), false, &m));
            Assert::IsTrue(ff_matrix_equal(m, ff_matrix_identity()));

            Assert::IsFalse(ff_dx12_view_matrix(size, view, ff_rect_float_make(0.0f, 0.0f, 100.0f, 0.0f), false, &m));
        }

        TEST_METHOD(rejects_empty_view_rect)
        {
            ff_dx12_target_size size = ff_dx12_target_size_make(800, 600);
            ff_rect_float world = ff_rect_float_make(0.0f, 0.0f, 100.0f, 50.0f);

            ff_matrix m = {};
            Assert::IsFalse(ff_dx12_view_matrix(size, ff_rect_float_zero(), world, false, &m));
            Assert::IsTrue(ff_matrix_equal(m, ff_matrix_identity()));
        }

        TEST_METHOD(ignore_rotation_matches_unrotated)
        {
            ff_rect_float view = ff_rect_float_make(0.0f, 0.0f, 800.0f, 600.0f);
            ff_rect_float world = ff_rect_float_make(0.0f, 0.0f, 100.0f, 50.0f);

            ff_dx12_target_size none = ff_dx12_target_size_make(800, 600);
            ff_matrix unrotated = {};
            Assert::IsTrue(ff_dx12_view_matrix(none, view, world, false, &unrotated));

            // Every rotation collapses to the unrotated matrix when rotation is ignored, which is
            // what keeps the current no-rotation behavior reachable from any orientation.
            for (int i = 0; i < ff_dx12_rotation_count; i++)
            {
                ff_dx12_target_size size = ff_dx12_target_size_make(800, 600);
                size.rotation = (ff_dx12_rotation)i;

                ff_matrix m = {};
                Assert::IsTrue(ff_dx12_view_matrix(size, view, world, true, &m));
                Assert::IsTrue(ff_matrix_equal(m, unrotated));
            }
        }

        TEST_METHOD(rotations_differ_from_each_other)
        {
            ff_rect_float view = ff_rect_float_make(0.0f, 0.0f, 800.0f, 600.0f);
            ff_rect_float world = ff_rect_float_make(0.0f, 0.0f, 100.0f, 50.0f);

            ff_matrix matrices[ff_dx12_rotation_count] = {};

            for (int i = 0; i < ff_dx12_rotation_count; i++)
            {
                ff_dx12_target_size size = ff_dx12_target_size_make(800, 600);
                size.rotation = (ff_dx12_rotation)i;
                Assert::IsTrue(ff_dx12_view_matrix(size, view, world, false, &matrices[i]));
            }

            for (int i = 0; i < ff_dx12_rotation_count; i++)
            {
                for (int j = i + 1; j < ff_dx12_rotation_count; j++)
                {
                    Assert::IsFalse(ff_matrix_equal(matrices[i], matrices[j]));
                }
            }
        }

        TEST_METHOD(rotation_180_negates_unrotated)
        {
            ff_rect_float view = ff_rect_float_make(0.0f, 0.0f, 800.0f, 600.0f);
            ff_rect_float world = ff_rect_float_make(0.0f, 0.0f, 100.0f, 50.0f);

            ff_dx12_target_size size = ff_dx12_target_size_make(800, 600);
            size.rotation = ff_dx12_rotation_180;

            ff_matrix m = {};
            Assert::IsTrue(ff_dx12_view_matrix(size, view, world, false, &m));

            ff_point_float top_left = ff_matrix_transform_point(m, ff_point_float_make(0.0f, 0.0f));
            Assert::AreEqual(1.0f, top_left.x, 0.0001f);
            Assert::AreEqual(-1.0f, top_left.y, 0.0001f);
        }

        TEST_METHOD(null_output_asserts_and_returns_false)
        {
            scoped_assert_counter counter;

            ff_dx12_target_size size = ff_dx12_target_size_make(800, 600);
            ff_rect_float view = ff_rect_float_make(0.0f, 0.0f, 800.0f, 600.0f);
            ff_rect_float world = ff_rect_float_make(0.0f, 0.0f, 100.0f, 50.0f);

            Assert::IsFalse(ff_dx12_view_matrix(size, view, world, false, nullptr));
            Assert::AreEqual(scoped_assert_counter::expected_count(1), scoped_assert_counter::count);
        }

        TEST_METHOD(out_of_range_rotation_asserts_and_returns_false)
        {
            scoped_assert_counter counter;

            ff_dx12_target_size size = ff_dx12_target_size_make(800, 600);
            size.rotation = (ff_dx12_rotation)ff_dx12_rotation_count;

            ff_rect_float view = ff_rect_float_make(0.0f, 0.0f, 800.0f, 600.0f);
            ff_rect_float world = ff_rect_float_make(0.0f, 0.0f, 100.0f, 50.0f);

            ff_matrix m = {};
            Assert::IsFalse(ff_dx12_view_matrix(size, view, world, false, &m));
            Assert::AreEqual(scoped_assert_counter::expected_count(1), scoped_assert_counter::count);
            Assert::IsTrue(ff_matrix_equal(m, ff_matrix_identity()));
        }

        TEST_METHOD(inverted_world_rect_still_produces_a_matrix)
        {
            // A flipped world rect has negative width but non-zero area, so it is legal and
            // mirrors the output rather than being rejected.
            ff_dx12_target_size size = ff_dx12_target_size_make(800, 600);
            ff_rect_float view = ff_rect_float_make(0.0f, 0.0f, 800.0f, 600.0f);
            ff_rect_float world = ff_rect_float_make(100.0f, 0.0f, 0.0f, 50.0f);

            ff_matrix m = {};
            Assert::IsTrue(ff_dx12_view_matrix(size, view, world, false, &m));

            ff_point_float right_edge = ff_matrix_transform_point(m, ff_point_float_make(0.0f, 0.0f));
            Assert::AreEqual(1.0f, right_edge.x, 0.0001f);
        }

        TEST_METHOD(view_rect_size_does_not_change_the_matrix)
        {
            // view_rect is only validated for non-zero area today; the projection is defined
            // entirely by the world rect and the rotation.
            ff_dx12_target_size size = ff_dx12_target_size_make(800, 600);
            ff_rect_float world = ff_rect_float_make(0.0f, 0.0f, 100.0f, 50.0f);

            ff_matrix a = {};
            ff_matrix b = {};
            Assert::IsTrue(ff_dx12_view_matrix(size, ff_rect_float_make(0.0f, 0.0f, 800.0f, 600.0f), world, false, &a));
            Assert::IsTrue(ff_dx12_view_matrix(size, ff_rect_float_make(0.0f, 0.0f, 10.0f, 10.0f), world, false, &b));
            Assert::IsTrue(ff_matrix_equal(a, b));
        }

        TEST_METHOD(negative_view_rect_area_is_rejected)
        {
            ff_dx12_target_size size = ff_dx12_target_size_make(800, 600);
            ff_rect_float world = ff_rect_float_make(0.0f, 0.0f, 100.0f, 50.0f);
            ff_rect_float view = ff_rect_float_make(0.0f, 0.0f, -800.0f, 600.0f);

            ff_matrix m = {};
            Assert::IsFalse(ff_dx12_view_matrix(size, view, world, false, &m));
            Assert::IsTrue(ff_matrix_equal(m, ff_matrix_identity()));
        }
    };
}
