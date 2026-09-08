#include "pch.h"

// Build a GUID with known, non-trivial byte content for equality checks.
static GUID make_test_guid()
{
    GUID g;
    g.Data1 = 0x11223344;
    g.Data2 = 0x5566;
    g.Data3 = 0x7788;
    g.Data4[0] = 0x99;
    g.Data4[1] = 0xAA;
    g.Data4[2] = 0xBB;
    g.Data4[3] = 0xCC;
    g.Data4[4] = 0xDD;
    g.Data4[5] = 0xEE;
    g.Data4[6] = 0xFF;
    g.Data4[7] = 0x01;
    return g;
}

static bool guid_equals(const GUID& a, const GUID& b)
{
    return ::memcmp(&a, &b, sizeof(GUID)) == 0;
}

static bool bytes_equal(const void* a, const void* b, size_t size)
{
    return ::memcmp(a, b, size) == 0;
}

// Bit-exact float helpers so NaN/infinity/negative zero can be checked without float comparison.
static float float32_from_bits(uint32_t bits)
{
    float value;
    ::memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t bits_from_float32(float value)
{
    uint32_t bits;
    ::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static double float64_from_bits(uint64_t bits)
{
    double value;
    ::memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint64_t bits_from_float64(double value)
{
    uint64_t bits;
    ::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

namespace ff::test::base
{
    TEST_CLASS(value_tests)
    {
    public:
        // ====================================================================
        // Empty / null
        // ====================================================================
        TEST_METHOD(empty_has_empty_type)
        {
            ff_value v = ff_value_new_empty();
            Assert::IsTrue(v.type == ff_value_type_empty);
        }

        TEST_METHOD(null_has_null_type)
        {
            ff_value v = ff_value_new_null();
            Assert::IsTrue(v.type == ff_value_type_null);
        }

        TEST_METHOD(empty_and_null_are_distinct)
        {
            Assert::IsTrue(ff_value_new_empty().type != ff_value_new_null().type);
        }

        TEST_METHOD(zero_initialized_value_is_empty)
        {
            // ff_value_type_empty == 0, so a zeroed value must read as empty.
            ff_value v{};
            Assert::IsTrue(v.type == ff_value_type_empty);
        }

        TEST_METHOD(empty_and_null_have_zero_payload)
        {
            // Neither constructor writes a payload, so the union must stay fully zeroed.
            ff_value e = ff_value_new_empty();
            ff_value n = ff_value_new_null();

            Assert::AreEqual<int64_t>(0, e.i64);
            Assert::IsNull(e.data.data);
            Assert::AreEqual<int64_t>(0, n.i64);
            Assert::IsNull(n.data.data);
        }

        // ====================================================================
        // Boolean
        // ====================================================================
        TEST_METHOD(boolean_true)
        {
            ff_value v = ff_value_new_boolean(true);
            Assert::IsTrue(v.type == ff_value_type_boolean);
            Assert::IsTrue(v.b);
        }

        TEST_METHOD(boolean_false)
        {
            ff_value v = ff_value_new_boolean(false);
            Assert::IsTrue(v.type == ff_value_type_boolean);
            Assert::IsFalse(v.b);
        }

        // ====================================================================
        // GUID
        // ====================================================================
        TEST_METHOD(guid_stores_value)
        {
            GUID g = make_test_guid();
            ff_value v = ff_value_new_guid(g);

            Assert::IsTrue(v.type == ff_value_type_guid);
            Assert::IsTrue(guid_equals(v.guid, g));
        }

        TEST_METHOD(guid_zero_stores_value)
        {
            GUID g{};
            ff_value v = ff_value_new_guid(g);

            Assert::IsTrue(v.type == ff_value_type_guid);
            Assert::IsTrue(guid_equals(v.guid, g));
        }

        TEST_METHOD(guid_all_bits_set_does_not_clobber_type)
        {
            // A GUID fills all 16 payload bytes, which is the worst case for the type field.
            GUID g;
            ::memset(&g, 0xFF, sizeof(g));

            ff_value v = ff_value_new_guid(g);

            Assert::IsTrue(v.type == ff_value_type_guid);
            Assert::IsTrue(guid_equals(v.guid, g));
        }

        // ====================================================================
        // Integers
        // ====================================================================
        TEST_METHOD(int32_stores_value)
        {
            ff_value v = ff_value_new_int32(-12345);
            Assert::IsTrue(v.type == ff_value_type_int32);
            Assert::AreEqual<int32_t>(-12345, v.i32);
        }

        TEST_METHOD(int32_stores_limits)
        {
            Assert::AreEqual<int32_t>(INT32_MIN, ff_value_new_int32(INT32_MIN).i32);
            Assert::AreEqual<int32_t>(INT32_MAX, ff_value_new_int32(INT32_MAX).i32);
        }

        TEST_METHOD(int64_stores_value)
        {
            ff_value v = ff_value_new_int64(-9000000000LL);
            Assert::IsTrue(v.type == ff_value_type_int64);
            Assert::AreEqual<int64_t>(-9000000000LL, v.i64);
        }

        TEST_METHOD(int64_stores_limits)
        {
            Assert::AreEqual<int64_t>(INT64_MIN, ff_value_new_int64(INT64_MIN).i64);
            Assert::AreEqual<int64_t>(INT64_MAX, ff_value_new_int64(INT64_MAX).i64);
        }

        // ====================================================================
        // Floating point
        // ====================================================================
        TEST_METHOD(float32_stores_value)
        {
            ff_value v = ff_value_new_float32(-2.25f);
            Assert::IsTrue(v.type == ff_value_type_float32);
            Assert::IsTrue(v.f32 == -2.25f);
        }

        TEST_METHOD(float64_stores_value)
        {
            ff_value v = ff_value_new_float64(1234.5);
            Assert::IsTrue(v.type == ff_value_type_float64);
            Assert::IsTrue(v.f64 == 1234.5);
        }

        TEST_METHOD(float32_preserves_negative_zero)
        {
            ff_value v = ff_value_new_float32(-0.0f);
            Assert::IsTrue(v.f32 == 0.0f);
            Assert::AreEqual<uint32_t>(0x80000000u, bits_from_float32(v.f32));
        }

        TEST_METHOD(float64_preserves_negative_zero)
        {
            ff_value v = ff_value_new_float64(-0.0);
            Assert::IsTrue(v.f64 == 0.0);
            Assert::AreEqual<uint64_t>(0x8000000000000000ull, bits_from_float64(v.f64));
        }

        TEST_METHOD(float32_preserves_nan_and_infinity)
        {
            constexpr uint32_t quiet_nan = 0x7FC00000u;
            constexpr uint32_t positive_inf = 0x7F800000u;
            constexpr uint32_t negative_inf = 0xFF800000u;

            Assert::AreEqual<uint32_t>(quiet_nan, bits_from_float32(ff_value_new_float32(float32_from_bits(quiet_nan)).f32));
            Assert::AreEqual<uint32_t>(positive_inf, bits_from_float32(ff_value_new_float32(float32_from_bits(positive_inf)).f32));
            Assert::AreEqual<uint32_t>(negative_inf, bits_from_float32(ff_value_new_float32(float32_from_bits(negative_inf)).f32));
        }

        TEST_METHOD(float64_preserves_nan_and_infinity)
        {
            constexpr uint64_t quiet_nan = 0x7FF8000000000000ull;
            constexpr uint64_t positive_inf = 0x7FF0000000000000ull;
            constexpr uint64_t negative_inf = 0xFFF0000000000000ull;

            Assert::AreEqual<uint64_t>(quiet_nan, bits_from_float64(ff_value_new_float64(float64_from_bits(quiet_nan)).f64));
            Assert::AreEqual<uint64_t>(positive_inf, bits_from_float64(ff_value_new_float64(float64_from_bits(positive_inf)).f64));
            Assert::AreEqual<uint64_t>(negative_inf, bits_from_float64(ff_value_new_float64(float64_from_bits(negative_inf)).f64));
        }

        TEST_METHOD(float32_preserves_denormal)
        {
            constexpr uint32_t smallest_denormal = 0x00000001u;
            Assert::AreEqual<uint32_t>(smallest_denormal, bits_from_float32(ff_value_new_float32(float32_from_bits(smallest_denormal)).f32));
        }

        TEST_METHOD(float64_keeps_precision_beyond_float32)
        {
            // A value that would lose bits if it were routed through a float.
            constexpr double precise = 1.0 + 1.0 / 4503599627370496.0; // 1 + 2^-52
            ff_value v = ff_value_new_float64(precise);

            Assert::IsTrue(v.f64 == precise);
            Assert::IsTrue(v.f64 != 1.0);
        }

        // ====================================================================
        // Points
        // ====================================================================
        TEST_METHOD(point_int32_stores_components)
        {
            ff_value v = ff_value_new_point_int32(-7, 11);
            Assert::IsTrue(v.type == ff_value_type_point_int32);
            Assert::AreEqual<int32_t>(-7, v.point_i32[0]);
            Assert::AreEqual<int32_t>(11, v.point_i32[1]);
        }

        TEST_METHOD(point_int64_stores_components)
        {
            ff_value v = ff_value_new_point_int64(-7000000000LL, 8000000000LL);
            Assert::IsTrue(v.type == ff_value_type_point_int64);
            Assert::AreEqual<int64_t>(-7000000000LL, v.point_i64[0]);
            Assert::AreEqual<int64_t>(8000000000LL, v.point_i64[1]);
        }

        TEST_METHOD(point_float32_stores_components)
        {
            ff_value v = ff_value_new_point_float32(1.5f, -3.25f);
            Assert::IsTrue(v.type == ff_value_type_point_float32);
            Assert::IsTrue(v.point_f32[0] == 1.5f);
            Assert::IsTrue(v.point_f32[1] == -3.25f);
        }

        TEST_METHOD(point_float64_stores_components)
        {
            ff_value v = ff_value_new_point_float64(1.5, -3.25);
            Assert::IsTrue(v.type == ff_value_type_point_float64);
            Assert::IsTrue(v.point_f64[0] == 1.5);
            Assert::IsTrue(v.point_f64[1] == -3.25);
        }

        // ====================================================================
        // Rects
        // ====================================================================
        TEST_METHOD(rect_int32_stores_components)
        {
            ff_value v = ff_value_new_rect_int32(1, 2, 3, 4);
            Assert::IsTrue(v.type == ff_value_type_rect_int32);
            Assert::AreEqual<int32_t>(1, v.rect_i32[0]);
            Assert::AreEqual<int32_t>(2, v.rect_i32[1]);
            Assert::AreEqual<int32_t>(3, v.rect_i32[2]);
            Assert::AreEqual<int32_t>(4, v.rect_i32[3]);
        }

        TEST_METHOD(rect_float32_stores_components)
        {
            ff_value v = ff_value_new_rect_float32(1.5f, 2.5f, 3.5f, 4.5f);
            Assert::IsTrue(v.type == ff_value_type_rect_float32);
            Assert::IsTrue(v.rect_f32[0] == 1.5f);
            Assert::IsTrue(v.rect_f32[1] == 2.5f);
            Assert::IsTrue(v.rect_f32[2] == 3.5f);
            Assert::IsTrue(v.rect_f32[3] == 4.5f);
        }

        TEST_METHOD(rect_int32_stores_limits_and_keeps_type)
        {
            // A rect fills all 16 payload bytes, so verify the type field survives.
            ff_value v = ff_value_new_rect_int32(INT32_MIN, INT32_MAX, -1, 0);

            Assert::IsTrue(v.type == ff_value_type_rect_int32);
            Assert::AreEqual<int32_t>(INT32_MIN, v.rect_i32[0]);
            Assert::AreEqual<int32_t>(INT32_MAX, v.rect_i32[1]);
            Assert::AreEqual<int32_t>(-1, v.rect_i32[2]);
            Assert::AreEqual<int32_t>(0, v.rect_i32[3]);
        }

        TEST_METHOD(rect_and_point_of_same_type_are_distinct)
        {
            ff_value point = ff_value_new_point_int32(1, 2);
            ff_value rect = ff_value_new_rect_int32(1, 2, 3, 4);

            Assert::IsTrue(point.type != rect.type);
            // The first two components share storage, so only the type distinguishes them.
            Assert::AreEqual<int32_t>(point.point_i32[0], rect.rect_i32[0]);
            Assert::AreEqual<int32_t>(point.point_i32[1], rect.rect_i32[1]);
        }

        // ====================================================================
        // Data (ff_span overload)
        // ====================================================================
        TEST_METHOD(data_from_span_sets_metadata)
        {
            uint8_t bytes[4] = { 10, 20, 30, 40 };
            struct ff_span span{ bytes, 4 };

            ff_value v = ff_value_new_data(span);

            Assert::IsTrue(v.type == ff_value_type_data);
            Assert::AreEqual<size_t>(4, (size_t)v.data.count);
            Assert::AreEqual<size_t>(1, (size_t)v.data.item_size);
            Assert::AreEqual<size_t>(alignof(size_t), (size_t)v.data.item_align);
        }

        TEST_METHOD(data_from_span_no_arena_shares_pointer)
        {
            uint8_t bytes[4] = { 10, 20, 30, 40 };
            struct ff_span span{ bytes, 4 };

            ff_value v = ff_value_new_data(span);

            // Without a copy arena the value references the caller's buffer directly.
            Assert::IsTrue(v.data.data == bytes);
        }

        TEST_METHOD(data_round_trips_through_as_data)
        {
            uint8_t bytes[5] = { 0, 0xFF, 0x7F, 0x80, 1 };
            struct ff_span span{ bytes, sizeof(bytes) };

            ff_value v = ff_value_new_data(span);
            ff_span out = ff_value_as_data(&v);

            Assert::IsTrue(out.data == bytes);
            Assert::AreEqual<size_t>(sizeof(bytes), out.size);
            Assert::IsTrue(bytes_equal(out.data, bytes, sizeof(bytes)));
        }

        TEST_METHOD(data_empty_span)
        {
            struct ff_span span{ nullptr, 0 };

            ff_value v = ff_value_new_data(span);
            ff_span out = ff_value_as_data(&v);

            Assert::IsTrue(v.type == ff_value_type_data);
            Assert::IsNull(out.data);
            Assert::AreEqual<size_t>(0, out.size);
            Assert::AreEqual<size_t>(1, (size_t)v.data.item_size);
        }

        TEST_METHOD(data_non_null_pointer_with_zero_size)
        {
            uint8_t bytes[1] = { 7 };
            struct ff_span span{ bytes, 0 };

            ff_value v = ff_value_new_data(span);
            ff_span out = ff_value_as_data(&v);

            Assert::IsTrue(out.data == bytes);
            Assert::AreEqual<size_t>(0, out.size);
        }

        // ====================================================================
        // Data (ff_array_span overload)
        // ====================================================================
        TEST_METHOD(data_from_array_span_preserves_metadata)
        {
            uint32_t nums[3] = { 0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu };

            struct ff_array_span as{};
            as.data = nums;
            as.count = 3;
            as.item_size = sizeof(uint32_t);
            as.item_align = alignof(uint32_t);

            ff_value v = ff_value_new_data_array(as);

            Assert::IsTrue(v.type == ff_value_type_data);
            Assert::IsTrue(v.data.data == nums);
            Assert::AreEqual<size_t>(3, (size_t)v.data.count);
            Assert::AreEqual<size_t>(sizeof(uint32_t), (size_t)v.data.item_size);
            Assert::AreEqual<size_t>(alignof(uint32_t), (size_t)v.data.item_align);
        }

        TEST_METHOD(data_size_uses_full_size_t_product)
        {
            struct ff_array_span as{};
            as.data = nullptr;
            as.count = 200000000;
            as.item_size = 24;
            as.item_align = 8;

            ff_value v = ff_value_new_data_array(as);
            Assert::AreEqual((size_t)200000000 * 24, ff_value_as_data(&v).size);
        }

        TEST_METHOD(data_array_holds_maximum_bitfield_values)
        {
            // ff_array_span packs count into 32 bits and item_size/item_align into 16 bits each.
            struct ff_array_span as{};
            as.data = nullptr;
            as.count = 0xFFFFFFFFu;
            as.item_size = 0xFFFFu;
            as.item_align = 0xFFFFu;

            ff_value v = ff_value_new_data_array(as);

            Assert::AreEqual<size_t>(0xFFFFFFFFu, (size_t)v.data.count);
            Assert::AreEqual<size_t>(0xFFFFu, (size_t)v.data.item_size);
            Assert::AreEqual<size_t>(0xFFFFu, (size_t)v.data.item_align);
            Assert::AreEqual<size_t>((size_t)0xFFFFFFFFu * 0xFFFFu, ff_value_as_data(&v).size);
        }

        TEST_METHOD(data_array_empty_keeps_item_metadata)
        {
            struct ff_array_span as{};
            as.data = nullptr;
            as.count = 0;
            as.item_size = sizeof(double);
            as.item_align = alignof(double);

            ff_value v = ff_value_new_data_array(as);

            Assert::AreEqual<size_t>(0, (size_t)v.data.count);
            Assert::AreEqual<size_t>(sizeof(double), (size_t)v.data.item_size);
            Assert::AreEqual<size_t>(0, ff_value_as_data(&v).size);
        }

        TEST_METHOD(data_array_contents_readable)
        {
            uint32_t nums[3] = { 0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu };

            struct ff_array_span as{};
            as.data = nums;
            as.count = 3;
            as.item_size = sizeof(uint32_t);
            as.item_align = alignof(uint32_t);

            ff_value v = ff_value_new_data_array(as);
            ff_span out = ff_value_as_data(&v);

            Assert::AreEqual<size_t>(sizeof(nums), out.size);
            Assert::IsTrue(bytes_equal(out.data, nums, sizeof(nums)));
        }

        // ====================================================================
        // Dict
        // ====================================================================
        TEST_METHOD(dict_stores_pointer)
        {
            // as_dict() only returns the stored pointer, so an opaque address is enough.
            int sentinel = 0;
            ff_dict* dict_ptr = reinterpret_cast<ff_dict*>(&sentinel);

            ff_value v = ff_value_new_dict(dict_ptr);

            Assert::IsTrue(v.type == ff_value_type_dict);
            Assert::IsTrue(ff_value_as_dict(&v) == dict_ptr);
        }

        TEST_METHOD(dict_null_pointer)
        {
            ff_value v = ff_value_new_dict(nullptr);
            Assert::IsTrue(v.type == ff_value_type_dict);
            Assert::IsNull(ff_value_as_dict(&v));
        }

        TEST_METHOD(dict_clears_span_metadata)
        {
            int sentinel = 0;
            ff_value v = ff_value_new_dict(reinterpret_cast<ff_dict*>(&sentinel));

            // A dict only stores a pointer, so the array metadata must stay zeroed.
            Assert::AreEqual<size_t>(0, (size_t)v.data.count);
            Assert::AreEqual<size_t>(0, (size_t)v.data.item_size);
            Assert::AreEqual<size_t>(0, (size_t)v.data.item_align);
        }

        // ====================================================================
        // String
        // ====================================================================
        TEST_METHOD(string_no_arena_shares_pointer)
        {
            ff_string_view src{ "hello", 5 };

            ff_value v = ff_value_new_string(src);
            ff_string_view out = ff_value_as_string(&v);

            Assert::IsTrue(v.type == ff_value_type_string);
            Assert::IsTrue(out.data == src.data);
            Assert::AreEqual<size_t>(5, out.count);
        }

        TEST_METHOD(string_empty)
        {
            ff_string_view src{ "", 0 };

            ff_value v = ff_value_new_string(src);
            ff_string_view out = ff_value_as_string(&v);

            Assert::IsTrue(v.type == ff_value_type_string);
            Assert::AreEqual<size_t>(0, out.count);
        }

        TEST_METHOD(string_uses_byte_sized_items)
        {
            ff_string_view src{ "hello", 5 };
            ff_value v = ff_value_new_string(src);

            Assert::AreEqual<size_t>(1, (size_t)v.data.item_size);
            Assert::AreEqual<size_t>(5, (size_t)v.data.count);
            Assert::AreEqual<size_t>(5, ff_value_as_data(&v).size);
        }

        TEST_METHOD(string_inherits_data_alignment)
        {
            ff_string_view src{ "hello", 5 };
            ff_value v = ff_value_new_string(src);

            // A string is built on top of a data value, so it keeps that value's alignment.
            Assert::AreEqual<size_t>(alignof(size_t), (size_t)v.data.item_align);
        }

        TEST_METHOD(string_keeps_embedded_null)
        {
            // Strings are counted, not null terminated, so an interior null is just a byte.
            const char text[] = "a\0b";
            ff_string_view src{ text, 3 };

            ff_value v = ff_value_new_string(src);
            ff_string_view out = ff_value_as_string(&v);

            Assert::AreEqual<size_t>(3, out.count);
            Assert::IsTrue(bytes_equal(out.data, text, 3));
        }

        TEST_METHOD(string_keeps_high_utf8_bytes)
        {
            const char text[] = "\xE2\x9C\x93"; // U+2713 check mark
            ff_string_view src{ text, 3 };

            ff_value v = ff_value_new_string(src);
            ff_string_view out = ff_value_as_string(&v);

            Assert::AreEqual<size_t>(3, out.count);
            Assert::IsTrue(bytes_equal(out.data, text, 3));
        }

        TEST_METHOD(string_null_data_with_zero_count)
        {
            ff_value v = ff_value_new_string(ff_string_view_empty());
            ff_string_view out = ff_value_as_string(&v);

            Assert::IsTrue(v.type == ff_value_type_string);
            Assert::AreEqual<size_t>(0, out.count);
        }

        // ====================================================================
        // Array
        // ====================================================================
        TEST_METHOD(array_no_arena_shares_pointer_and_size)
        {
            ff_value items[3] =
            {
                ff_value_new_int32(10),
                ff_value_new_int32(20),
                ff_value_new_int32(30),
            };

            ff_value v = ff_value_new_array(ff_value_span{ items, 3 });

            Assert::IsTrue(v.type == ff_value_type_array);
            Assert::IsTrue(ff_value_as_array(&v).data == items);
            Assert::AreEqual<size_t>(3, ff_value_as_array(&v).count);
        }

        TEST_METHOD(array_elements_readable)
        {
            ff_value items[3] =
            {
                ff_value_new_int32(10),
                ff_value_new_int32(20),
                ff_value_new_int32(30),
            };

            ff_value v = ff_value_new_array(ff_value_span{ items, 3 });
            const ff_value* arr = ff_value_as_array(&v).data;

            for (int32_t i = 0; i < 3; i++)
            {
                Assert::IsTrue(arr[i].type == ff_value_type_int32);
                Assert::AreEqual<int32_t>((i + 1) * 10, arr[i].i32);
            }
        }

        TEST_METHOD(array_empty)
        {
            ff_value items[1] = { ff_value_new_int32(0) };
            ff_value v = ff_value_new_array(ff_value_span{ items, 0 });

            Assert::IsTrue(v.type == ff_value_type_array);
            Assert::AreEqual<size_t>(0, ff_value_as_array(&v).count);
        }

        TEST_METHOD(array_of_mixed_types_round_trip)
        {
            ff_value items[3] =
            {
                ff_value_new_boolean(true),
                ff_value_new_float64(2.5),
                ff_value_new_int64(42),
            };

            ff_value v = ff_value_new_array(ff_value_span{ items, 3 });
            const ff_value* arr = ff_value_as_array(&v).data;

            Assert::IsTrue(arr[0].type == ff_value_type_boolean);
            Assert::IsTrue(arr[0].b);
            Assert::IsTrue(arr[1].type == ff_value_type_float64);
            Assert::IsTrue(arr[1].f64 == 2.5);
            Assert::IsTrue(arr[2].type == ff_value_type_int64);
            Assert::AreEqual<int64_t>(42, arr[2].i64);
        }

        TEST_METHOD(array_item_metadata_matches_value_layout)
        {
            // as_array() asserts on this metadata, so it must be exact.
            ff_value items[2] = { ff_value_new_null(), ff_value_new_null() };
            ff_value v = ff_value_new_array(ff_value_span{ items, 2 });

            Assert::AreEqual<size_t>(sizeof(ff_value), (size_t)v.data.item_size);
            Assert::AreEqual<size_t>(alignof(ff_value), (size_t)v.data.item_align);
            Assert::AreEqual<size_t>(2 * sizeof(ff_value), ff_value_as_data(&v).size);
        }

        TEST_METHOD(array_can_nest_arrays)
        {
            ff_value inner_items[2] = { ff_value_new_int32(1), ff_value_new_int32(2) };
            ff_string_view nested{ "nested", 6 };
            ff_value outer_items[2] =
            {
                ff_value_new_array(ff_value_span{ inner_items, 2 }),
                ff_value_new_string(nested),
            };

            ff_value v = ff_value_new_array(ff_value_span{ outer_items, 2 });
            ff_value_span outer = ff_value_as_array(&v);

            Assert::IsTrue(outer.data[0].type == ff_value_type_array);
            ff_value_span inner = ff_value_as_array(&outer.data[0]);
            Assert::AreEqual<size_t>(2, inner.count);
            Assert::AreEqual<int32_t>(2, inner.data[1].i32);

            Assert::IsTrue(outer.data[1].type == ff_value_type_string);
            Assert::AreEqual<size_t>(6, ff_value_as_string(&outer.data[1]).count);
        }

        TEST_METHOD(array_of_empty_values_keeps_element_types)
        {
            ff_value items[2] = { ff_value_new_empty(), ff_value_new_null() };
            ff_value v = ff_value_new_array(ff_value_span{ items, 2 });
            ff_value_span out = ff_value_as_array(&v);

            Assert::IsTrue(out.data[0].type == ff_value_type_empty);
            Assert::IsTrue(out.data[1].type == ff_value_type_null);
        }

        // ====================================================================
        // Layout invariants
        // ====================================================================
        TEST_METHOD(sizeof_value_is_24_bytes)
        {
            Assert::AreEqual<size_t>(24, sizeof(ff_value));
        }

        TEST_METHOD(value_layout_matches_payload_and_type)
        {
            // 16 bytes of payload (GUID / rect) plus the type, padded to pointer alignment.
            Assert::AreEqual<size_t>(8, alignof(ff_value));
            Assert::AreEqual<size_t>(16, sizeof(GUID));
            Assert::AreEqual<size_t>(16, offsetof(ff_value, type));
        }

        TEST_METHOD(value_copy_is_a_byte_copy)
        {
            // Values are POD handles, so copying must not change any bits.
            ff_value v = ff_value_new_point_float64(1.5, -3.25);
            ff_value copy = v;

            Assert::IsTrue(copy.type == v.type);
            Assert::IsTrue(bytes_equal(&copy, &v, sizeof(ff_value)));
        }
    };
}
