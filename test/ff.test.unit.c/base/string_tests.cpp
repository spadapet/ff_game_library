#include "pch.h"

// Compare a UTF-8 view to a known byte array (exact length, no null terminator assumed).
static bool utf8_equals(ff_string_view view, const char* bytes, size_t size)
{
    return view.count == size && (size == 0 || ::memcmp(view.data, bytes, size) == 0);
}

// Compare a UTF-16 view to a known wchar_t array (exact length, no null terminator assumed).
static bool wide_equals(ff_wstring_view view, const wchar_t* units, size_t size)
{
    return view.count == size && (size == 0 || ::memcmp(view.data, units, size * sizeof(wchar_t)) == 0);
}

namespace ff::test::base
{
    TEST_CLASS(string_tests)
    {
    public:
        // ====================================================================
        // UTF-8 -> UTF-16
        // ====================================================================
        TEST_METHOD(utf8_to_wide_ascii)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 256);

            ff_string_view src{ "Hello", 5 };
            ff_wstring_view dest = ff_utf8_to_wide(src, &arena, false);

            const wchar_t expected[] = { L'H', L'e', L'l', L'l', L'o' };
            Assert::IsTrue(wide_equals(dest, expected, 5));
            Assert::IsTrue(dest.data != nullptr);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(utf8_to_wide_two_byte)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 256);

            // "é" U+00E9 => UTF-8 C3 A9 (2 bytes) => UTF-16 0x00E9 (1 unit)
            const char src_bytes[] = { (char)0xC3, (char)0xA9 };
            ff_string_view src{ src_bytes, 2 };
            ff_wstring_view dest = ff_utf8_to_wide(src, &arena, false);

            const wchar_t expected[] = { 0x00E9 };
            Assert::IsTrue(wide_equals(dest, expected, 1));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(utf8_to_wide_three_byte)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 256);

            // "€" U+20AC => UTF-8 E2 82 AC (3 bytes) => UTF-16 0x20AC (1 unit)
            const char src_bytes[] = { (char)0xE2, (char)0x82, (char)0xAC };
            ff_string_view src{ src_bytes, 3 };
            ff_wstring_view dest = ff_utf8_to_wide(src, &arena, false);

            const wchar_t expected[] = { 0x20AC };
            Assert::IsTrue(wide_equals(dest, expected, 1));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(utf8_to_wide_surrogate_pair)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 256);

            // "😀" U+1F600 => UTF-8 F0 9F 98 80 (4 bytes) => UTF-16 surrogate pair D83D DE00 (2 units)
            const char src_bytes[] = { (char)0xF0, (char)0x9F, (char)0x98, (char)0x80 };
            ff_string_view src{ src_bytes, 4 };
            ff_wstring_view dest = ff_utf8_to_wide(src, &arena, false);

            const wchar_t expected[] = { 0xD83D, 0xDE00 };
            Assert::IsTrue(wide_equals(dest, expected, 2));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(utf8_to_wide_empty)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 256);

            ff_string_view src{ "", 0 };
            ff_wstring_view dest = ff_utf8_to_wide(src, &arena, false);

            Assert::AreEqual((size_t)0, dest.count);
            Assert::IsTrue(dest.data != nullptr);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(utf8_to_wide_from_sz_view)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 256);

            ff_wstring_view dest = ff_utf8_to_wide(ff_sz_view("abc"), &arena, false);

            const wchar_t expected[] = { L'a', L'b', L'c' };
            Assert::IsTrue(wide_equals(dest, expected, 3));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(utf8_to_wide_not_null_terminated)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 256);

            // The reported size is exactly the converted length; no extra terminator is stored
            // alongside it and callers must append one explicitly when needed.
            ff_string_view src{ "AB", 2 };
            ff_wstring_view dest = ff_utf8_to_wide(src, &arena, false);

            Assert::AreEqual((size_t)2, dest.count);
            Assert::IsTrue(dest.data != nullptr);
            Assert::IsTrue(dest.data[0] == L'A');
            Assert::IsTrue(dest.data[1] == L'B');

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // UTF-16 -> UTF-8
        // ====================================================================
        TEST_METHOD(wide_to_utf8_ascii)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 256);

            const wchar_t src_units[] = { L'H', L'e', L'l', L'l', L'o' };
            ff_wstring_view src{ src_units, 5 };
            ff_string_view dest = ff_wide_to_utf8(src, &arena, false);

            Assert::IsTrue(utf8_equals(dest, "Hello", 5));
            Assert::IsTrue(dest.data != nullptr);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(wide_to_utf8_three_byte)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 256);

            // U+20AC => UTF-8 E2 82 AC (3 bytes)
            const wchar_t src_units[] = { 0x20AC };
            ff_wstring_view src{ src_units, 1 };
            ff_string_view dest = ff_wide_to_utf8(src, &arena, false);

            const char expected[] = { (char)0xE2, (char)0x82, (char)0xAC };
            Assert::IsTrue(utf8_equals(dest, expected, 3));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(wide_to_utf8_surrogate_pair)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 256);

            // Surrogate pair D83D DE00 => U+1F600 => UTF-8 F0 9F 98 80 (4 bytes)
            const wchar_t src_units[] = { 0xD83D, 0xDE00 };
            ff_wstring_view src{ src_units, 2 };
            ff_string_view dest = ff_wide_to_utf8(src, &arena, false);

            const char expected[] = { (char)0xF0, (char)0x9F, (char)0x98, (char)0x80 };
            Assert::IsTrue(utf8_equals(dest, expected, 4));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(wide_to_utf8_empty)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 256);

            const wchar_t src_units[] = { 0 };
            ff_wstring_view src{ src_units, 0 };
            ff_string_view dest = ff_wide_to_utf8(src, &arena, false);

            Assert::AreEqual((size_t)0, dest.count);
            Assert::IsTrue(dest.data != nullptr);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(wide_to_utf8_from_sz_view)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 256);

            ff_string_view dest = ff_wide_to_utf8(ff_wz_view(L"abc"), &arena, false);

            Assert::IsTrue(utf8_equals(dest, "abc", 3));

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Round trips
        // ====================================================================
        TEST_METHOD(round_trip_utf8_wide_utf8)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 1024);

            // Mix of ASCII, 2-byte, 3-byte, and 4-byte (surrogate) sequences.
            const char original[] = { 'A', (char)0xC3, (char)0xA9, (char)0xE2, (char)0x82, (char)0xAC, (char)0xF0, (char)0x9F, (char)0x98, (char)0x80, 'Z' };
            ff_string_view src{ original, sizeof(original) };

            ff_wstring_view wide = ff_utf8_to_wide(src, &arena, false);
            ff_string_view back = ff_wide_to_utf8(wide, &arena, false);

            Assert::IsTrue(utf8_equals(back, original, sizeof(original)));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(round_trip_wide_utf8_wide)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 1024);

            const wchar_t original[] = { L'A', 0x00E9, 0x20AC, 0xD83D, 0xDE00, L'Z' };
            ff_wstring_view src{ original, _countof(original) };

            ff_string_view utf8 = ff_wide_to_utf8(src, &arena, false);
            ff_wstring_view back = ff_utf8_to_wide(utf8, &arena, false);

            Assert::IsTrue(wide_equals(back, original, _countof(original)));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(round_trip_long_ascii)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 64); // small so the conversion forces arena growth

            char buffer[2000];
            for (int i = 0; i < 2000; ++i)
            {
                buffer[i] = (char)('a' + (i % 26));
            }

            ff_string_view src{ buffer, sizeof(buffer) };
            ff_wstring_view wide = ff_utf8_to_wide(src, &arena, false);
            Assert::AreEqual((size_t)2000, wide.count);

            ff_string_view back = ff_wide_to_utf8(wide, &arena, false);
            Assert::IsTrue(utf8_equals(back, buffer, sizeof(buffer)));

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // string_view helpers: FF_SVL / FF_WSVL / sz_view
        // ====================================================================
        TEST_METHOD(ff_svl_literal_length)
        {
            ff_string_view view = FF_SVL("hello");
            Assert::AreEqual((size_t)5, view.count); // excludes the null terminator
            Assert::IsTrue(utf8_equals(view, "hello", 5));
        }

        TEST_METHOD(ff_svl_empty_literal)
        {
            ff_string_view view = FF_SVL("");
            Assert::AreEqual((size_t)0, view.count);
        }

        TEST_METHOD(sz_view_narrow)
        {
            const char* sz = "world";
            ff_string_view view = ff_sz_view(sz);
            Assert::AreEqual((size_t)5, view.count);
            Assert::IsTrue(utf8_equals(view, "world", 5));
        }

        TEST_METHOD(sz_view_narrow_null)
        {
            ff_string_view view = ff_sz_view((const char*)nullptr);
            Assert::AreEqual((size_t)0, view.count);
            Assert::IsNull(view.data);
        }

        TEST_METHOD(ff_wsvl_literal_length)
        {
            // 'size' must be the wchar_t unit count, not the byte count.
            ff_wstring_view view = FF_WSVL(L"hello");
            Assert::AreEqual((size_t)5, view.count);

            const wchar_t expected[] = { L'h', L'e', L'l', L'l', L'o' };
            Assert::IsTrue(wide_equals(view, expected, 5));
        }

        TEST_METHOD(ff_wsvl_empty_literal)
        {
            ff_wstring_view view = FF_WSVL(L"");
            Assert::AreEqual((size_t)0, view.count);
        }

        TEST_METHOD(ff_wsvl_counts_units_not_bytes)
        {
            // A surrogate pair is 2 wchar_t units; "ab" + U+1F600 => 'a', 'b', D83D, DE00 = 4 units.
            ff_wstring_view view = FF_WSVL(L"ab\U0001F600");
            Assert::AreEqual((size_t)4, view.count);
        }

        TEST_METHOD(sz_view_wide)
        {
            const wchar_t* sz = L"world";
            ff_wstring_view view = ff_wz_view(sz);
            Assert::AreEqual((size_t)5, view.count);

            const wchar_t expected[] = { L'w', L'o', L'r', L'l', L'd' };
            Assert::IsTrue(wide_equals(view, expected, 5));
        }

        TEST_METHOD(sz_view_wide_null)
        {
            ff_wstring_view view = ff_wz_view((const wchar_t*)nullptr);
            Assert::AreEqual((size_t)0, view.count);
            Assert::IsNull(view.data);
        }

        TEST_METHOD(ff_wsvl_round_trips_through_utf8)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 256);

            // FF_WSVL view feeds straight into the UTF-16 -> UTF-8 conversion.
            ff_string_view utf8 = ff_wide_to_utf8(FF_WSVL(L"Hello"), &arena, false);
            Assert::IsTrue(utf8_equals(utf8, "Hello", 5));

            ff_arena_destroy(&arena);
        }
    };
}
