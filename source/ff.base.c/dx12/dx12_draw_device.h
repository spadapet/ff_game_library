#pragma once

#include "dx12_buffer.h"
#include "dx12_color.h"
#include "dx12_depth.h"
#include "dx12_draw_state.h"
#include "dx12_instance_bucket.h"
#include "dx12_texture_view.h"
#include "dx12_palette.h"

#define FF_DX12_MAX_PALETTE_STACK 16

// One interned palette row: which palette produced it, and the hash that identified it. The hash
// is kept separately from the palette because the palette may move to another row later.
typedef struct ff_dx12_palette_entry
{
    ff_dx12_palette palette;
    uint64_t hash;
} ff_dx12_palette_entry;

typedef struct ff_dx12_palette_remap_entry
{
    ff_dx12_palette_remap remap;
    uint64_t hash;
} ff_dx12_palette_remap_entry;


#define FF_DX12_MAX_RENDER_COUNT 0x80000
#define FF_DX12_MAX_RENDER_DEPTH 1.0f
#define FF_DX12_RENDER_DEPTH_DELTA (FF_DX12_MAX_RENDER_DEPTH / (float)FF_DX12_MAX_RENDER_COUNT)
#define FF_DX12_INVALID_INDEX 0xFFFFFFFF
#define FF_DX12_MAX_SAMPLER_STACK 32

// Whether the previous draw call consumed a depth slice. Successive no-overlap draws deliberately
// share one depth so they can be merged into a single instanced call; anything else advances.
typedef enum ff_dx12_last_depth_type
{
    ff_dx12_last_depth_none,
    ff_dx12_last_depth_instance,
    ff_dx12_last_depth_instance_no_overlap,
} ff_dx12_last_depth_type;

#define FF_DX12_CIRCLE_SEGMENTS 32

// Offsets into the one shared static index buffer. Every geometry kind draws from it, so a bucket
// only has to pick a range rather than bind a different buffer.
#define FF_DX12_TRIANGLE_INDEX_START 0
#define FF_DX12_TRIANGLE_INDEX_COUNT 3
#define FF_DX12_RECTANGLE_INDEX_START 0
#define FF_DX12_RECTANGLE_INDEX_COUNT 6
#define FF_DX12_RECTANGLE_OUTLINE_INDEX_START (FF_DX12_RECTANGLE_INDEX_START + FF_DX12_RECTANGLE_INDEX_COUNT)
#define FF_DX12_RECTANGLE_OUTLINE_INDEX_COUNT 24
#define FF_DX12_CIRCLE_FILLED_INDEX_START (FF_DX12_RECTANGLE_OUTLINE_INDEX_START + FF_DX12_RECTANGLE_OUTLINE_INDEX_COUNT)
#define FF_DX12_CIRCLE_FILLED_INDEX_COUNT (FF_DX12_CIRCLE_SEGMENTS * 3)
#define FF_DX12_CIRCLE_OUTLINE_INDEX_START (FF_DX12_CIRCLE_FILLED_INDEX_START + FF_DX12_CIRCLE_FILLED_INDEX_COUNT)
#define FF_DX12_CIRCLE_OUTLINE_INDEX_COUNT (FF_DX12_CIRCLE_SEGMENTS * 6)
#define FF_DX12_STATIC_INDEX_COUNT (FF_DX12_CIRCLE_OUTLINE_INDEX_START + FF_DX12_CIRCLE_OUTLINE_INDEX_COUNT)

// Two identical rings of unit-circle points plus a center point. The rings are duplicated rather
// than shared because an outline expands them apart by the instance thickness, which needs two
// distinct vertices at the same angle.
#define FF_DX12_CIRCLE_VERTEX_COUNT (FF_DX12_CIRCLE_SEGMENTS * 2 + 1)
#define FF_DX12_CIRCLE_CENTER_VERTEX (FF_DX12_CIRCLE_SEGMENTS * 2)

typedef enum ff_dx12_draw_state_machine
{
    ff_dx12_draw_state_machine_invalid,
    ff_dx12_draw_state_machine_valid,
    ff_dx12_draw_state_machine_drawing,
} ff_dx12_draw_state_machine;

// The five instance layouts. Field order and size are load-bearing: these are fed to the GPU as
// instance vertex buffers, so they must match the input element descs in dx12_draw_state.c
// element for element. dx12_draw_device.c static_asserts every stride.

typedef struct ff_dx12_sprite_instance
{
    ff_rect_float rect;
    ff_rect_float uv_rect;

    // For a palette-out target, R=1 means take the sampled color, and R<1 overrides the output
    // instead, which is how font glyphs get recolored.
    float color[4];

    // x, y, z = depth, w = rotation in radians counter-clockwise.
    float pos_rot[4];

    // matrix << 24, remap << 16, (palette or sampler) << 8, texture
    uint32_t indexes;
} ff_dx12_sprite_instance;

typedef struct ff_dx12_line_instance
{
    ff_point_float start;
    ff_point_float end;

    // The neighboring points, which the vertex shader needs to miter the joints. For an open
    // polyline the endpoints repeat themselves; for a closed one they wrap around.
    ff_point_float before_start;
    ff_point_float after_end;

    float start_color[4];
    float end_color[4];
    float start_thickness;
    float end_thickness;
    float depth;
    uint32_t matrix_index;
} ff_dx12_line_instance;

typedef struct ff_dx12_triangle_instance
{
    ff_point_float position[3];
    float color[3][4];
    float depth;
    uint32_t matrix_index;
} ff_dx12_triangle_instance;

typedef struct ff_dx12_rectangle_instance
{
    ff_rect_float rect;
    float color[4];
    float depth;

    // Zero means filled; non-zero is the outline width.
    float thickness;
    uint32_t matrix_index;
} ff_dx12_rectangle_instance;

typedef struct ff_dx12_circle_instance
{
    // x, y, z = depth, w = radius.
    float position_radius[4];
    float inside_color[4];
    float outside_color[4];
    float thickness;
    uint32_t matrix_index;
} ff_dx12_circle_instance;

// A sprite ready to draw: the quad to place in world space, the sub-rectangle of the texture to
// sample, and the view that sub-rectangle belongs to. This is the C port's stand-in for the old
// sprite_data type, carrying only what the instance layout actually needs.
//
// world is relative to the transform's position, so a sprite whose world rect is centered on the
// origin rotates about its own center.
typedef struct ff_dx12_sprite
{
    ff_dx12_texture_view* view;
    ff_rect_float world;
    ff_rect_float texture_uv;

    // Whether this sprite's own pixels include partial alpha, which forces it into the transparent
    // bucket even under an opaque tint. This is a property of the sub-rectangle, not of the
    // texture: two sprites on one sheet can differ. Fully transparent pixels don't count, since the
    // shader discards those without needing to blend.
    //
    // The old sprite_type enum also carried a palette bit, which has no counterpart here because
    // ff_dx12_draw_device_draw_palette_sprite is a separate entry point.
    bool transparent;
} ff_dx12_sprite;

// Position, scale, rotation and tint applied to a sprite at draw time. None of this becomes a
// matrix: the vertex shader reads it straight out of the instance, so a scene of differently
// placed sprites still shares one model matrix.
typedef struct ff_dx12_sprite_transform
{
    ff_point_float position;
    ff_point_float scale;
    float rotation_radians;
    ff_color color;
} ff_dx12_sprite_transform;

ff_dx12_sprite_transform ff_dx12_sprite_transform_default(void);

// One transparent instance, recorded in the order the caller issued it. Transparent geometry can't
// be drawn in bucket order like opaque geometry can, because overlapping translucent pixels only
// composite correctly back to front.
//
// No sort is needed: ff_dx12_draw_device_nudge_depth hands out strictly non-decreasing depths, so
// issue order is already depth order. The flush only has to coalesce runs.
typedef struct ff_dx12_transparent_entry
{
    ff_dx12_instance_bucket_type bucket_type;
    size_t index;
    float depth;
} ff_dx12_transparent_entry;

// One matrix that instances reference by index. Matrices are deduplicated within a flush so that a
// scene drawn under one transform costs one cbuffer slot rather than one per instance.
typedef struct ff_dx12_matrix_entry
{
    ff_matrix matrix;
    uint32_t index;
} ff_dx12_matrix_entry;

// The batching half of the draw device. Draw calls append instances into per-bucket storage
// instead of issuing GPU work, and a flush turns the whole accumulation into a handful of
// instanced draw calls.
//
// The counters are depth counters, not booleans, so the push/pop pairs nest. no_overlap and opaque
// only affect how later instances are bucketed, but pre_multiplied_alpha changes the pipeline, so
// its push and pop have to flush whatever was batched under the previous setting.
typedef struct ff_dx12_draw_device
{
    ff_dx12_draw_state_machine state;
    ff_dx12_draw_state* draw_state;
    ff_dx12_commands* commands;

    ff_arena arena;
    ff_dx12_instance_bucket buckets[ff_dx12_instance_bucket_count];

    ff_dx12_transparent_entry* transparent;
    size_t transparent_count;
    size_t transparent_capacity;

    // Static geometry shared by every bucket and never rewritten after init: the index ranges
    // above, and the unit-circle vertex ring the circle buckets expand per instance.
    ff_dx12_buffer index_buffer;
    ff_dx12_buffer circle_vertex_buffer;

    // Instance data for the current flush, living in upload memory owned by the ring allocator
    // rather than in a buffer owned here. Valid only between build_instance_buffer and the end of
    // the flush that produced it.
    D3D12_GPU_VIRTUAL_ADDRESS instance_address;
    size_t instance_byte_size;

    ff_dx12_vs_constants_0 vs_constants_0;
    ff_dx12_vs_constants_1 vs_constants_1;

    ff_matrix view_matrix;
    ff_matrix world_matrix;
    ff_dx12_matrix_entry matrixes[FF_DX12_MAX_TRANSFORM_MATRIXES];
    size_t matrix_count;
    uint32_t matrix_index;

    DXGI_FORMAT target_format;
    bool target_requires_palette;

    // Textures referenced by the sprites batched so far, in the order they were first seen. An
    // instance stores its slot here rather than a descriptor, so the whole table binds once per
    // flush. Filling it forces a flush, which is what empties it.
    ff_dx12_texture_view* textures[FF_DX12_MAX_TEXTURES];
    size_t texture_count;

    // The same table for palette sprites, whose textures hold indexes rather than colors and so
    // bind to a separate shader register range.
    ff_dx12_texture_view* palette_textures[FF_DX12_MAX_PALETTE_TEXTURES];
    size_t palette_texture_count;

    // Every palette row and remap row in use this flush, gathered into two shared textures so a
    // draw needs one binding rather than one per palette. Rows are interned by hash, so repeated
    // pushes of the same palette reuse a row, and the stored hash also suppresses the upload when
    // a row is already correct from a previous flush.
    ff_dx12_texture palette_texture;
    ff_dx12_texture palette_remap_texture;
    ff_dx12_texture_view palette_texture_view;
    ff_dx12_texture_view palette_remap_texture_view;

    ff_dx12_palette_entry palettes[FF_DX12_MAX_PALETTES];
    size_t palette_count;
    uint64_t palette_row_hashes[FF_DX12_MAX_PALETTES];

    ff_dx12_palette_remap_entry palette_remaps[FF_DX12_MAX_PALETTE_REMAPS];
    size_t palette_remap_count;
    uint64_t palette_remap_row_hashes[FF_DX12_MAX_PALETTE_REMAPS];

    // Cached results of interning the top of each stack, invalidated on push/pop and on flush.
    uint32_t palette_index;
    uint32_t palette_remap_index;

    ff_dx12_palette palette_stack[FF_DX12_MAX_PALETTE_STACK];
    size_t palette_stack_count;

    ff_dx12_palette_remap palette_remap_stack[FF_DX12_MAX_PALETTE_STACK];
    size_t palette_remap_stack_count;

    ff_dx12_ps_constants_0 ps_constants_0;

    // Set by begin when drawing into a target that has a depth buffer. Selects the depth-enabled
    // pipeline variant, and is what makes the per-instance depth slices actually resolve overlap
    // on the GPU rather than relying purely on draw order.
    ff_dx12_depth* depth;

    float draw_depth;
    ff_dx12_last_depth_type last_depth_type;
    int force_no_overlap;
    int force_opaque;
    int force_pre_multiplied_alpha;

    bool sampler_stack[FF_DX12_MAX_SAMPLER_STACK];
    size_t sampler_stack_count;

    ff_dx12_device_child device_child;
} ff_dx12_draw_device;

bool ff_dx12_draw_device_init(ff_dx12_draw_device* device, ff_dx12_draw_state* draw_state);
void ff_dx12_draw_device_destroy(ff_dx12_draw_device* device);
bool ff_dx12_draw_device_valid(const ff_dx12_draw_device* device);

// Enters the drawing state. Fails without entering it if the view and world rects don't produce a
// usable projection, so a failed begin leaves the device safely reusable.
//
// target and target_view are the render target to draw into. They are bound here rather than left
// to the caller because a draw with no bound target, viewport, or scissor is silently clipped away
// rather than reported as an error.
bool ff_dx12_draw_device_begin(ff_dx12_draw_device* device, ff_dx12_commands* commands,
    ff_dx12_resource* target, D3D12_CPU_DESCRIPTOR_HANDLE target_view,
    ff_dx12_target_size target_size, DXGI_FORMAT target_format, ff_dx12_depth* depth,
    ff_rect_float view_rect, ff_rect_float world_rect, bool ignore_rotation);

// Flushes anything still batched and leaves the drawing state. Safe to call when not drawing.
void ff_dx12_draw_device_end(ff_dx12_draw_device* device);

// Issues everything batched so far as instanced draw calls, then clears the batch. Called
// automatically when a per-flush table fills up or when the pipeline has to change.
void ff_dx12_draw_device_flush(ff_dx12_draw_device* device);

// Reserves the next depth slice. Successive calls under push_no_overlap return the same depth so
// the instances can merge into one draw call.
float ff_dx12_draw_device_nudge_depth(ff_dx12_draw_device* device);

// Interns the current world matrix and returns its cbuffer slot, flushing first if the table is
// full. FF_DX12_INVALID_INDEX only if the flush failed to make room.
uint32_t ff_dx12_draw_device_matrix_index(ff_dx12_draw_device* device);

void ff_dx12_draw_device_set_world_matrix(ff_dx12_draw_device* device, ff_matrix matrix);
ff_matrix ff_dx12_draw_device_world_matrix(const ff_dx12_draw_device* device);

void ff_dx12_draw_device_push_no_overlap(ff_dx12_draw_device* device);
void ff_dx12_draw_device_pop_no_overlap(ff_dx12_draw_device* device);
void ff_dx12_draw_device_push_opaque(ff_dx12_draw_device* device);
void ff_dx12_draw_device_pop_opaque(ff_dx12_draw_device* device);
void ff_dx12_draw_device_push_pre_multiplied_alpha(ff_dx12_draw_device* device);
void ff_dx12_draw_device_pop_pre_multiplied_alpha(ff_dx12_draw_device* device);
void ff_dx12_draw_device_push_sampler_linear(ff_dx12_draw_device* device, bool linear);
void ff_dx12_draw_device_pop_sampler_linear(ff_dx12_draw_device* device);

bool ff_dx12_draw_device_linear_sampler(const ff_dx12_draw_device* device);
bool ff_dx12_draw_device_pre_multiplied_alpha(const ff_dx12_draw_device* device);

// False when the target is a palette or push_opaque is in effect, in which case transparent
// geometry is bucketed as opaque instead.
bool ff_dx12_draw_device_allow_transparent(const ff_dx12_draw_device* device);

// Appends one instance to a bucket, recording it in the transparent list when the bucket is a
// transparent one. Returns uninitialized storage for the caller to fill.
void* ff_dx12_draw_device_add_instance(ff_dx12_draw_device* device,
    ff_dx12_instance_bucket_type bucket_type, float depth);

// One point of a geometry draw. The meaning of size depends on the call: line thickness for
// draw_lines, radius for draw_circle, and unused for draw_triangles.
//
// Unlike the C++ original there is no "inherit the previous point's color" sentinel, because that
// needed a nullable color pointer. Every endpoint carries its own color.
typedef struct ff_dx12_draw_endpoint
{
    ff_point_float pos;
    ff_color color;
    float size;
} ff_dx12_draw_endpoint;

// A polyline through the given points. Needs at least two. A run whose first and last points match
// is treated as closed, which wraps the miter neighbors around instead of repeating the endpoints.
// Degenerate and fully transparent segments are skipped individually rather than failing the call.
void ff_dx12_draw_device_draw_lines(ff_dx12_draw_device* device,
    const ff_dx12_draw_endpoint* points, size_t count);

// Independent triangles, three points each. A trailing partial triangle is ignored.
void ff_dx12_draw_device_draw_triangles(ff_dx12_draw_device* device,
    const ff_dx12_draw_endpoint* points, size_t count);

// Filled when thickness is zero, outlined otherwise. A negative thickness draws the outline
// outside the rect rather than inside it. An outline too thick to fit becomes a fill.
void ff_dx12_draw_device_draw_rectangle(ff_dx12_draw_device* device,
    ff_rect_float rect, ff_color color, float thickness);

// Filled when thickness is zero, outlined otherwise. A negative thickness grows the outline
// outward from the radius. outside_color produces a gradient from the endpoint's own color.
void ff_dx12_draw_device_draw_circle(ff_dx12_draw_device* device,
    ff_dx12_draw_endpoint pos, float thickness, ff_color outside_color);

// One textured sprite. The sprite's world rect is scaled by the transform's scale, then rotated
// about the transform's position, all in the vertex shader. The transform color multiplies the
// sampled texel, so ff_color_white draws the texture unmodified.
//
// The texture is interned into a per-flush table; a full table forces a flush, so any number of
// distinct textures can be drawn between one begin and end.
void ff_dx12_draw_device_draw_sprite(ff_dx12_draw_device* device,
    const ff_dx12_sprite* sprite, const ff_dx12_sprite_transform* transform);

// Draws a sprite whose texture holds palette indexes rather than colors. The active palette and
// palette remap come from the stacks below, so the same sprite can be recolored without touching
// its texture.
void ff_dx12_draw_device_draw_palette_sprite(ff_dx12_draw_device* device,
    const ff_dx12_sprite* sprite, const ff_dx12_sprite_transform* transform);

// The palette used by palette sprites drawn until the matching pop. Pushing the same palette that
// is already active is cheap: it interns to the row already in the shared texture.
void ff_dx12_draw_device_push_palette(ff_dx12_draw_device* device, ff_dx12_palette palette);
void ff_dx12_draw_device_pop_palette(ff_dx12_draw_device* device);

// The index -> index remap applied before the palette lookup. A NULL or empty remap is the
// identity.
void ff_dx12_draw_device_push_palette_remap(ff_dx12_draw_device* device, const ff_dx12_palette_remap* remap);
void ff_dx12_draw_device_pop_palette_remap(ff_dx12_draw_device* device);
