#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/string.h"

ff::string_view ff::sz_view(const char* sz)
{
    return ff::string_view{ sz, sz ? ::strlen(sz) : 0 };
}

ff::wstring_view ff::sz_view(const char16_t* sz)
{
    // char16_t and wchar_t are both 16-bit on Windows, so wcslen measures the UTF-16 unit count.
    return ff::wstring_view{ sz, sz ? ::wcslen((const wchar_t*)sz) : 0 };
}

ff::wstring_view ff::utf8_to_wide(ff::string_view utf8, ff::arena* arena)
{
    ff::wstring_view result{ nullptr, 0 };
    if (!utf8.size)
    {
        return result;
    }

    // Source length is passed explicitly so no null terminator is read or written, and the
    // returned count is the exact UTF-16 length. CP_UTF8 with no flags is the fast, lenient path.
    FF_ASSERT_RET_VAL(utf8.size <= (size_t)INT_MAX, result);
    int source_len = (int)utf8.size;

    int wide_len = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data, source_len, nullptr, 0);
    FF_ASSERT_RET_VAL(wide_len > 0, result);

    char16_t* dest = (char16_t*)arena->alloc((size_t)wide_len * sizeof(char16_t), alignof(char16_t));
    FF_ASSERT_RET_VAL(dest, result);

    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data, source_len, (wchar_t*)dest, wide_len);

    result.data = dest;
    result.size = (size_t)wide_len;
    return result;
}

ff::string_view ff::wide_to_utf8(ff::wstring_view wide, ff::arena* arena)
{
    ff::string_view result{ nullptr, 0 };
    if (!wide.size)
    {
        return result;
    }

    // Source length is passed explicitly so no null terminator is read or written, and the
    // returned count is the exact UTF-8 byte length. CP_UTF8 with no flags is the fast path.
    FF_ASSERT_RET_VAL(wide.size <= (size_t)INT_MAX, result);
    int source_len = (int)wide.size;

    int utf8_len = ::WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)wide.data, source_len, nullptr, 0, nullptr, nullptr);
    FF_ASSERT_RET_VAL(utf8_len > 0, result);

    char* dest = (char*)arena->alloc((size_t)utf8_len, alignof(char));
    FF_ASSERT_RET_VAL(dest, result);

    ::WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)wide.data, source_len, dest, utf8_len, nullptr, nullptr);

    result.data = dest;
    result.size = (size_t)utf8_len;
    return result;
}
