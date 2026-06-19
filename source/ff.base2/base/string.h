#pragma once

#define FF_SVL(literal) (ff::string_view{ (literal), sizeof(literal) - 1 })
#define FF_WSVL(literal) (ff::wstring_view{ (literal), sizeof(literal) / sizeof(wchar_t) - 1 })

// FF_SV_FORMAT is for use in format strings, like: printf("%.*s", FF_SV_FORMAT(sv));
#define FF_SV_FORMAT(sv) ((int)(sv).count), ((sv).data)

namespace ff
{
    struct arena;

    struct string_view
    {
        const char* data;
        size_t count;
    };

    struct wstring_view
    {
        const wchar_t* data;
        size_t count;
    };

    ff::string_view sz_view(const char* sz);
    ff::wstring_view sz_view(const wchar_t* sz);

    // Convert between UTF-8 and UTF-16, allocating the result from 'arena'. The returned view's 'size'
    // excludes the terminator, but 'data' is always null-terminated so it can be passed to C-string APIs.
    ff::wstring_view utf8_to_wide(ff::string_view utf8, ff::arena* arena);
    ff::string_view wide_to_utf8(ff::wstring_view wide, ff::arena* arena);
}
