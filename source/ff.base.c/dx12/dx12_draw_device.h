#pragma once

#include "dx12_buffer.h"
#include "dx12_depth.h"
#include "dx12_draw_state.h"
#include "dx12_instance_bucket.h"

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

    // x, y, z = depth, w = rotation in degrees counter-clockwise.
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

    ff_dx12_buffer instance_buffer;

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
} ff_dx12_draw_device;

bool ff_dx12_draw_device_init(ff_dx12_draw_device* device, ff_dx12_draw_state* draw_state);
void ff_dx12_draw_device_destroy(ff_dx12_draw_device* device);
bool ff_dx12_draw_device_valid(const ff_dx12_draw_device* device);

// Enters the drawing state. Fails without entering it if the view and world rects don't produce a
// usable projection, so a failed begin leaves the device safely reusable.
bool ff_dx12_draw_device_begin(ff_dx12_draw_device* device, ff_dx12_commands* commands,
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
