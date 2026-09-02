#pragma once

#define FF_SVL(literal) ((ff_string_view){ .data = (literal), .count = sizeof(literal) - 1 })
#define FF_WSVL(literal) ((ff_wstring_view){ .data = (literal), .count = sizeof(literal) / sizeof((literal)[0]) - 1 })

// FF_SV_FORMAT is for use in format strings, like: printf("%.*s", FF_SV_FORMAT(sv));
#define FF_SV_FORMAT(sv) ((int)(sv).count), ((sv).data)

typedef struct ff_arena ff_arena;

typedef struct ff_string_view
{
    const char* data;
    size_t count;
} ff_string_view;

typedef struct ff_wstring_view
{
    const wchar_t* data;
    size_t count;
} ff_wstring_view;

ff_string_view ff_sz_view(const char* sz);
ff_wstring_view ff_wz_view(const wchar_t* sz);

// Convert between UTF-8 and UTF-16, allocating the result from 'arena'. The returned view's 'size'
// excludes the terminator, but 'data' is always null-terminated so it can be passed to C-string APIs.
ff_wstring_view ff_utf8_to_wide(ff_string_view utf8, ff_arena* arena);
ff_string_view ff_wide_to_utf8(ff_wstring_view wide, ff_arena* arena);
