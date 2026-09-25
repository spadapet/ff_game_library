#pragma once

#include "../base/string.h"

bool ff_dx12_format_compressed(DXGI_FORMAT format);
bool ff_dx12_format_color(DXGI_FORMAT format);
bool ff_dx12_format_palette(DXGI_FORMAT format);
bool ff_dx12_format_has_alpha(DXGI_FORMAT format);
bool ff_dx12_format_supports_pre_multiplied_alpha(DXGI_FORMAT format);

size_t ff_dx12_format_bits_per_pixel(DXGI_FORMAT format);

// Returns DXGI_FORMAT_UNKNOWN for an unrecognized name rather than asserting, so callers can
// report a bad asset instead of crashing.
DXGI_FORMAT ff_dx12_format_parse(ff_string_view name);

// Falls back to R8G8B8A8_UNORM when the format is unknown, or when a compressed format can't
// satisfy its size restrictions: BC formats need multiple-of-four dimensions, and mip chains
// additionally need powers of two.
DXGI_FORMAT ff_dx12_format_fix(DXGI_FORMAT format, size_t texture_width, size_t texture_height, size_t mip_count);
