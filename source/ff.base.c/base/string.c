#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/string.h"

ff_string_view ff_sz_view(const char* sz)
{
	return (ff_string_view){ .data = sz, .count = sz ? strlen(sz) : 0 };
}

ff_wstring_view ff_wsz_view(const wchar_t* sz)
{
	return (ff_wstring_view){ .data = sz, .count = sz ? wcslen(sz) : 0 };
}

ff_wstring_view ff_utf8_to_wide(ff_string_view utf8, ff_arena* arena)
{
    ff_wstring_view result = { .data = L"", .count = 0 };
    FF_CHECK_RET_VAL(utf8.count, result);

    int source_len = (int)utf8.count;
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, utf8.data, source_len, nullptr, 0);
    FF_ASSERT_RET_VAL(wide_len > 0, result);

    wchar_t* dest = (wchar_t*)arena->alloc(((size_t)wide_len + 1) * sizeof(wchar_t), alignof(wchar_t));
    FF_ASSERT_RET_VAL(dest, result);

    MultiByteToWideChar(CP_UTF8, 0, utf8.data, source_len, dest, wide_len);
    dest[wide_len] = 0;

    result = (ff_wstring_view){ .data = dest, .count = (size_t)wide_len };
    return result;
}

ff_string_view ff_wide_to_utf8(ff_wstring_view wide, ff_arena* arena)
{
    ff_string_view result = { .data = "", .count = 0 };
    FF_CHECK_RET_VAL(wide.count, result);

    int source_len = (int)wide.count;
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide.data, source_len, nullptr, 0, nullptr, nullptr);
    FF_ASSERT_RET_VAL(utf8_len > 0, result);

    char* dest = (char*)arena->alloc((size_t)utf8_len + 1, alignof(char));
    FF_ASSERT_RET_VAL(dest, result);

    WideCharToMultiByte(CP_UTF8, 0, wide.data, source_len, dest, utf8_len, nullptr, nullptr);
    dest[utf8_len] = 0;

    result = (ff_string_view){ .data = dest, .count = (size_t)utf8_len };
    return result;
}
