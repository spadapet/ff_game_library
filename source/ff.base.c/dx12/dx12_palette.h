#pragma once

#include "../base/span.h"
#include "dx12_device_child.h"
#include "dx12_texture.h"

#define FF_PALETTE_SIZE 256

// A palette is one or more 256-entry rows of RGBA colors. Multiple rows let a palette animate by
// cycling which row is current without re-uploading anything.
//
// Rows are hashed once at init. The draw device uploads a row only when its hash differs from what
// is already in that slot of the shared palette texture, so a palette that never changes costs one
// copy for the lifetime of the device.
typedef struct ff_dx12_palette_data
{
    ff_dx12_texture texture;
    uint64_t* row_hashes;
    size_t row_count;

    // The colors are kept so a device reset can re-upload them: ff_dx12_texture rebuilds the
    // resource and its view but never restores pixels, leaving every palette black otherwise.
    uint32_t* colors;

    ff_dx12_device_child device_child;
    ff_arena arena;
} ff_dx12_palette_data;

// colors is row_count * FF_PALETTE_SIZE RGBA values, laid out one full row after another. The
// pixels are copied, so the caller's buffer may go away immediately.
bool ff_dx12_palette_data_init(ff_dx12_palette_data* data, ff_dx12_commands* commands,
    const uint32_t* colors, size_t row_count);
void ff_dx12_palette_data_destroy(ff_dx12_palette_data* data);

bool ff_dx12_palette_data_valid(const ff_dx12_palette_data* data);
size_t ff_dx12_palette_data_row_count(const ff_dx12_palette_data* data);
uint64_t ff_dx12_palette_data_row_hash(const ff_dx12_palette_data* data, size_t row);

// A palette is a cursor onto one row of some palette data. It is a value, not a resource: several
// palettes may share the same data and sit on different rows.
typedef struct ff_dx12_palette
{
    ff_dx12_palette_data* data;
    size_t current_row;
} ff_dx12_palette;

ff_dx12_palette ff_dx12_palette_make(ff_dx12_palette_data* data, size_t current_row);
uint64_t ff_dx12_palette_row_hash(const ff_dx12_palette* palette);

// A remap is a 256-byte index -> index table applied after the texture lookup and before the
// palette lookup. The hash identifies it, so two identical remaps intern to one texture row.
typedef struct ff_dx12_palette_remap
{
    uint8_t remap[FF_PALETTE_SIZE];
    uint64_t hash;
} ff_dx12_palette_remap;

// The identity remap, which maps every index to itself.
ff_dx12_palette_remap ff_dx12_palette_remap_identity(void);

// Hashes the bytes and fills in the hash field, so callers never compute it themselves.
ff_dx12_palette_remap ff_dx12_palette_remap_make(ff_span remap);

// Device reset: re-uploads the saved colors into the rebuilt texture. There is no before_reset
// because the texture and its resource handle their own teardown.
bool internal_ff_dx12_palette_data_reset(ff_dx12_palette_data* data, ff_dx12_commands* commands);
