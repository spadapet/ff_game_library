#pragma once

#include "../base/arena.h"
#include "dx12_draw_state.h"

// One bucket per (geometry kind, transparency) pair. The opaque half mirrors ff_dx12_draw_bucket
// one-for-one, and the transparent half repeats it in the same order, so a transparent bucket
// maps to its pipeline by subtracting ff_dx12_instance_bucket_first_transparent.
//
// Opaque and transparent are separate buckets rather than a flag on the instance because they are
// drawn by completely different strategies: the opaque half is drawn in bucket order with one
// instanced call per bucket, while the transparent half has to be drawn back to front in the
// order the caller issued it.
typedef enum ff_dx12_instance_bucket_type
{
    ff_dx12_instance_bucket_sprites,
    ff_dx12_instance_bucket_palette_sprites,
    ff_dx12_instance_bucket_lines,
    ff_dx12_instance_bucket_triangles,
    ff_dx12_instance_bucket_rectangles_filled,
    ff_dx12_instance_bucket_rectangles_outline,
    ff_dx12_instance_bucket_circles_filled,
    ff_dx12_instance_bucket_circles_outline,

    ff_dx12_instance_bucket_sprites_transparent,
    ff_dx12_instance_bucket_palette_sprites_transparent,
    ff_dx12_instance_bucket_lines_transparent,
    ff_dx12_instance_bucket_triangles_transparent,
    ff_dx12_instance_bucket_rectangles_filled_transparent,
    ff_dx12_instance_bucket_rectangles_outline_transparent,
    ff_dx12_instance_bucket_circles_filled_transparent,
    ff_dx12_instance_bucket_circles_outline_transparent,

    ff_dx12_instance_bucket_count,
    ff_dx12_instance_bucket_first_transparent = ff_dx12_instance_bucket_sprites_transparent,
} ff_dx12_instance_bucket_type;

#define FF_DX12_MIN_INSTANCE_BUCKET_COUNT 64

// Type-erased storage for one bucket's instances. The draw device holds all sixteen and appends to
// whichever one a draw call lands in, so the element type varies per bucket while the growth and
// bookkeeping are shared.
//
// Instances are plain bytes with no destructors, so clearing is just a rewind of the write cursor:
// the allocation is deliberately kept across frames, since bucket sizes are stable from one frame
// to the next and reusing the memory keeps a steady-state frame free of allocation.
typedef struct ff_dx12_instance_bucket
{
    ff_arena arena;
    ff_dx12_instance_bucket_type bucket_type;
    size_t item_size;
    size_t item_align;
    uint8_t* data;
    size_t count;
    size_t capacity;

    // Where this bucket's instances landed in the combined instance buffer, and how many were
    // there when the flush snapshotted it. Held separately from count because the draw happens
    // after the buckets have been copied out and cleared.
    size_t render_start;
    size_t render_count;
} ff_dx12_instance_bucket;

void ff_dx12_instance_bucket_init(ff_dx12_instance_bucket* bucket,
    ff_dx12_instance_bucket_type bucket_type, size_t item_size, size_t item_align);
void ff_dx12_instance_bucket_destroy(ff_dx12_instance_bucket* bucket);

// Returns uninitialized storage for one instance; the caller fills every field. NULL only if the
// arena cannot grow.
void* ff_dx12_instance_bucket_add(ff_dx12_instance_bucket* bucket);

// Rewinds the write cursor and keeps the allocation, so the next frame reuses the same memory.
void ff_dx12_instance_bucket_clear(ff_dx12_instance_bucket* bucket);

bool ff_dx12_instance_bucket_transparent(const ff_dx12_instance_bucket* bucket);
size_t ff_dx12_instance_bucket_byte_size(const ff_dx12_instance_bucket* bucket);
const void* ff_dx12_instance_bucket_data(const ff_dx12_instance_bucket* bucket);

// Snapshots the current count as the range to draw, starting at the given offset in the combined
// instance buffer.
void ff_dx12_instance_bucket_set_render_start(ff_dx12_instance_bucket* bucket, size_t start);

// The pipeline bucket this maps to. Opaque and transparent halves share a pipeline bucket, since
// blending is chosen by the draw state flags rather than by the geometry.
ff_dx12_draw_bucket ff_dx12_instance_bucket_draw_bucket(const ff_dx12_instance_bucket* bucket);
