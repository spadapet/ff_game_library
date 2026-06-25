#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/string.h"

ff::string_view ff::sz_view(const char* sz)
{
    ff::string_view result;
    result.data = sz;
    result.count = sz ? ::strlen(sz) : 0;
    return result;
}

ff::wstring_view ff::sz_view(const wchar_t* sz)
{
    ff::wstring_view result;
    result.data = sz;
    result.count = sz ? ::wcslen(sz) : 0;
    return result;
}

ff::wstring_view ff::utf8_to_wide(ff::string_view utf8, ff::arena* arena)
{
    // Every return path is contracted to be null-terminated, so the default/failure result is an
    // empty (but still null-terminated) static string rather than a null pointer.
    ff::wstring_view result;
    result.data = L"";
    result.count = 0;
    FF_CHECK_RET_VAL(utf8.count, result);

    // Source length is passed explicitly so the input need not be null-terminated and the returned
    // count is the exact UTF-16 length. CP_UTF8 with no flags is the fast, lenient path.
    FF_ASSERT_RET_VAL(utf8.count <= (size_t)INT_MAX, result);
    int source_len = (int)utf8.count;

    int wide_len = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data, source_len, nullptr, 0);
    FF_ASSERT_RET_VAL(wide_len > 0, result);

    // One extra unit holds a null terminator so 'data' is a valid C string; 'count' still excludes it.
    wchar_t* dest = (wchar_t*)arena->alloc(((size_t)wide_len + 1) * sizeof(wchar_t), alignof(wchar_t));
    FF_ASSERT_RET_VAL(dest, result);

    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data, source_len, dest, wide_len);
    dest[wide_len] = 0;

    result.data = dest;
    result.count = (size_t)wide_len;
    return result;
}

ff::string_view ff::wide_to_utf8(ff::wstring_view wide, ff::arena* arena)
{
    // Every return path is contracted to be null-terminated, so the default/failure result is an
    // empty (but still null-terminated) static string rather than a null pointer.
    ff::string_view result;
    result.data = "";
    result.count = 0;
    FF_CHECK_RET_VAL(wide.count, result);

    // Source length is passed explicitly so the input need not be null-terminated and the returned
    // count is the exact UTF-8 byte length. CP_UTF8 with no flags is the fast path.
    FF_ASSERT_RET_VAL(wide.count <= (size_t)INT_MAX, result);
    int source_len = (int)wide.count;

    int utf8_len = ::WideCharToMultiByte(CP_UTF8, 0, wide.data, source_len, nullptr, 0, nullptr, nullptr);
    FF_ASSERT_RET_VAL(utf8_len > 0, result);

    // One extra byte holds a null terminator so 'data' is a valid C string; 'count' still excludes it.
    char* dest = (char*)arena->alloc((size_t)utf8_len + 1, alignof(char));
    FF_ASSERT_RET_VAL(dest, result);

    ::WideCharToMultiByte(CP_UTF8, 0, wide.data, source_len, dest, utf8_len, nullptr, nullptr);
    dest[utf8_len] = 0;

    result.data = dest;
    result.count = (size_t)utf8_len;
    return result;
}
