#include "pch.h"

// Compare a UTF-8 view to a known byte array (exact length, no null terminator assumed).
static bool utf8_equals(ff::string_view view, const char* bytes, size_t size)
{
    return view.size == size && (size == 0 || ::memcmp(view.data, bytes, size) == 0);
}

// Compare a UTF-16 view to a known char16_t array (exact length, no null terminator assumed).
static bool wide_equals(ff::wstring_view view, const char16_t* units, size_t size)
{
    return view.size == size && (size == 0 || ::memcmp(view.data, units, size * sizeof(char16_t)) == 0);
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

            const char16_t expected[] = { u'H', u'e', u'l', u'l', u'o' };
            Assert::IsTrue(wide_equals(dest, expected, 5));

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

            const char16_t expected[] = { 0x00E9 };
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

            const char16_t expected[] = { 0x20AC };
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

            const char16_t expected[] = { 0xD83D, 0xDE00 };
            Assert::IsTrue(wide_equals(dest, expected, 2));

            arena.destroy();
        }

        TEST_METHOD(utf8_to_wide_empty)
        {
            ff::arena arena;
            arena.init_heap(256);

            ff::string_view src{ "", 0 };
            ff::wstring_view dest = ff::utf8_to_wide(src, &arena);

            Assert::AreEqual((size_t)0, dest.size);

            arena.destroy();
        }

        TEST_METHOD(utf8_to_wide_from_sz_view)
        {
            ff::arena arena;
            arena.init_heap(256);

            ff::wstring_view dest = ff::utf8_to_wide(ff::sz_view("abc"), &arena);

            const char16_t expected[] = { u'a', u'b', u'c' };
            Assert::IsTrue(wide_equals(dest, expected, 3));

            arena.destroy();
        }

        TEST_METHOD(utf8_to_wide_not_null_terminated)
        {
            ff::arena arena;
            arena.init_heap(256);

            // The returned size must be exactly the converted length, with no implied terminator.
            ff::string_view src{ "AB", 2 };
            ff::wstring_view dest = ff::utf8_to_wide(src, &arena);

            Assert::AreEqual((size_t)2, dest.size);
            Assert::IsTrue(dest.data[0] == u'A');
            Assert::IsTrue(dest.data[1] == u'B');

            arena.destroy();
        }

        // ====================================================================
        // UTF-16 -> UTF-8
        // ====================================================================
        TEST_METHOD(wide_to_utf8_ascii)
        {
            ff::arena arena;
            arena.init_heap(256);

            const char16_t src_units[] = { u'H', u'e', u'l', u'l', u'o' };
            ff::wstring_view src{ src_units, 5 };
            ff::string_view dest = ff::wide_to_utf8(src, &arena);

            Assert::IsTrue(utf8_equals(dest, "Hello", 5));

            arena.destroy();
        }

        TEST_METHOD(wide_to_utf8_three_byte)
        {
            ff::arena arena;
            arena.init_heap(256);

            // U+20AC => UTF-8 E2 82 AC (3 bytes)
            const char16_t src_units[] = { 0x20AC };
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
            const char16_t src_units[] = { 0xD83D, 0xDE00 };
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

            const char16_t src_units[] = { 0 };
            ff::wstring_view src{ src_units, 0 };
            ff::string_view dest = ff::wide_to_utf8(src, &arena);

            Assert::AreEqual((size_t)0, dest.size);

            arena.destroy();
        }

        TEST_METHOD(wide_to_utf8_from_sz_view)
        {
            ff::arena arena;
            arena.init_heap(256);

            ff::string_view dest = ff::wide_to_utf8(ff::sz_view(u"abc"), &arena);

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

            const char16_t original[] = { u'A', 0x00E9, 0x20AC, 0xD83D, 0xDE00, u'Z' };
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
            Assert::AreEqual((size_t)2000, wide.size);

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
            Assert::AreEqual((size_t)5, view.size); // excludes the null terminator
            Assert::IsTrue(utf8_equals(view, "hello", 5));
        }

        TEST_METHOD(ff_svl_empty_literal)
        {
            ff::string_view view = FF_SVL("");
            Assert::AreEqual((size_t)0, view.size);
        }

        TEST_METHOD(sz_view_narrow)
        {
            const char* sz = "world";
            ff::string_view view = ff::sz_view(sz);
            Assert::AreEqual((size_t)5, view.size);
            Assert::IsTrue(utf8_equals(view, "world", 5));
        }

        TEST_METHOD(sz_view_narrow_null)
        {
            ff::string_view view = ff::sz_view((const char*)nullptr);
            Assert::AreEqual((size_t)0, view.size);
            Assert::IsNull(view.data);
        }

        TEST_METHOD(ff_wsvl_literal_length)
        {
            // 'size' must be the char16_t unit count, not the byte count.
            ff::wstring_view view = FF_WSVL(u"hello");
            Assert::AreEqual((size_t)5, view.size);

            const char16_t expected[] = { u'h', u'e', u'l', u'l', u'o' };
            Assert::IsTrue(wide_equals(view, expected, 5));
        }

        TEST_METHOD(ff_wsvl_empty_literal)
        {
            ff::wstring_view view = FF_WSVL(u"");
            Assert::AreEqual((size_t)0, view.size);
        }

        TEST_METHOD(ff_wsvl_counts_units_not_bytes)
        {
            // A surrogate pair is 2 char16_t units; "ab😀" => 'a','b', D83D, DE00 = 4 units.
            ff::wstring_view view = FF_WSVL(u"ab\U0001F600");
            Assert::AreEqual((size_t)4, view.size);
        }

        TEST_METHOD(sz_view_wide)
        {
            const char16_t* sz = u"world";
            ff::wstring_view view = ff::sz_view(sz);
            Assert::AreEqual((size_t)5, view.size);

            const char16_t expected[] = { u'w', u'o', u'r', u'l', u'd' };
            Assert::IsTrue(wide_equals(view, expected, 5));
        }

        TEST_METHOD(sz_view_wide_null)
        {
            ff::wstring_view view = ff::sz_view((const char16_t*)nullptr);
            Assert::AreEqual((size_t)0, view.size);
            Assert::IsNull(view.data);
        }

        TEST_METHOD(ff_wsvl_round_trips_through_utf8)
        {
            ff::arena arena;
            arena.init_heap(256);

            // FF_WSVL view feeds straight into the UTF-16 -> UTF-8 conversion.
            ff::string_view utf8 = ff::wide_to_utf8(FF_WSVL(u"Hello"), &arena);
            Assert::IsTrue(utf8_equals(utf8, "Hello", 5));

            arena.destroy();
        }
    };
}
