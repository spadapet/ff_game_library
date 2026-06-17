#include "pch.h"

// Compare a UTF-8 view to a known byte array (exact length, no null terminator assumed).
static bool utf8_equals(ff::string_view view, const char* bytes, size_t size)
{
    return view.count == size && (size == 0 || ::memcmp(view.data, bytes, size) == 0);
}

// Compare a UTF-16 view to a known wchar_t array (exact length, no null terminator assumed).
static bool wide_equals(ff::wstring_view view, const wchar_t* units, size_t size)
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
            ff::arena arena;
            arena.init_heap(256);

            ff::string_view src{ "Hello", 5 };
            ff::wstring_view dest = ff::utf8_to_wide(src, &arena);

            const wchar_t expected[] = { L'H', L'e', L'l', L'l', L'o' };
            Assert::IsTrue(wide_equals(dest, expected, 5));
            Assert::IsTrue(dest.data[dest.count] == 0); // null-terminated past the reported size

            arena.destroy();
        }

        TEST_METHOD(utf8_to_wide_two_byte)
        {
            ff::arena arena;
            arena.init_heap(256);

            // "é" U+00E9 => UTF-8 C3 A9 (2 bytes) => UTF-16 0x00E9 (1 unit)
            const char src_bytes[] = { (char)0xC3, (char)0xA9 };
            ff::string_view src{ src_bytes, 2 };
            ff::wstring_view dest = ff::utf8_to_wide(src, &arena);

            const wchar_t expected[] = { 0x00E9 };
            Assert::IsTrue(wide_equals(dest, expected, 1));

            arena.destroy();
        }

        TEST_METHOD(utf8_to_wide_three_byte)
        {
            ff::arena arena;
            arena.init_heap(256);

            // "€" U+20AC => UTF-8 E2 82 AC (3 bytes) => UTF-16 0x20AC (1 unit)
            const char src_bytes[] = { (char)0xE2, (char)0x82, (char)0xAC };
            ff::string_view src{ src_bytes, 3 };
            ff::wstring_view dest = ff::utf8_to_wide(src, &arena);

            const wchar_t expected[] = { 0x20AC };
            Assert::IsTrue(wide_equals(dest, expected, 1));

            arena.destroy();
        }

        TEST_METHOD(utf8_to_wide_surrogate_pair)
        {
            ff::arena arena;
            arena.init_heap(256);

            // "😀" U+1F600 => UTF-8 F0 9F 98 80 (4 bytes) => UTF-16 surrogate pair D83D DE00 (2 units)
            const char src_bytes[] = { (char)0xF0, (char)0x9F, (char)0x98, (char)0x80 };
            ff::string_view src{ src_bytes, 4 };
            ff::wstring_view dest = ff::utf8_to_wide(src, &arena);

            const wchar_t expected[] = { 0xD83D, 0xDE00 };
            Assert::IsTrue(wide_equals(dest, expected, 2));

            arena.destroy();
        }

        TEST_METHOD(utf8_to_wide_empty)
        {
            ff::arena arena;
            arena.init_heap(256);

            ff::string_view src{ "", 0 };
            ff::wstring_view dest = ff::utf8_to_wide(src, &arena);

            Assert::AreEqual((size_t)0, dest.count);
            Assert::IsTrue(dest.data != nullptr);
            Assert::IsTrue(dest.data[0] == 0); // empty but still null-terminated

            arena.destroy();
        }

        TEST_METHOD(utf8_to_wide_from_sz_view)
        {
            ff::arena arena;
            arena.init_heap(256);

            ff::wstring_view dest = ff::utf8_to_wide(ff::sz_view("abc"), &arena);

            const wchar_t expected[] = { L'a', L'b', L'c' };
            Assert::IsTrue(wide_equals(dest, expected, 3));

            arena.destroy();
        }

        TEST_METHOD(utf8_to_wide_not_null_terminated)
        {
            ff::arena arena;
            arena.init_heap(256);

            // The returned size is exactly the converted length (the terminator is not counted),
            // but the buffer is still null-terminated at data[size].
            ff::string_view src{ "AB", 2 };
            ff::wstring_view dest = ff::utf8_to_wide(src, &arena);

            Assert::AreEqual((size_t)2, dest.count);
            Assert::IsTrue(dest.data[0] == L'A');
            Assert::IsTrue(dest.data[1] == L'B');
            Assert::IsTrue(dest.data[2] == 0); // explicit terminator just past the size

            arena.destroy();
        }

        // ====================================================================
        // UTF-16 -> UTF-8
        // ====================================================================
        TEST_METHOD(wide_to_utf8_ascii)
        {
            ff::arena arena;
            arena.init_heap(256);

            const wchar_t src_units[] = { L'H', L'e', L'l', L'l', L'o' };
            ff::wstring_view src{ src_units, 5 };
            ff::string_view dest = ff::wide_to_utf8(src, &arena);

            Assert::IsTrue(utf8_equals(dest, "Hello", 5));
            Assert::IsTrue(dest.data[dest.count] == 0); // null-terminated past the reported size

            arena.destroy();
        }

        TEST_METHOD(wide_to_utf8_three_byte)
        {
            ff::arena arena;
            arena.init_heap(256);

            // U+20AC => UTF-8 E2 82 AC (3 bytes)
            const wchar_t src_units[] = { 0x20AC };
            ff::wstring_view src{ src_units, 1 };
            ff::string_view dest = ff::wide_to_utf8(src, &arena);

            const char expected[] = { (char)0xE2, (char)0x82, (char)0xAC };
            Assert::IsTrue(utf8_equals(dest, expected, 3));

            arena.destroy();
        }

        TEST_METHOD(wide_to_utf8_surrogate_pair)
        {
            ff::arena arena;
            arena.init_heap(256);

            // Surrogate pair D83D DE00 => U+1F600 => UTF-8 F0 9F 98 80 (4 bytes)
            const wchar_t src_units[] = { 0xD83D, 0xDE00 };
            ff::wstring_view src{ src_units, 2 };
            ff::string_view dest = ff::wide_to_utf8(src, &arena);

            const char expected[] = { (char)0xF0, (char)0x9F, (char)0x98, (char)0x80 };
            Assert::IsTrue(utf8_equals(dest, expected, 4));

            arena.destroy();
        }

        TEST_METHOD(wide_to_utf8_empty)
        {
            ff::arena arena;
            arena.init_heap(256);

            const wchar_t src_units[] = { 0 };
            ff::wstring_view src{ src_units, 0 };
            ff::string_view dest = ff::wide_to_utf8(src, &arena);

            Assert::AreEqual((size_t)0, dest.count);
            Assert::IsTrue(dest.data != nullptr);
            Assert::IsTrue(dest.data[0] == 0); // empty but still null-terminated

            arena.destroy();
        }

        TEST_METHOD(wide_to_utf8_from_sz_view)
        {
            ff::arena arena;
            arena.init_heap(256);

            ff::string_view dest = ff::wide_to_utf8(ff::sz_view(L"abc"), &arena);

            Assert::IsTrue(utf8_equals(dest, "abc", 3));

            arena.destroy();
        }

        // ====================================================================
        // Round trips
        // ====================================================================
        TEST_METHOD(round_trip_utf8_wide_utf8)
        {
            ff::arena arena;
            arena.init_heap(1024);

            // Mix of ASCII, 2-byte, 3-byte, and 4-byte (surrogate) sequences.
            const char original[] = { 'A', (char)0xC3, (char)0xA9, (char)0xE2, (char)0x82, (char)0xAC, (char)0xF0, (char)0x9F, (char)0x98, (char)0x80, 'Z' };
            ff::string_view src{ original, sizeof(original) };

            ff::wstring_view wide = ff::utf8_to_wide(src, &arena);
            ff::string_view back = ff::wide_to_utf8(wide, &arena);

            Assert::IsTrue(utf8_equals(back, original, sizeof(original)));

            arena.destroy();
        }

        TEST_METHOD(round_trip_wide_utf8_wide)
        {
            ff::arena arena;
            arena.init_heap(1024);

            const wchar_t original[] = { L'A', 0x00E9, 0x20AC, 0xD83D, 0xDE00, L'Z' };
            ff::wstring_view src{ original, _countof(original) };

            ff::string_view utf8 = ff::wide_to_utf8(src, &arena);
            ff::wstring_view back = ff::utf8_to_wide(utf8, &arena);

            Assert::IsTrue(wide_equals(back, original, _countof(original)));

            arena.destroy();
        }

        TEST_METHOD(round_trip_long_ascii)
        {
            ff::arena arena;
            arena.init_heap(64); // small so the conversion forces arena growth

            char buffer[2000];
            for (int i = 0; i < 2000; ++i)
            {
                buffer[i] = (char)('a' + (i % 26));
            }

            ff::string_view src{ buffer, sizeof(buffer) };
            ff::wstring_view wide = ff::utf8_to_wide(src, &arena);
            Assert::AreEqual((size_t)2000, wide.count);

            ff::string_view back = ff::wide_to_utf8(wide, &arena);
            Assert::IsTrue(utf8_equals(back, buffer, sizeof(buffer)));

            arena.destroy();
        }

        // ====================================================================
        // string_view helpers: FF_SVL / FF_WSVL / sz_view
        // ====================================================================
        TEST_METHOD(ff_svl_literal_length)
        {
            ff::string_view view = FF_SVL("hello");
            Assert::AreEqual((size_t)5, view.count); // excludes the null terminator
            Assert::IsTrue(utf8_equals(view, "hello", 5));
        }

        TEST_METHOD(ff_svl_empty_literal)
        {
            ff::string_view view = FF_SVL("");
            Assert::AreEqual((size_t)0, view.count);
        }

        TEST_METHOD(sz_view_narrow)
        {
            const char* sz = "world";
            ff::string_view view = ff::sz_view(sz);
            Assert::AreEqual((size_t)5, view.count);
            Assert::IsTrue(utf8_equals(view, "world", 5));
        }

        TEST_METHOD(sz_view_narrow_null)
        {
            ff::string_view view = ff::sz_view((const char*)nullptr);
            Assert::AreEqual((size_t)0, view.count);
            Assert::IsNull(view.data);
        }

        TEST_METHOD(ff_wsvl_literal_length)
        {
            // 'size' must be the wchar_t unit count, not the byte count.
            ff::wstring_view view = FF_WSVL(L"hello");
            Assert::AreEqual((size_t)5, view.count);

            const wchar_t expected[] = { L'h', L'e', L'l', L'l', L'o' };
            Assert::IsTrue(wide_equals(view, expected, 5));
        }

        TEST_METHOD(ff_wsvl_empty_literal)
        {
            ff::wstring_view view = FF_WSVL(L"");
            Assert::AreEqual((size_t)0, view.count);
        }

        TEST_METHOD(ff_wsvl_counts_units_not_bytes)
        {
            // A surrogate pair is 2 wchar_t units; "ab" + U+1F600 => 'a', 'b', D83D, DE00 = 4 units.
            ff::wstring_view view = FF_WSVL(L"ab\U0001F600");
            Assert::AreEqual((size_t)4, view.count);
        }

        TEST_METHOD(sz_view_wide)
        {
            const wchar_t* sz = L"world";
            ff::wstring_view view = ff::sz_view(sz);
            Assert::AreEqual((size_t)5, view.count);

            const wchar_t expected[] = { L'w', L'o', L'r', L'l', L'd' };
            Assert::IsTrue(wide_equals(view, expected, 5));
        }

        TEST_METHOD(sz_view_wide_null)
        {
            ff::wstring_view view = ff::sz_view((const wchar_t*)nullptr);
            Assert::AreEqual((size_t)0, view.count);
            Assert::IsNull(view.data);
        }

        TEST_METHOD(ff_wsvl_round_trips_through_utf8)
        {
            ff::arena arena;
            arena.init_heap(256);

            // FF_WSVL view feeds straight into the UTF-16 -> UTF-8 conversion.
            ff::string_view utf8 = ff::wide_to_utf8(FF_WSVL(L"Hello"), &arena);
            Assert::IsTrue(utf8_equals(utf8, "Hello", 5));

            arena.destroy();
        }
    };
}
