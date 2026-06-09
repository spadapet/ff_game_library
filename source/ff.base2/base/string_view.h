#pragma once

#define FF_SVL(literal) (ff::string_view{ (literal), sizeof(literal) - 1 })
#define FF_WSVL(literal) (ff::wstring_view{ (literal), sizeof(literal) / sizeof(wchar_t) - 1 })

// FF_SV_FORMAT is for use in format strings, like: printf("%.*s", FF_SV_FORMAT(sv));
#define FF_SV_FORMAT(sv) ((int)(sv).size), ((sv).data)

namespace ff
{
    struct string_view
    {
        const char* data;
        size_t size;
    };

    struct wstring_view
    {
        const wchar_t* data;
        size_t size;
    };
}
