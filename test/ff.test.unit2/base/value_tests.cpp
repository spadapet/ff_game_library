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

static bool is_aligned(const void* ptr, size_t align)
{
    return ((uintptr_t)ptr & (align - 1)) == 0;
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
            ff::value v = ff::value::new_empty();
            Assert::IsTrue(v.type == ff::value_type::empty);
        }

        TEST_METHOD(null_has_null_type)
        {
            ff::value v = ff::value::new_null();
            Assert::IsTrue(v.type == ff::value_type::null);
        }

        TEST_METHOD(empty_and_null_are_distinct)
        {
            Assert::IsTrue(ff::value::new_empty().type != ff::value::new_null().type);
        }

        TEST_METHOD(zero_initialized_value_is_empty)
        {
            // value_type::empty == 0, so a zeroed value must read as empty.
            ff::value v{};
            Assert::IsTrue(v.type == ff::value_type::empty);
        }

        // ====================================================================
        // Boolean
        // ====================================================================
        TEST_METHOD(boolean_true)
        {
            ff::value v = ff::value::new_boolean(true);
            Assert::IsTrue(v.type == ff::value_type::boolean);
            Assert::IsTrue(v.b);
        }

        TEST_METHOD(boolean_false)
        {
            ff::value v = ff::value::new_boolean(false);
            Assert::IsTrue(v.type == ff::value_type::boolean);
            Assert::IsFalse(v.b);
        }

        // ====================================================================
        // GUID
        // ====================================================================
        TEST_METHOD(guid_stores_value)
        {
            GUID g = make_test_guid();
            ff::value v = ff::value::new_guid(g);

            Assert::IsTrue(v.type == ff::value_type::guid);
            Assert::IsTrue(guid_equals(v.guid, g));
        }

        // ====================================================================
        // Integers
        // ====================================================================
        TEST_METHOD(int32_stores_value)
        {
            ff::value v = ff::value::new_int32(-12345);
            Assert::IsTrue(v.type == ff::value_type::int32);
            Assert::AreEqual<int32_t>(-12345, v.i32);
        }

        TEST_METHOD(int32_stores_limits)
        {
            Assert::AreEqual<int32_t>(INT32_MIN, ff::value::new_int32(INT32_MIN).i32);
            Assert::AreEqual<int32_t>(INT32_MAX, ff::value::new_int32(INT32_MAX).i32);
        }

        TEST_METHOD(int64_stores_value)
        {
            ff::value v = ff::value::new_int64(-9000000000LL);
            Assert::IsTrue(v.type == ff::value_type::int64);
            Assert::AreEqual<int64_t>(-9000000000LL, v.i64);
        }

        TEST_METHOD(int64_stores_limits)
        {
            Assert::AreEqual<int64_t>(INT64_MIN, ff::value::new_int64(INT64_MIN).i64);
            Assert::AreEqual<int64_t>(INT64_MAX, ff::value::new_int64(INT64_MAX).i64);
        }

        // ====================================================================
        // Floating point
        // ====================================================================
        TEST_METHOD(float32_stores_value)
        {
            ff::value v = ff::value::new_float32(-2.25f);
            Assert::IsTrue(v.type == ff::value_type::float32);
            Assert::IsTrue(v.f32 == -2.25f);
        }

        TEST_METHOD(float64_stores_value)
        {
            ff::value v = ff::value::new_float64(1234.5);
            Assert::IsTrue(v.type == ff::value_type::float64);
            Assert::IsTrue(v.f64 == 1234.5);
        }

        // ====================================================================
        // Points
        // ====================================================================
        TEST_METHOD(point_int32_stores_components)
        {
            ff::value v = ff::value::new_point_int32(-7, 11);
            Assert::IsTrue(v.type == ff::value_type::point_int32);
            Assert::AreEqual<int32_t>(-7, v.point_i32[0]);
            Assert::AreEqual<int32_t>(11, v.point_i32[1]);
        }

        TEST_METHOD(point_int64_stores_components)
        {
            ff::value v = ff::value::new_point_int64(-7000000000LL, 8000000000LL);
            Assert::IsTrue(v.type == ff::value_type::point_int64);
            Assert::AreEqual<int64_t>(-7000000000LL, v.point_i64[0]);
            Assert::AreEqual<int64_t>(8000000000LL, v.point_i64[1]);
        }

        TEST_METHOD(point_float32_stores_components)
        {
            ff::value v = ff::value::new_point_float32(1.5f, -3.25f);
            Assert::IsTrue(v.type == ff::value_type::point_float32);
            Assert::IsTrue(v.point_f32[0] == 1.5f);
            Assert::IsTrue(v.point_f32[1] == -3.25f);
        }

        TEST_METHOD(point_float64_stores_components)
        {
            ff::value v = ff::value::new_point_float64(1.5, -3.25);
            Assert::IsTrue(v.type == ff::value_type::point_float64);
            Assert::IsTrue(v.point_f64[0] == 1.5);
            Assert::IsTrue(v.point_f64[1] == -3.25);
        }

        // ====================================================================
        // Rects
        // ====================================================================
        TEST_METHOD(rect_int32_stores_components)
        {
            ff::value v = ff::value::new_rect_int32(1, 2, 3, 4);
            Assert::IsTrue(v.type == ff::value_type::rect_int32);
            Assert::AreEqual<int32_t>(1, v.rect_i32[0]);
            Assert::AreEqual<int32_t>(2, v.rect_i32[1]);
            Assert::AreEqual<int32_t>(3, v.rect_i32[2]);
            Assert::AreEqual<int32_t>(4, v.rect_i32[3]);
        }

        TEST_METHOD(rect_float32_stores_components)
        {
            ff::value v = ff::value::new_rect_float32(1.5f, 2.5f, 3.5f, 4.5f);
            Assert::IsTrue(v.type == ff::value_type::rect_float32);
            Assert::IsTrue(v.rect_f32[0] == 1.5f);
            Assert::IsTrue(v.rect_f32[1] == 2.5f);
            Assert::IsTrue(v.rect_f32[2] == 3.5f);
            Assert::IsTrue(v.rect_f32[3] == 4.5f);
        }

        // ====================================================================
        // Data (ff::span overload)
        // ====================================================================
        TEST_METHOD(data_from_span_sets_metadata)
        {
            uint8_t bytes[4] = { 10, 20, 30, 40 };
            ff::span span{ bytes, 4 };

            ff::value v = ff::value::new_data(span);

            Assert::IsTrue(v.type == ff::value_type::data);
            Assert::AreEqual<size_t>(4, (size_t)v.data_a.count);
            Assert::AreEqual<size_t>(1, (size_t)v.data_a.element_size);
            Assert::AreEqual<size_t>(alignof(size_t), (size_t)v.data_a.element_align);
        }

        TEST_METHOD(data_from_span_no_arena_shares_pointer)
        {
            uint8_t bytes[4] = { 10, 20, 30, 40 };
            ff::span span{ bytes, 4 };

            ff::value v = ff::value::new_data(span);

            // Without a copy arena the value references the caller's buffer directly.
            Assert::IsTrue(v.data_a.data == bytes);
        }

        TEST_METHOD(data_from_span_with_arena_copies)
        {
            ff::arena arena;
            arena.init_heap(4096);

            uint8_t bytes[4] = { 1, 2, 3, 4 };
            ff::span span{ bytes, 4 };

            ff::value v = ff::value::new_data(span, &arena);

            Assert::IsTrue(v.data_a.data != bytes); // deep copy
            Assert::IsTrue(bytes_equal(v.data_a.data, bytes, 4));

            // Mutating the source must not affect the copy.
            bytes[0] = 0xFF;
            uint8_t expected[4] = { 1, 2, 3, 4 };
            Assert::IsTrue(bytes_equal(v.data_a.data, expected, 4));

            arena.destroy();
        }

        TEST_METHOD(data_from_span_with_arena_is_aligned)
        {
            ff::arena arena;
            arena.init_heap(4096);

            uint8_t bytes[4] = { 1, 2, 3, 4 };
            ff::span span{ bytes, 4 };

            ff::value v = ff::value::new_data(span, &arena);
            Assert::IsTrue(is_aligned(v.data_a.data, alignof(size_t)));

            arena.destroy();
        }

        // ====================================================================
        // Data (ff::array_span overload)
        // ====================================================================
        TEST_METHOD(data_from_array_span_preserves_metadata)
        {
            uint32_t nums[3] = { 0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu };

            ff::array_span as{};
            as.data = nums;
            as.count = 3;
            as.element_size = sizeof(uint32_t);
            as.element_align = alignof(uint32_t);

            ff::value v = ff::value::new_data(as);

            Assert::IsTrue(v.type == ff::value_type::data);
            Assert::IsTrue(v.data_a.data == nums);
            Assert::AreEqual<size_t>(3, (size_t)v.data_a.count);
            Assert::AreEqual<size_t>(sizeof(uint32_t), (size_t)v.data_a.element_size);
            Assert::AreEqual<size_t>(alignof(uint32_t), (size_t)v.data_a.element_align);
        }

        TEST_METHOD(data_from_array_span_with_arena_copies_all_elements)
        {
            ff::arena arena;
            arena.init_heap(4096);

            uint32_t nums[3] = { 0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu };

            ff::array_span as{};
            as.data = nums;
            as.count = 3;
            as.element_size = sizeof(uint32_t);
            as.element_align = alignof(uint32_t);

            ff::value v = ff::value::new_data(as, &arena);

            Assert::IsTrue(v.data_a.data != nums);
            Assert::IsTrue(bytes_equal(v.data_a.data, nums, 3 * sizeof(uint32_t)));

            nums[1] = 0u;
            uint32_t expected[3] = { 0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu };
            Assert::IsTrue(bytes_equal(v.data_a.data, expected, 3 * sizeof(uint32_t)));

            arena.destroy();
        }

        TEST_METHOD(data_empty_with_arena_does_not_copy)
        {
            ff::arena arena;
            arena.init_heap(4096);

            uint8_t bytes[1] = { 7 };
            ff::span span{ bytes, 0 };

            // Zero-count data has nothing to copy; the pointer is left as provided.
            ff::value v = ff::value::new_data(span, &arena);

            Assert::IsTrue(v.type == ff::value_type::data);
            Assert::AreEqual<size_t>(0, (size_t)v.data_a.count);
            Assert::IsTrue(v.data_a.data == bytes);

            arena.destroy();
        }

        // ====================================================================
        // Dict
        // ====================================================================
        TEST_METHOD(dict_stores_pointer)
        {
            // as_dict() only returns the stored pointer, so an opaque address is enough.
            int sentinel = 0;
            ff::dict* dict_ptr = reinterpret_cast<ff::dict*>(&sentinel);

            ff::value v = ff::value::new_dict(dict_ptr);

            Assert::IsTrue(v.type == ff::value_type::dict);
            Assert::IsTrue(v.as_dict() == dict_ptr);
        }

        TEST_METHOD(dict_null_pointer)
        {
            ff::value v = ff::value::new_dict(nullptr);
            Assert::IsTrue(v.type == ff::value_type::dict);
            Assert::IsNull(v.as_dict());
        }

        // ====================================================================
        // String
        // ====================================================================
        TEST_METHOD(string_no_arena_shares_pointer)
        {
            ff::string_view src{ "hello", 5 };

            ff::value v = ff::value::new_string(src);
            ff::string_view out = v.as_string();

            Assert::IsTrue(v.type == ff::value_type::string);
            Assert::IsTrue(out.data == src.data);
            Assert::AreEqual<size_t>(5, out.size);
        }

        TEST_METHOD(string_with_arena_copies)
        {
            ff::arena arena;
            arena.init_heap(4096);

            char buffer[5] = { 'h', 'e', 'l', 'l', 'o' };
            ff::string_view src{ buffer, 5 };

            ff::value v = ff::value::new_string(src, &arena);
            ff::string_view out = v.as_string();

            Assert::IsTrue(v.type == ff::value_type::string);
            Assert::IsTrue(out.data != buffer); // deep copy
            Assert::AreEqual<size_t>(5, out.size);
            Assert::IsTrue(bytes_equal(out.data, "hello", 5));

            // Mutating the source must not affect the copy.
            buffer[0] = 'X';
            Assert::IsTrue(bytes_equal(v.as_string().data, "hello", 5));

            arena.destroy();
        }

        TEST_METHOD(string_empty)
        {
            ff::string_view src{ "", 0 };

            ff::value v = ff::value::new_string(src);
            ff::string_view out = v.as_string();

            Assert::IsTrue(v.type == ff::value_type::string);
            Assert::AreEqual<size_t>(0, out.size);
        }

        // ====================================================================
        // Array
        // ====================================================================
        TEST_METHOD(array_no_arena_shares_pointer_and_size)
        {
            ff::value items[3] =
            {
                ff::value::new_int32(10),
                ff::value::new_int32(20),
                ff::value::new_int32(30),
            };

            ff::value v = ff::value::new_array(items, 3);

            Assert::IsTrue(v.type == ff::value_type::array);
            Assert::IsTrue(v.as_array() == items);
            Assert::AreEqual<size_t>(3, v.as_array_size());
        }

        TEST_METHOD(array_elements_readable)
        {
            ff::value items[3] =
            {
                ff::value::new_int32(10),
                ff::value::new_int32(20),
                ff::value::new_int32(30),
            };

            ff::value v = ff::value::new_array(items, 3);
            ff::value* arr = v.as_array();

            for (int32_t i = 0; i < 3; i++)
            {
                Assert::IsTrue(arr[i].type == ff::value_type::int32);
                Assert::AreEqual<int32_t>((i + 1) * 10, arr[i].i32);
            }
        }

        TEST_METHOD(array_with_arena_copies)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::value items[2] =
            {
                ff::value::new_int32(10),
                ff::value::new_int32(20),
            };

            ff::value v = ff::value::new_array(items, 2, &arena);
            ff::value* arr = v.as_array();

            Assert::IsTrue(arr != items); // deep copy
            Assert::AreEqual<size_t>(2, v.as_array_size());
            Assert::AreEqual<int32_t>(10, arr[0].i32);
            Assert::AreEqual<int32_t>(20, arr[1].i32);

            // Mutating the source must not affect the copy.
            items[0] = ff::value::new_int32(999);
            Assert::AreEqual<int32_t>(10, v.as_array()[0].i32);

            arena.destroy();
        }

        TEST_METHOD(array_empty)
        {
            ff::value items[1] = { ff::value::new_int32(0) };

            ff::value v = ff::value::new_array(items, 0);

            Assert::IsTrue(v.type == ff::value_type::array);
            Assert::AreEqual<size_t>(0, v.as_array_size());
        }

        TEST_METHOD(array_of_mixed_types_round_trip)
        {
            ff::value items[3] =
            {
                ff::value::new_boolean(true),
                ff::value::new_float64(2.5),
                ff::value::new_int64(42),
            };

            ff::value v = ff::value::new_array(items, 3);
            ff::value* arr = v.as_array();

            Assert::IsTrue(arr[0].type == ff::value_type::boolean);
            Assert::IsTrue(arr[0].b);
            Assert::IsTrue(arr[1].type == ff::value_type::float64);
            Assert::IsTrue(arr[1].f64 == 2.5);
            Assert::IsTrue(arr[2].type == ff::value_type::int64);
            Assert::AreEqual<int64_t>(42, arr[2].i64);
        }

        // ====================================================================
        // Layout invariants
        // ====================================================================
        TEST_METHOD(sizeof_value_is_24_bytes)
        {
            Assert::AreEqual<size_t>(24, sizeof(ff::value));
        }
    };
}
