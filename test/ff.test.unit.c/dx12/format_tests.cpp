#include "pch.h"

namespace ff::test::base
{
    TEST_CLASS(format_tests)
    {
    public:
        TEST_METHOD(compressed_formats)
        {
            Assert::IsTrue(ff_dx12_format_compressed(DXGI_FORMAT_BC1_UNORM));
            Assert::IsTrue(ff_dx12_format_compressed(DXGI_FORMAT_BC2_UNORM));
            Assert::IsTrue(ff_dx12_format_compressed(DXGI_FORMAT_BC3_UNORM));
            Assert::IsTrue(ff_dx12_format_compressed(DXGI_FORMAT_BC3_UNORM_SRGB));
            Assert::IsFalse(ff_dx12_format_compressed(DXGI_FORMAT_R8G8B8A8_UNORM));
            Assert::IsFalse(ff_dx12_format_compressed(DXGI_FORMAT_B8G8R8A8_UNORM));
            Assert::IsFalse(ff_dx12_format_compressed(DXGI_FORMAT_UNKNOWN));
        }

        TEST_METHOD(color_formats)
        {
            Assert::IsTrue(ff_dx12_format_color(DXGI_FORMAT_R8G8B8A8_UNORM));
            Assert::IsTrue(ff_dx12_format_color(DXGI_FORMAT_B8G8R8A8_UNORM));
            Assert::IsTrue(ff_dx12_format_color(DXGI_FORMAT_BC1_UNORM));
            Assert::IsTrue(ff_dx12_format_color(DXGI_FORMAT_R8_UNORM));

            // A8 carries only alpha, and the integer/depth formats are not color.
            Assert::IsFalse(ff_dx12_format_color(DXGI_FORMAT_A8_UNORM));
            Assert::IsFalse(ff_dx12_format_color(DXGI_FORMAT_R8_UINT));
            Assert::IsFalse(ff_dx12_format_color(DXGI_FORMAT_D24_UNORM_S8_UINT));
            Assert::IsFalse(ff_dx12_format_color(DXGI_FORMAT_UNKNOWN));
        }

        TEST_METHOD(palette_format)
        {
            Assert::IsTrue(ff_dx12_format_palette(DXGI_FORMAT_R8_UINT));
            Assert::IsFalse(ff_dx12_format_palette(DXGI_FORMAT_R8_UNORM));
            Assert::IsFalse(ff_dx12_format_palette(DXGI_FORMAT_R8G8B8A8_UNORM));
            Assert::IsFalse(ff_dx12_format_palette(DXGI_FORMAT_UNKNOWN));
        }

        TEST_METHOD(has_alpha)
        {
            Assert::IsTrue(ff_dx12_format_has_alpha(DXGI_FORMAT_R8G8B8A8_UNORM));
            Assert::IsTrue(ff_dx12_format_has_alpha(DXGI_FORMAT_B8G8R8A8_UNORM));
            Assert::IsTrue(ff_dx12_format_has_alpha(DXGI_FORMAT_A8_UNORM));
            Assert::IsTrue(ff_dx12_format_has_alpha(DXGI_FORMAT_BC2_UNORM));
            Assert::IsTrue(ff_dx12_format_has_alpha(DXGI_FORMAT_BC3_UNORM));

            // BC1 has at most 1-bit punch-through alpha, which the legacy code did not count.
            Assert::IsFalse(ff_dx12_format_has_alpha(DXGI_FORMAT_BC1_UNORM));
            Assert::IsFalse(ff_dx12_format_has_alpha(DXGI_FORMAT_B8G8R8X8_UNORM));
            Assert::IsFalse(ff_dx12_format_has_alpha(DXGI_FORMAT_R8_UNORM));
            Assert::IsFalse(ff_dx12_format_has_alpha(DXGI_FORMAT_UNKNOWN));
        }

        TEST_METHOD(supports_pre_multiplied_alpha)
        {
            Assert::IsTrue(ff_dx12_format_supports_pre_multiplied_alpha(DXGI_FORMAT_R8G8B8A8_UNORM));
            Assert::IsTrue(ff_dx12_format_supports_pre_multiplied_alpha(DXGI_FORMAT_B8G8R8A8_UNORM));

            // Needs all three of: uncompressed, color, and alpha.
            Assert::IsFalse(ff_dx12_format_supports_pre_multiplied_alpha(DXGI_FORMAT_BC3_UNORM));
            Assert::IsFalse(ff_dx12_format_supports_pre_multiplied_alpha(DXGI_FORMAT_A8_UNORM));
            Assert::IsFalse(ff_dx12_format_supports_pre_multiplied_alpha(DXGI_FORMAT_B8G8R8X8_UNORM));
            Assert::IsFalse(ff_dx12_format_supports_pre_multiplied_alpha(DXGI_FORMAT_UNKNOWN));
        }

        TEST_METHOD(supports_pre_multiplied_alpha_agrees_with_its_parts)
        {
            DXGI_FORMAT formats[] =
            {
                DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8X8_UNORM,
                DXGI_FORMAT_A8_UNORM, DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UINT,
                DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC2_UNORM, DXGI_FORMAT_BC3_UNORM,
                DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_UNKNOWN,
            };

            for (DXGI_FORMAT format : formats)
            {
                bool expected =
                    !ff_dx12_format_compressed(format) &&
                    ff_dx12_format_color(format) &&
                    ff_dx12_format_has_alpha(format);

                Assert::AreEqual(expected, ff_dx12_format_supports_pre_multiplied_alpha(format));
            }
        }

        TEST_METHOD(bits_per_pixel)
        {
            Assert::AreEqual((size_t)32, ff_dx12_format_bits_per_pixel(DXGI_FORMAT_R8G8B8A8_UNORM));
            Assert::AreEqual((size_t)32, ff_dx12_format_bits_per_pixel(DXGI_FORMAT_B8G8R8A8_UNORM));
            Assert::AreEqual((size_t)128, ff_dx12_format_bits_per_pixel(DXGI_FORMAT_R32G32B32A32_FLOAT));
            Assert::AreEqual((size_t)8, ff_dx12_format_bits_per_pixel(DXGI_FORMAT_R8_UINT));
            Assert::AreEqual((size_t)1, ff_dx12_format_bits_per_pixel(DXGI_FORMAT_R1_UNORM));
            Assert::AreEqual((size_t)4, ff_dx12_format_bits_per_pixel(DXGI_FORMAT_BC1_UNORM));
            Assert::AreEqual((size_t)8, ff_dx12_format_bits_per_pixel(DXGI_FORMAT_BC3_UNORM));
            Assert::AreEqual((size_t)0, ff_dx12_format_bits_per_pixel(DXGI_FORMAT_UNKNOWN));
        }

        TEST_METHOD(parse_known_names)
        {
            Assert::IsTrue(DXGI_FORMAT_R8G8B8A8_UNORM == ff_dx12_format_parse(FF_SVL("rgba32")));
            Assert::IsTrue(DXGI_FORMAT_B8G8R8A8_UNORM == ff_dx12_format_parse(FF_SVL("bgra32")));
            Assert::IsTrue(DXGI_FORMAT_BC1_UNORM == ff_dx12_format_parse(FF_SVL("bc1")));
            Assert::IsTrue(DXGI_FORMAT_BC2_UNORM == ff_dx12_format_parse(FF_SVL("bc2")));
            Assert::IsTrue(DXGI_FORMAT_BC3_UNORM == ff_dx12_format_parse(FF_SVL("bc3")));
            Assert::IsTrue(DXGI_FORMAT_R8_UNORM == ff_dx12_format_parse(FF_SVL("gray")));
            Assert::IsTrue(DXGI_FORMAT_R1_UNORM == ff_dx12_format_parse(FF_SVL("bw")));
            Assert::IsTrue(DXGI_FORMAT_A8_UNORM == ff_dx12_format_parse(FF_SVL("alpha")));
        }

        TEST_METHOD(parse_palette_aliases)
        {
            Assert::IsTrue(DXGI_FORMAT_R8_UINT == ff_dx12_format_parse(FF_SVL("pal")));
            Assert::IsTrue(DXGI_FORMAT_R8_UINT == ff_dx12_format_parse(FF_SVL("palette")));
        }

        TEST_METHOD(parse_unknown_returns_unknown)
        {
            // The legacy version asserted here. Returning UNKNOWN lets a caller report a bad
            // asset name instead of taking down the process.
            Assert::IsTrue(DXGI_FORMAT_UNKNOWN == ff_dx12_format_parse(FF_SVL("nonsense")));
            Assert::IsTrue(DXGI_FORMAT_UNKNOWN == ff_dx12_format_parse(ff_string_view_empty()));
        }

        TEST_METHOD(parse_is_not_a_prefix_match)
        {
            // "pal" is a real name and a prefix of "palette", so a length-insensitive compare
            // would mis-parse both of these.
            Assert::IsTrue(DXGI_FORMAT_UNKNOWN == ff_dx12_format_parse(FF_SVL("pal32")));
            Assert::IsTrue(DXGI_FORMAT_UNKNOWN == ff_dx12_format_parse(FF_SVL("rgba")));
            Assert::IsTrue(DXGI_FORMAT_UNKNOWN == ff_dx12_format_parse(FF_SVL("bc")));
        }

        TEST_METHOD(parse_is_case_sensitive)
        {
            Assert::IsTrue(DXGI_FORMAT_UNKNOWN == ff_dx12_format_parse(FF_SVL("RGBA32")));
        }

        TEST_METHOD(fix_unknown_becomes_rgba)
        {
            Assert::IsTrue(DXGI_FORMAT_R8G8B8A8_UNORM == ff_dx12_format_fix(DXGI_FORMAT_UNKNOWN, 64, 64, 1));
        }

        TEST_METHOD(fix_leaves_uncompressed_formats_alone)
        {
            // Uncompressed formats have no size restrictions, so odd sizes are fine.
            Assert::IsTrue(DXGI_FORMAT_B8G8R8A8_UNORM == ff_dx12_format_fix(DXGI_FORMAT_B8G8R8A8_UNORM, 63, 31, 1));
            Assert::IsTrue(DXGI_FORMAT_R8_UINT == ff_dx12_format_fix(DXGI_FORMAT_R8_UINT, 63, 31, 4));
        }

        TEST_METHOD(fix_keeps_compressed_when_size_is_valid)
        {
            Assert::IsTrue(DXGI_FORMAT_BC3_UNORM == ff_dx12_format_fix(DXGI_FORMAT_BC3_UNORM, 64, 32, 1));
            Assert::IsTrue(DXGI_FORMAT_BC1_UNORM == ff_dx12_format_fix(DXGI_FORMAT_BC1_UNORM, 12, 8, 1));
        }

        TEST_METHOD(fix_rejects_compressed_when_not_multiple_of_four)
        {
            Assert::IsTrue(DXGI_FORMAT_R8G8B8A8_UNORM == ff_dx12_format_fix(DXGI_FORMAT_BC3_UNORM, 63, 64, 1));
            Assert::IsTrue(DXGI_FORMAT_R8G8B8A8_UNORM == ff_dx12_format_fix(DXGI_FORMAT_BC3_UNORM, 64, 63, 1));
            Assert::IsTrue(DXGI_FORMAT_R8G8B8A8_UNORM == ff_dx12_format_fix(DXGI_FORMAT_BC3_UNORM, 2, 2, 1));
        }

        TEST_METHOD(fix_rejects_compressed_mips_when_not_power_of_two)
        {
            // 12x8 is a legal single-mip BC size but not a legal mip chain, since 12 is not a
            // power of two. This pair is what separates the two size rules.
            Assert::IsTrue(DXGI_FORMAT_BC1_UNORM == ff_dx12_format_fix(DXGI_FORMAT_BC1_UNORM, 12, 8, 1));
            Assert::IsTrue(DXGI_FORMAT_R8G8B8A8_UNORM == ff_dx12_format_fix(DXGI_FORMAT_BC1_UNORM, 12, 8, 4));
        }

        TEST_METHOD(fix_keeps_compressed_mips_when_power_of_two)
        {
            Assert::IsTrue(DXGI_FORMAT_BC3_UNORM == ff_dx12_format_fix(DXGI_FORMAT_BC3_UNORM, 64, 32, 4));
        }
    };

    TEST_CLASS(string_equal_tests)
    {
    public:
        TEST_METHOD(equal_and_not_equal)
        {
            Assert::IsTrue(ff_string_equal(FF_SVL("hello"), FF_SVL("hello")));
            Assert::IsFalse(ff_string_equal(FF_SVL("hello"), FF_SVL("world")));
            Assert::IsFalse(ff_string_equal(FF_SVL("hello"), FF_SVL("hell")));
            Assert::IsFalse(ff_string_equal(FF_SVL("hell"), FF_SVL("hello")));
        }

        TEST_METHOD(empty_strings_are_equal)
        {
            Assert::IsTrue(ff_string_equal(ff_string_view_empty(), ff_string_view_empty()));
            Assert::IsTrue(ff_string_equal(ff_string_view_empty(), FF_SVL("")));
            Assert::IsFalse(ff_string_equal(ff_string_view_empty(), FF_SVL("x")));
        }

        TEST_METHOD(null_data_with_zero_count_is_equal_to_empty)
        {
            // A zero-count view never dereferences data, so a null one must compare equal rather
            // than trip the assert.
            ff_string_view null_view;
            null_view.data = nullptr;
            null_view.count = 0;

            Assert::IsTrue(ff_string_equal(null_view, ff_string_view_empty()));
        }

        TEST_METHOD(embedded_nulls_are_compared)
        {
            // Views carry a count, so a null byte is ordinary data rather than a terminator.
            const char a[] = { 'a', '\0', 'b' };
            const char b[] = { 'a', '\0', 'c' };

            ff_string_view va;
            va.data = a;
            va.count = 3;

            ff_string_view vb;
            vb.data = b;
            vb.count = 3;

            Assert::IsFalse(ff_string_equal(va, vb));

            vb.data = a;
            Assert::IsTrue(ff_string_equal(va, vb));
        }

        TEST_METHOD(sz_view_round_trip)
        {
            Assert::IsTrue(ff_string_equal(ff_sz_view("hello"), FF_SVL("hello")));
        }

        TEST_METHOD(wide_equal_and_not_equal)
        {
            Assert::IsTrue(ff_wstring_equal(FF_WSVL(L"hello"), FF_WSVL(L"hello")));
            Assert::IsFalse(ff_wstring_equal(FF_WSVL(L"hello"), FF_WSVL(L"world")));
            Assert::IsFalse(ff_wstring_equal(FF_WSVL(L"hello"), FF_WSVL(L"hell")));
            Assert::IsTrue(ff_wstring_equal(ff_wstring_view_empty(), FF_WSVL(L"")));
        }

        TEST_METHOD(wide_compares_whole_characters)
        {
            // A byte-count bug would compare only half of each wide character and miss a
            // difference confined to the high bytes.
            const wchar_t a[] = { (wchar_t)0x0100, (wchar_t)0x0200 };
            const wchar_t b[] = { (wchar_t)0x0100, (wchar_t)0x0300 };

            ff_wstring_view va;
            va.data = a;
            va.count = 2;

            ff_wstring_view vb;
            vb.data = b;
            vb.count = 2;

            Assert::IsFalse(ff_wstring_equal(va, vb));
        }
    };
}
