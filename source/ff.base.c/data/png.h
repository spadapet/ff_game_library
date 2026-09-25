#pragma once

#include "../base/span.h"
#include "../base/string.h"

typedef struct ff_arena ff_arena;

// A decoded image. 'pixels' is tightly packed with no row padding, so the row pitch is always
// width * 4, and it lives in the arena passed to the decode call.
typedef struct ff_png_image
{
    uint32_t width;
    uint32_t height;

    // R8G8B8A8_UNORM, width * height * 4 bytes.
    uint8_t* pixels;

    // Set when the source was an indexed PNG whose palette was preserved rather than expanded.
    // 'indexes' is then width * height bytes of palette indexes, and 'palette' is exactly 256
    // RGBA entries with unused entries zeroed. 'pixels' is still filled in either way, so a caller
    // that does not care about palettes can ignore all three.
    bool has_palette;
    uint8_t* indexes;
    uint8_t palette[256 * 4];
    uint32_t palette_size;
} ff_png_image;

// Decodes a PNG into 'arena'. Everything allocated on success belongs to the arena, so there is no
// destroy call; the caller resets or destroys the arena instead. Returns false on any malformed or
// unsupported input, leaving '*image' zeroed.
//
// The decode always produces R8G8B8A8_UNORM pixels. When 'keep_palette' is true and the source is
// an indexed PNG, the palette and the raw indexes are also returned, which is what the palette
// renderer needs; the expanded RGBA pixels are produced either way.
bool ff_png_decode(ff_span png_bytes, ff_arena* arena, bool keep_palette, ff_png_image* image);
