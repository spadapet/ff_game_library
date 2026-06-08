#pragma once

#include "../base/string_view.h"

namespace ff
{
    struct arena;

    ff::string_view sz_view(const char* sz);
    ff::wstring_view sz_view(const char16_t* sz);

    ff::wstring_view utf8_to_wide(ff::string_view utf8, ff::arena* arena);
    ff::string_view wide_to_utf8(ff::wstring_view wide, ff::arena* arena);
}
