#include "pch.h"
#include "base/assert.h"
#include "base/math.h"
#include "base/string.h"
#include "dx12/dx12_format.h"

typedef struct format_info
{
    DXGI_FORMAT format;
    uint16_t bits_per_pixel;
    uint8_t block_width;
    uint8_t block_height;
    bool compressed;
    bool color;
    bool has_alpha;
    bool render_target;
} format_info;

static const format_info s_formats[] =
{
    { .format = DXGI_FORMAT_R32G32B32A32_TYPELESS, .bits_per_pixel = 128 },
    { .format = DXGI_FORMAT_R32G32B32A32_UINT, .bits_per_pixel = 128, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_R32G32B32A32_SINT, .bits_per_pixel = 128, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_R32G32B32_TYPELESS, .bits_per_pixel = 96 },
    { .format = DXGI_FORMAT_R32G32B32_FLOAT, .bits_per_pixel = 96, .color = true },
    { .format = DXGI_FORMAT_R32G32B32_UINT, .bits_per_pixel = 96 },
    { .format = DXGI_FORMAT_R32G32B32_SINT, .bits_per_pixel = 96 },
    { .format = DXGI_FORMAT_R16G16B16A16_TYPELESS, .bits_per_pixel = 64 },
    { .format = DXGI_FORMAT_R16G16B16A16_FLOAT, .bits_per_pixel = 64, .color = true, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_R16G16B16A16_UNORM, .bits_per_pixel = 64, .color = true, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_R16G16B16A16_UINT, .bits_per_pixel = 64, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_R16G16B16A16_SNORM, .bits_per_pixel = 64, .color = true, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_R16G16B16A16_SINT, .bits_per_pixel = 64, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_R32G32_TYPELESS, .bits_per_pixel = 64 },
    { .format = DXGI_FORMAT_R32G32_UINT, .bits_per_pixel = 64, .render_target = true },
    { .format = DXGI_FORMAT_R32G32_SINT, .bits_per_pixel = 64, .render_target = true },
    { .format = DXGI_FORMAT_R10G10B10A2_TYPELESS, .bits_per_pixel = 32 },
    { .format = DXGI_FORMAT_R10G10B10A2_UNORM, .bits_per_pixel = 32, .color = true, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_R10G10B10A2_UINT, .bits_per_pixel = 32, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM, .bits_per_pixel = 32, .color = true, .has_alpha = true },
    { .format = DXGI_FORMAT_R11G11B10_FLOAT, .bits_per_pixel = 32, .color = true, .render_target = true },
    { .format = DXGI_FORMAT_R8G8B8A8_TYPELESS, .bits_per_pixel = 32 },
    { .format = DXGI_FORMAT_R8G8B8A8_UINT, .bits_per_pixel = 32, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_R8G8B8A8_SNORM, .bits_per_pixel = 32, .color = true, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_R8G8B8A8_SINT, .bits_per_pixel = 32, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_R16G16_TYPELESS, .bits_per_pixel = 32 },
    { .format = DXGI_FORMAT_R16G16_FLOAT, .bits_per_pixel = 32, .color = true, .render_target = true },
    { .format = DXGI_FORMAT_R16G16_UNORM, .bits_per_pixel = 32, .color = true, .render_target = true },
    { .format = DXGI_FORMAT_R16G16_UINT, .bits_per_pixel = 32, .render_target = true },
    { .format = DXGI_FORMAT_R16G16_SNORM, .bits_per_pixel = 32, .color = true, .render_target = true },
    { .format = DXGI_FORMAT_R16G16_SINT, .bits_per_pixel = 32, .render_target = true },
    { .format = DXGI_FORMAT_R32_TYPELESS, .bits_per_pixel = 32 },
    { .format = DXGI_FORMAT_R32_SINT, .bits_per_pixel = 32, .render_target = true },
    { .format = DXGI_FORMAT_R8G8_TYPELESS, .bits_per_pixel = 16 },
    { .format = DXGI_FORMAT_R8G8_UNORM, .bits_per_pixel = 16, .color = true, .render_target = true },
    { .format = DXGI_FORMAT_R8G8_UINT, .bits_per_pixel = 16, .render_target = true },
    { .format = DXGI_FORMAT_R8G8_SNORM, .bits_per_pixel = 16, .color = true, .render_target = true },
    { .format = DXGI_FORMAT_R8G8_SINT, .bits_per_pixel = 16, .render_target = true },
    { .format = DXGI_FORMAT_R16_TYPELESS, .bits_per_pixel = 16 },
    { .format = DXGI_FORMAT_R16_FLOAT, .bits_per_pixel = 16, .color = true, .render_target = true },
    { .format = DXGI_FORMAT_R16_UNORM, .bits_per_pixel = 16, .color = true, .render_target = true },
    { .format = DXGI_FORMAT_R16_SNORM, .bits_per_pixel = 16, .color = true, .render_target = true },
    { .format = DXGI_FORMAT_R16_SINT, .bits_per_pixel = 16, .render_target = true },
    { .format = DXGI_FORMAT_R8_TYPELESS, .bits_per_pixel = 8 },
    { .format = DXGI_FORMAT_R8_SNORM, .bits_per_pixel = 8, .color = true, .render_target = true },
    { .format = DXGI_FORMAT_R8_SINT, .bits_per_pixel = 8, .render_target = true },
    { .format = DXGI_FORMAT_R9G9B9E5_SHAREDEXP, .bits_per_pixel = 32, .color = true },
    { .format = DXGI_FORMAT_B5G6R5_UNORM, .bits_per_pixel = 16, .color = true, .render_target = true },
    { .format = DXGI_FORMAT_B5G5R5A1_UNORM, .bits_per_pixel = 16, .color = true, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_B4G4R4A4_UNORM, .bits_per_pixel = 16, .color = true, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_B8G8R8A8_TYPELESS, .bits_per_pixel = 32 },
    { .format = DXGI_FORMAT_B8G8R8X8_TYPELESS, .bits_per_pixel = 32 },
    { .format = DXGI_FORMAT_B8G8R8X8_UNORM_SRGB, .bits_per_pixel = 32, .color = true, .render_target = true },
    { .format = DXGI_FORMAT_R8G8B8A8_UNORM, .bits_per_pixel = 32, .color = true, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, .bits_per_pixel = 32, .color = true, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_B8G8R8A8_UNORM, .bits_per_pixel = 32, .color = true, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, .bits_per_pixel = 32, .color = true, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_B8G8R8X8_UNORM, .bits_per_pixel = 32, .color = true, .render_target = true },
    { .format = DXGI_FORMAT_R32G32B32A32_FLOAT, .bits_per_pixel = 128, .color = true, .has_alpha = true, .render_target = true },
    { .format = DXGI_FORMAT_R32G32_FLOAT, .bits_per_pixel = 64, .color = true, .render_target = true },
    { .format = DXGI_FORMAT_R32_FLOAT, .bits_per_pixel = 32, .color = true, .render_target = true },
    { .format = DXGI_FORMAT_R8_UNORM, .bits_per_pixel = 8, .color = true, .render_target = true },
    { .format = DXGI_FORMAT_A8_UNORM, .bits_per_pixel = 8, .has_alpha = true },
    { .format = DXGI_FORMAT_R1_UNORM, .bits_per_pixel = 1, .color = true },
    { .format = DXGI_FORMAT_R8_UINT, .bits_per_pixel = 8, .render_target = true },
    { .format = DXGI_FORMAT_R16_UINT, .bits_per_pixel = 16, .render_target = true },
    { .format = DXGI_FORMAT_R32_UINT, .bits_per_pixel = 32, .render_target = true },
    { .format = DXGI_FORMAT_D16_UNORM, .bits_per_pixel = 16 },
    { .format = DXGI_FORMAT_D24_UNORM_S8_UINT, .bits_per_pixel = 32 },
    { .format = DXGI_FORMAT_D32_FLOAT, .bits_per_pixel = 32 },
    { .format = DXGI_FORMAT_BC1_TYPELESS, .bits_per_pixel = 4, .block_width = 4, .block_height = 4, .compressed = true },
    { .format = DXGI_FORMAT_BC2_TYPELESS, .bits_per_pixel = 8, .block_width = 4, .block_height = 4, .compressed = true },
    { .format = DXGI_FORMAT_BC3_TYPELESS, .bits_per_pixel = 8, .block_width = 4, .block_height = 4, .compressed = true },
    { .format = DXGI_FORMAT_BC1_UNORM, .bits_per_pixel = 4, .block_width = 4, .block_height = 4, .compressed = true, .color = true },
    { .format = DXGI_FORMAT_BC1_UNORM_SRGB, .bits_per_pixel = 4, .block_width = 4, .block_height = 4, .compressed = true, .color = true },
    { .format = DXGI_FORMAT_BC2_UNORM, .bits_per_pixel = 8, .block_width = 4, .block_height = 4, .compressed = true, .color = true, .has_alpha = true },
    { .format = DXGI_FORMAT_BC2_UNORM_SRGB, .bits_per_pixel = 8, .block_width = 4, .block_height = 4, .compressed = true, .color = true, .has_alpha = true },
    { .format = DXGI_FORMAT_BC3_UNORM, .bits_per_pixel = 8, .block_width = 4, .block_height = 4, .compressed = true, .color = true, .has_alpha = true },
    { .format = DXGI_FORMAT_BC3_UNORM_SRGB, .bits_per_pixel = 8, .block_width = 4, .block_height = 4, .compressed = true, .color = true, .has_alpha = true },
    { .format = DXGI_FORMAT_BC4_TYPELESS, .bits_per_pixel = 4, .block_width = 4, .block_height = 4, .compressed = true },
    { .format = DXGI_FORMAT_BC4_UNORM, .bits_per_pixel = 4, .block_width = 4, .block_height = 4, .compressed = true, .color = true },
    { .format = DXGI_FORMAT_BC4_SNORM, .bits_per_pixel = 4, .block_width = 4, .block_height = 4, .compressed = true, .color = true },
    { .format = DXGI_FORMAT_BC5_TYPELESS, .bits_per_pixel = 8, .block_width = 4, .block_height = 4, .compressed = true },
    { .format = DXGI_FORMAT_BC5_UNORM, .bits_per_pixel = 8, .block_width = 4, .block_height = 4, .compressed = true, .color = true },
    { .format = DXGI_FORMAT_BC5_SNORM, .bits_per_pixel = 8, .block_width = 4, .block_height = 4, .compressed = true, .color = true },
    { .format = DXGI_FORMAT_BC6H_TYPELESS, .bits_per_pixel = 8, .block_width = 4, .block_height = 4, .compressed = true },
    { .format = DXGI_FORMAT_BC6H_UF16, .bits_per_pixel = 8, .block_width = 4, .block_height = 4, .compressed = true, .color = true },
    { .format = DXGI_FORMAT_BC6H_SF16, .bits_per_pixel = 8, .block_width = 4, .block_height = 4, .compressed = true, .color = true },
    { .format = DXGI_FORMAT_BC7_TYPELESS, .bits_per_pixel = 8, .block_width = 4, .block_height = 4, .compressed = true },
    { .format = DXGI_FORMAT_BC7_UNORM, .bits_per_pixel = 8, .block_width = 4, .block_height = 4, .compressed = true, .color = true, .has_alpha = true },
    { .format = DXGI_FORMAT_BC7_UNORM_SRGB, .bits_per_pixel = 8, .block_width = 4, .block_height = 4, .compressed = true, .color = true, .has_alpha = true },
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

bool ff_dx12_format_render_target(DXGI_FORMAT format)
{
    const format_info* info = find_format(format);
    return info && info->render_target;
}

size_t ff_dx12_format_bits_per_pixel(DXGI_FORMAT format)
{
    const format_info* info = find_format(format);
    return info ? info->bits_per_pixel : 0;
}

size_t ff_dx12_format_block_width(DXGI_FORMAT format)
{
    const format_info* info = find_format(format);
    return info ? (info->block_width ? (size_t)info->block_width : 1) : 0;
}

size_t ff_dx12_format_block_height(DXGI_FORMAT format)
{
    const format_info* info = find_format(format);
    return info ? (info->block_height ? (size_t)info->block_height : 1) : 0;
}

size_t ff_dx12_format_row_count(DXGI_FORMAT format, size_t height)
{
    const size_t block_height = ff_dx12_format_block_height(format);
    return block_height ? height / block_height + (height % block_height != 0) : 0;
}

size_t ff_dx12_format_row_pitch(DXGI_FORMAT format, size_t width)
{
    const size_t bits_per_pixel = ff_dx12_format_bits_per_pixel(format);
    const size_t block_width = ff_dx12_format_block_width(format);
    const size_t block_height = ff_dx12_format_block_height(format);

    if (!bits_per_pixel || !block_width || !block_height)
    {
        return 0;
    }

    const size_t block_count = width / block_width + (width % block_width != 0);
    const size_t bits_per_block = bits_per_pixel * block_width * block_height;
    if (bits_per_block < 8)
    {
        const size_t blocks_per_byte = 8 / bits_per_block;
        return block_count / blocks_per_byte + (block_count % blocks_per_byte != 0);
    }

    const size_t bytes_per_block = bits_per_block / 8;
    FF_CHECK_RET_VAL(block_count <= SIZE_MAX / bytes_per_block, 0);
    return block_count * bytes_per_block;
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
