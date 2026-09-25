#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "data/png.h"
#include <libpng/png.h>

// Bytes libpng is given to identify the format. png_sig_cmp only ever looks at the first 8.
static const size_t s_signature_size = 8;

// Rows that fit in the stack arena before the row pointer array spills to the heap. Covers any
// texture a 2D game is likely to load.
#define PNG_STACK_ROWS 2048

typedef struct png_read_state
{
    const uint8_t* data;
    size_t size;
    size_t read_pos;
    bool failed;
} png_read_state;

static void png_error_callback(png_structp png, png_const_charp text)
{
    png_read_state* state = (png_read_state*)png_get_error_ptr(png);

    if (state)
    {
        state->failed = true;
    }

    // libpng requires that the error callback does not return. Everything owned by the decode
    // lives in the caller's arena or in the png structs, so the jump target can clean up both.
    longjmp(png_jmpbuf(png), 1);
}

static void png_warning_callback(png_structp png, png_const_charp text)
{
}

static void png_read_callback(png_structp png, png_bytep out, size_t count)
{
    png_read_state* state = (png_read_state*)png_get_io_ptr(png);

    if (!state || state->read_pos + count > state->size)
    {
        png_error(png, "Read past the end of the PNG data");
        return;
    }

    memcpy(out, state->data + state->read_pos, count);
    state->read_pos += count;
}

static void expand_indexes_to_rgba(ff_png_image* image)
{
    const size_t pixel_count = (size_t)image->width * (size_t)image->height;

    for (size_t i = 0; i < pixel_count; i++)
    {
        const uint8_t index = image->indexes[i];
        memcpy(&image->pixels[i * 4], &image->palette[(size_t)index * 4], 4);
    }
}

static void read_palette(png_structp png, png_infop info, ff_png_image* image)
{
    png_colorp colors = NULL;
    int color_count = 0;
    FF_CHECK_RET(png_get_PLTE(png, info, &colors, &color_count) && colors && color_count > 0);

    png_bytep alphas = NULL;
    int alpha_count = 0;
    png_color_16p trans_color = NULL;
    const bool has_alphas = png_get_tRNS(png, info, &alphas, &alpha_count, &trans_color) != 0;

    if (color_count > 256)
    {
        color_count = 256;
    }

    image->palette_size = (uint32_t)color_count;

    for (int i = 0; i < color_count; i++)
    {
        image->palette[i * 4 + 0] = colors[i].red;
        image->palette[i * 4 + 1] = colors[i].green;
        image->palette[i * 4 + 2] = colors[i].blue;
        image->palette[i * 4 + 3] = (has_alphas && alphas && i < alpha_count) ? alphas[i] : 0xFF;
    }

    image->has_palette = true;
}

bool ff_png_decode(ff_span png_bytes, ff_arena* arena, bool keep_palette, ff_png_image* image)
{
    FF_ASSERT_RET_VAL(image && arena, false);

    *image = (ff_png_image){ 0 };

    FF_CHECK_RET_VAL(png_bytes.data && png_bytes.size > s_signature_size, false);
    FF_CHECK_RET_VAL(!png_sig_cmp((png_const_bytep)png_bytes.data, 0, s_signature_size), false);

    png_read_state state =
    {
        .data = (const uint8_t*)png_bytes.data,
        .size = png_bytes.size,
    };

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, &state,
        png_error_callback, png_warning_callback);
    FF_CHECK_RET_VAL(png, false);

    png_infop info = png_create_info_struct(png);

    if (!info)
    {
        png_destroy_read_struct(&png, NULL, NULL);
        return false;
    }

    // The row pointer array is a decode-time temporary, so it comes from a stack arena that only
    // spills to the heap for unusually tall images.
    ff_arena_declare_stack(row_arena, PNG_STACK_ROWS * sizeof(png_bytep));

    // libpng reports errors by longjmp'ing here. Every allocation made below this point is either
    // in the caller's arena or in row_arena, both of which are cleaned up on this path too.
    if (setjmp(png_jmpbuf(png)))
    {
        png_destroy_read_struct(&png, &info, NULL);
        ff_arena_destroy(&row_arena);
        *image = (ff_png_image){ 0 };
        return false;
    }

    png_set_read_fn(png, &state, png_read_callback);
    png_set_keep_unknown_chunks(png, PNG_HANDLE_CHUNK_NEVER, NULL, 0);
    png_read_info(png, info);

    png_uint_32 width = 0;
    png_uint_32 height = 0;
    int bit_depth = 0;
    int color_type = 0;
    int interlace = 0;

    if (!png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type, &interlace, NULL, NULL) ||
        !width || !height)
    {
        png_error(png, "Invalid PNG header");
    }

    // Interlaced PNGs need the whole multi-pass machinery for no benefit to a game's assets.
    if (interlace != PNG_INTERLACE_NONE)
    {
        png_error(png, "Interlaced PNG is not supported");
    }

    const bool indexed = (color_type == PNG_COLOR_TYPE_PALETTE);
    const bool as_indexes = indexed && keep_palette;

    if (as_indexes)
    {
        read_palette(png, info, image);

        // Indexes below 8 bits are packed several to a byte, and the renderer wants one byte per
        // pixel. This is the only transform allowed on this path: anything that changes the
        // channel count would destroy the indexes being preserved.
        if (bit_depth < 8)
        {
            png_set_packing(png);
        }
    }
    else
    {
        if (indexed)
        {
            png_set_palette_to_rgb(png);
        }

        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        {
            png_set_expand_gray_1_2_4_to_8(png);
        }

        if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        {
            png_set_gray_to_rgb(png);
        }

        if (bit_depth == 16)
        {
            png_set_strip_16(png);
        }

        if (png_get_valid(png, info, PNG_INFO_tRNS))
        {
            png_set_tRNS_to_alpha(png);
        }
        else if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_PALETTE)
        {
            png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);
        }
    }

    png_read_update_info(png, info);

    const size_t row_pitch = png_get_rowbytes(png, info);
    const size_t expected_pitch = as_indexes ? (size_t)width : (size_t)width * 4;

    if (row_pitch != expected_pitch)
    {
        png_error(png, "Unexpected PNG row size after transforms");
    }

    image->width = (uint32_t)width;
    image->height = (uint32_t)height;
    image->pixels = ff_arena_alloc_type(arena, uint8_t, (size_t)width * (size_t)height * 4);

    if (!image->pixels)
    {
        png_error(png, "Out of memory for PNG pixels");
    }

    if (as_indexes)
    {
        image->indexes = ff_arena_alloc_type(arena, uint8_t, (size_t)width * (size_t)height);

        if (!image->indexes)
        {
            png_error(png, "Out of memory for PNG indexes");
        }
    }

    png_bytep* rows = ff_arena_alloc_type(&row_arena, png_bytep, height);

    if (!rows)
    {
        png_error(png, "Out of memory for PNG rows");
    }

    uint8_t* const decode_target = as_indexes ? image->indexes : image->pixels;

    for (png_uint_32 i = 0; i < height; i++)
    {
        rows[i] = decode_target + (size_t)i * row_pitch;
    }

    png_read_image(png, rows);
    png_read_end(png, NULL);

    if (as_indexes)
    {
        expand_indexes_to_rgba(image);
    }

    png_destroy_read_struct(&png, &info, NULL);
    ff_arena_destroy(&row_arena);

    return true;
}
