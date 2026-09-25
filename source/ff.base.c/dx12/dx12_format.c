#include "pch.h"
#include "base/assert.h"
#include "base/math.h"
#include "base/string.h"
#include "dx12/dx12_format.h"

typedef struct format_info
{
    DXGI_FORMAT format;
    uint16_t bits_per_pixel;
    bool compressed;
    bool color;
    bool has_alpha;
} format_info;

// The legacy code answered these through DirectXTex. This table covers every format the engine
// actually uses and keeps the library free of that dependency.
static const format_info s_formats[] =
{
    { .format = DXGI_FORMAT_R8G8B8A8_UNORM, .bits_per_pixel = 32, .color = true, .has_alpha = true },
    { .format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, .bits_per_pixel = 32, .color = true, .has_alpha = true },
    { .format = DXGI_FORMAT_B8G8R8A8_UNORM, .bits_per_pixel = 32, .color = true, .has_alpha = true },
    { .format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, .bits_per_pixel = 32, .color = true, .has_alpha = true },
    { .format = DXGI_FORMAT_B8G8R8X8_UNORM, .bits_per_pixel = 32, .color = true },
    { .format = DXGI_FORMAT_R32G32B32A32_FLOAT, .bits_per_pixel = 128, .color = true, .has_alpha = true },
    { .format = DXGI_FORMAT_R32G32_FLOAT, .bits_per_pixel = 64, .color = true },
    { .format = DXGI_FORMAT_R32_FLOAT, .bits_per_pixel = 32, .color = true },
    { .format = DXGI_FORMAT_R8_UNORM, .bits_per_pixel = 8, .color = true },
    { .format = DXGI_FORMAT_A8_UNORM, .bits_per_pixel = 8, .has_alpha = true },
    { .format = DXGI_FORMAT_R1_UNORM, .bits_per_pixel = 1, .color = true },
    { .format = DXGI_FORMAT_R8_UINT, .bits_per_pixel = 8 },
    { .format = DXGI_FORMAT_R16_UINT, .bits_per_pixel = 16 },
    { .format = DXGI_FORMAT_R32_UINT, .bits_per_pixel = 32 },
    { .format = DXGI_FORMAT_D24_UNORM_S8_UINT, .bits_per_pixel = 32 },
    { .format = DXGI_FORMAT_D32_FLOAT, .bits_per_pixel = 32 },
    { .format = DXGI_FORMAT_BC1_UNORM, .bits_per_pixel = 4, .compressed = true, .color = true },
    { .format = DXGI_FORMAT_BC1_UNORM_SRGB, .bits_per_pixel = 4, .compressed = true, .color = true },
    { .format = DXGI_FORMAT_BC2_UNORM, .bits_per_pixel = 8, .compressed = true, .color = true, .has_alpha = true },
    { .format = DXGI_FORMAT_BC2_UNORM_SRGB, .bits_per_pixel = 8, .compressed = true, .color = true, .has_alpha = true },
    { .format = DXGI_FORMAT_BC3_UNORM, .bits_per_pixel = 8, .compressed = true, .color = true, .has_alpha = true },
    { .format = DXGI_FORMAT_BC3_UNORM_SRGB, .bits_per_pixel = 8, .compressed = true, .color = true, .has_alpha = true },
};

typedef struct format_name
{
    ff_string_view name;
    DXGI_FORMAT format;
} format_name;

static const format_name s_format_names[] =
{
    { .name = FF_SVL_INIT("rgba32"), .format = DXGI_FORMAT_R8G8B8A8_UNORM },
    { .name = FF_SVL_INIT("bgra32"), .format = DXGI_FORMAT_B8G8R8A8_UNORM },
    { .name = FF_SVL_INIT("bc1"), .format = DXGI_FORMAT_BC1_UNORM },
    { .name = FF_SVL_INIT("bc2"), .format = DXGI_FORMAT_BC2_UNORM },
    { .name = FF_SVL_INIT("bc3"), .format = DXGI_FORMAT_BC3_UNORM },
    { .name = FF_SVL_INIT("pal"), .format = DXGI_FORMAT_R8_UINT },
    { .name = FF_SVL_INIT("palette"), .format = DXGI_FORMAT_R8_UINT },
    { .name = FF_SVL_INIT("gray"), .format = DXGI_FORMAT_R8_UNORM },
    { .name = FF_SVL_INIT("bw"), .format = DXGI_FORMAT_R1_UNORM },
    { .name = FF_SVL_INIT("alpha"), .format = DXGI_FORMAT_A8_UNORM },
};

static const format_info* find_format(DXGI_FORMAT format)
{
    for (size_t i = 0; i < _countof(s_formats); i++)
    {
        if (s_formats[i].format == format)
        {
            return &s_formats[i];
        }
    }

    return NULL;
}

bool ff_dx12_format_compressed(DXGI_FORMAT format)
{
    const format_info* info = find_format(format);
    return info && info->compressed;
}

bool ff_dx12_format_color(DXGI_FORMAT format)
{
    const format_info* info = find_format(format);
    return info && info->color;
}

bool ff_dx12_format_palette(DXGI_FORMAT format)
{
    return format == DXGI_FORMAT_R8_UINT;
}

bool ff_dx12_format_has_alpha(DXGI_FORMAT format)
{
    const format_info* info = find_format(format);
    return info && info->has_alpha;
}

bool ff_dx12_format_supports_pre_multiplied_alpha(DXGI_FORMAT format)
{
    const format_info* info = find_format(format);
    return info && !info->compressed && info->color && info->has_alpha;
}

size_t ff_dx12_format_bits_per_pixel(DXGI_FORMAT format)
{
    const format_info* info = find_format(format);
    return info ? info->bits_per_pixel : 0;
}

DXGI_FORMAT ff_dx12_format_parse(ff_string_view name)
{
    for (size_t i = 0; i < _countof(s_format_names); i++)
    {
        if (ff_string_equal(name, s_format_names[i].name))
        {
            return s_format_names[i].format;
        }
    }

    return DXGI_FORMAT_UNKNOWN;
}

DXGI_FORMAT ff_dx12_format_fix(DXGI_FORMAT format, size_t texture_width, size_t texture_height, size_t mip_count)
{
    if (format == DXGI_FORMAT_UNKNOWN)
    {
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    if (ff_dx12_format_compressed(format))
    {
        if ((texture_width % 4) || (texture_height % 4))
        {
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        if (mip_count > 1 && (!ff_math_is_pow2(texture_width) || !ff_math_is_pow2(texture_height)))
        {
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        }
    }

    return format;
}
