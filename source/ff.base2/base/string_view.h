#pragma once

#define FF_SVL(literal) (ff::string_view{ (literal), sizeof(literal) - 1 })
#define FF_WSVL(literal) (ff::wstring_view{ (literal), sizeof(literal) / sizeof(char16_t) - 1 })

namespace ff
{
    struct string_view
    {
        const char* data;
        size_t size;
    };

    struct wstring_view
    {
        const char16_t* data;
        size_t size;
    };
}
