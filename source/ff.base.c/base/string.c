#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/string.h"

ff_string_view ff_string_view_empty(void)
{
    return (ff_string_view)
    {
        .data = "",
        .count = 0,
    };
}

ff_wstring_view ff_wstring_view_empty(void)
{
    return (ff_wstring_view)
    {
        .data = L"",
        .count = 0,
    };
}

ff_span ff_string_view_span(ff_string_view str)
{
    return (ff_span) { .data = str.data, .size = str.count };
}

ff_span ff_wstring_view_span(ff_wstring_view str)
{
    return (ff_span) { .data = str.data, .size = str.count * sizeof(wchar_t) };
}

ff_string_view ff_string_copy(ff_string_view str, ff_arena* arena)
{
    if (str.count)
    {
        char* copy = ff_arena_alloc_type(arena, char, str.count);
        memcpy(copy, str.data, str.count);
        str.data = copy;
    }

    return str;
}

ff_wstring_view ff_wstring_copy(ff_wstring_view str, ff_arena* arena)
{
    if (str.count)
    {
        wchar_t* copy = ff_arena_alloc_type(arena, wchar_t, str.count);
        memcpy(copy, str.data, str.count * sizeof(wchar_t));
        str.data = copy;
    }

    return str;
}

ff_string_view ff_sz_view(const char* sz)
{
    return (ff_string_view)
    {
        .data = sz,
        .count = sz ? strlen(sz) : 0
    };
}

ff_wstring_view ff_wz_view(const wchar_t* sz)
{
    return (ff_wstring_view)
    {
        .data = sz,
        .count = sz ? wcslen(sz) : 0
    };
}

ff_wstring_view ff_utf8_to_wide(ff_string_view utf8, ff_arena* arena, bool null_terminating)
{
    FF_CHECK_RET_VAL(utf8.count, ff_wstring_view_empty());

    int source_len = (int)utf8.count;
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, utf8.data, source_len, NULL, 0);
    FF_ASSERT_RET_VAL(wide_len > 0, ff_wstring_view_empty());

    wchar_t* dest = ff_arena_alloc_type(arena, wchar_t, (size_t)wide_len + (null_terminating ? 1 : 0));
    MultiByteToWideChar(CP_UTF8, 0, utf8.data, source_len, dest, wide_len);

    if (null_terminating)
    {
        dest[wide_len] = 0;
    }

    return (ff_wstring_view){ .data = dest, .count = (size_t)wide_len };
}

ff_string_view ff_wide_to_utf8(ff_wstring_view wide, ff_arena* arena, bool null_terminating)
{
    FF_CHECK_RET_VAL(wide.count, ff_string_view_empty());
    int source_len = (int)wide.count;
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide.data, source_len, NULL, 0, NULL, NULL);
    FF_ASSERT_RET_VAL(utf8_len > 0, ff_string_view_empty());

    char* dest = ff_arena_alloc_type(arena, char, (size_t)utf8_len + (null_terminating ? 1 : 0));
    WideCharToMultiByte(CP_UTF8, 0, wide.data, source_len, dest, utf8_len, NULL, NULL);

    if (null_terminating)
    {
        dest[utf8_len] = 0;
    }

    return (ff_string_view){ .data = dest, .count = (size_t)utf8_len };
}
