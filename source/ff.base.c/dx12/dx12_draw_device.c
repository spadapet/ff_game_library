#include "pch.h"
#include "base/assert.h"
#include "base/math.h"
#include "dx12/dx12_commands.h"
#include "dx12/dx12_draw_device.h"
#include "dx12/dx12_format.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_mem_allocator.h"

typedef struct bucket_layout
{
    size_t item_size;
    size_t item_align;
} bucket_layout;

// Strides the GPU reads these as. A mismatch here would silently feed garbage to the vertex
// shader rather than fail, so each is pinned to the element list it has to agree with.
static_assert(sizeof(ff_dx12_sprite_instance) == 68, "sprite instance layout");
static_assert(sizeof(ff_dx12_line_instance) == 80, "line instance layout");
static_assert(sizeof(ff_dx12_triangle_instance) == 80, "triangle instance layout");
static_assert(sizeof(ff_dx12_rectangle_instance) == 44, "rectangle instance layout");
static_assert(sizeof(ff_dx12_circle_instance) == 56, "circle instance layout");

// Instance strides for the eight geometry kinds, matching the input layouts the draw state
// already bakes into its pipelines. The transparent half reuses the same layout.
static const bucket_layout s_bucket_layouts[ff_dx12_draw_bucket_count] =
{
    { .item_size = sizeof(ff_dx12_sprite_instance), .item_align = alignof(ff_dx12_sprite_instance) },
    { .item_size = sizeof(ff_dx12_sprite_instance), .item_align = alignof(ff_dx12_sprite_instance) },
    { .item_size = sizeof(ff_dx12_line_instance), .item_align = alignof(ff_dx12_line_instance) },
    { .item_size = sizeof(ff_dx12_triangle_instance), .item_align = alignof(ff_dx12_triangle_instance) },
    { .item_size = sizeof(ff_dx12_rectangle_instance), .item_align = alignof(ff_dx12_rectangle_instance) },
    { .item_size = sizeof(ff_dx12_rectangle_instance), .item_align = alignof(ff_dx12_rectangle_instance) },
    { .item_size = sizeof(ff_dx12_circle_instance), .item_align = alignof(ff_dx12_circle_instance) },
    { .item_size = sizeof(ff_dx12_circle_instance), .item_align = alignof(ff_dx12_circle_instance) },
};

static const uint16_t s_rectangle_indexes[] =
{
    0, 1, 2, 2, 1, 3,

    0, 1, 4, 4, 1, 5,
    1, 3, 5, 5, 3, 7,
    3, 2, 7, 7, 2, 6,
    2, 0, 6, 6, 0, 4,
};

static_assert(_countof(s_rectangle_indexes) ==
    FF_DX12_RECTANGLE_INDEX_COUNT + FF_DX12_RECTANGLE_OUTLINE_INDEX_COUNT,
    "rectangle index ranges must cover the whole table");

// The circle index patterns are regular, so they are generated rather than written out: a filled
// circle is a fan of triangles to the center vertex, and an outline is a quad per segment
// spanning the outer and inner rings.
static void build_static_indexes(uint16_t* indexes)
{
    memcpy(indexes, s_rectangle_indexes, sizeof(s_rectangle_indexes));

    uint16_t* filled = indexes + FF_DX12_CIRCLE_FILLED_INDEX_START;

    for (uint16_t i = 0; i < FF_DX12_CIRCLE_SEGMENTS; i++)
    {
        const uint16_t next = (uint16_t)((i + 1) % FF_DX12_CIRCLE_SEGMENTS);

        filled[i * 3 + 0] = i;
        filled[i * 3 + 1] = FF_DX12_CIRCLE_CENTER_VERTEX;
        filled[i * 3 + 2] = next;
    }

    uint16_t* outline = indexes + FF_DX12_CIRCLE_OUTLINE_INDEX_START;

    for (uint16_t i = 0; i < FF_DX12_CIRCLE_SEGMENTS; i++)
    {
        const uint16_t next = (uint16_t)((i + 1) % FF_DX12_CIRCLE_SEGMENTS);
        const uint16_t inner = (uint16_t)(i + FF_DX12_CIRCLE_SEGMENTS);
        const uint16_t inner_next = (uint16_t)(next + FF_DX12_CIRCLE_SEGMENTS);

        outline[i * 6 + 0] = i;
        outline[i * 6 + 1] = inner;
        outline[i * 6 + 2] = next;
        outline[i * 6 + 3] = next;
        outline[i * 6 + 4] = inner;
        outline[i * 6 + 5] = inner_next;
    }
}

static void build_circle_vertexes(ff_point_float* vertexes)
{
    for (size_t i = 0; i < FF_DX12_CIRCLE_SEGMENTS; i++)
    {
        const double angle = (double)i * 2.0 * 3.14159265358979323846 / FF_DX12_CIRCLE_SEGMENTS;
        const ff_point_float point = { .x = (float)cos(angle), .y = (float)sin(angle) };

        vertexes[i] = point;
        vertexes[i + FF_DX12_CIRCLE_SEGMENTS] = point;
    }

    vertexes[FF_DX12_CIRCLE_CENTER_VERTEX] = (ff_point_float){ .x = 0.0f, .y = 0.0f };
}

// The static geometry is uploaded through a command list, which init has no access to, so the
// upload is deferred to the first begin. It never changes afterward.
static bool init_static_geometry(ff_dx12_draw_device* device, ff_dx12_commands* commands)
{
    FF_CHECK_RET_VAL(!ff_dx12_buffer_valid(&device->index_buffer), true);

    uint16_t indexes[FF_DX12_STATIC_INDEX_COUNT];
    build_static_indexes(indexes);

    ff_point_float vertexes[FF_DX12_CIRCLE_VERTEX_COUNT];
    build_circle_vertexes(vertexes);

    FF_CHECK_RET_VAL(ff_dx12_buffer_init_gpu_static(&device->index_buffer,
        ff_dx12_buffer_type_index, commands, indexes, sizeof(indexes)), false);
    FF_CHECK_RET_VAL(ff_dx12_buffer_init_gpu_static(&device->circle_vertex_buffer,
        ff_dx12_buffer_type_vertex, commands, vertexes, sizeof(vertexes)), false);

    return true;
}

static void reset_batch(ff_dx12_draw_device* device)
{
    for (size_t i = 0; i < ff_dx12_instance_bucket_count; i++)
    {
        ff_dx12_instance_bucket_clear(&device->buckets[i]);
    }

    device->transparent_count = 0;
    device->matrix_count = 0;
    device->matrix_index = FF_DX12_INVALID_INDEX;
    device->last_depth_type = ff_dx12_last_depth_none;
}

bool ff_dx12_draw_device_init(ff_dx12_draw_device* device, ff_dx12_draw_state* draw_state)
{
    FF_ASSERT_RET_VAL(device, false);
    FF_ASSERT_RET_VAL(draw_state && ff_dx12_draw_state_valid(draw_state), false);

    *device = (ff_dx12_draw_device){ 0 };
    device->draw_state = draw_state;
    device->view_matrix = ff_matrix_identity();
    device->world_matrix = ff_matrix_identity();
    device->matrix_index = FF_DX12_INVALID_INDEX;

    ff_arena_init_heap_local(&device->arena, 0);

    for (size_t i = 0; i < ff_dx12_instance_bucket_count; i++)
    {
        const bucket_layout* layout = &s_bucket_layouts[i % ff_dx12_draw_bucket_count];
        ff_dx12_instance_bucket_init(&device->buckets[i], (ff_dx12_instance_bucket_type)i,
            layout->item_size, layout->item_align);
    }

    // The instance buffer is written every flush and read by the GPU in the same frame, so it is a
    // CPU-visible upload buffer rather than a GPU-local one that would need a copy.
    if (!ff_dx12_buffer_init_cpu(&device->instance_buffer, ff_dx12_buffer_type_vertex))
    {
        ff_dx12_draw_device_destroy(device);
        return false;
    }

    device->sampler_stack[0] = false;
    device->sampler_stack_count = 1;
    device->state = ff_dx12_draw_state_machine_valid;

    return true;
}

void ff_dx12_draw_device_destroy(ff_dx12_draw_device* device)
{
    FF_ASSERT_RET(device);

    ff_dx12_buffer_destroy(&device->instance_buffer);
    ff_dx12_buffer_destroy(&device->index_buffer);
    ff_dx12_buffer_destroy(&device->circle_vertex_buffer);

    for (size_t i = 0; i < ff_dx12_instance_bucket_count; i++)
    {
        ff_dx12_instance_bucket_destroy(&device->buckets[i]);
    }

    ff_arena_destroy(&device->arena);

    *device = (ff_dx12_draw_device){ 0 };
}

bool ff_dx12_draw_device_valid(const ff_dx12_draw_device* device)
{
    return device && device->state != ff_dx12_draw_state_machine_invalid;
}

bool ff_dx12_draw_device_begin(ff_dx12_draw_device* device, ff_dx12_commands* commands,
    ff_dx12_target_size target_size, DXGI_FORMAT target_format, ff_dx12_depth* depth,
    ff_rect_float view_rect, ff_rect_float world_rect, bool ignore_rotation)
{
    FF_ASSERT_RET_VAL(ff_dx12_draw_device_valid(device), false);
    FF_ASSERT_RET_VAL(commands, false);
    FF_ASSERT_RET_VAL(ff_dx12_draw_state_target_format_valid(target_format), false);

    ff_dx12_draw_device_end(device);

    ff_matrix view_matrix;
    FF_CHECK_RET_VAL(ff_dx12_view_matrix(target_size, view_rect, world_rect, ignore_rotation, &view_matrix), false);

    const ff_point_float view_size =
    {
        .x = (view_rect.right - view_rect.left) / (float)target_size.dpi_scale,
        .y = (view_rect.bottom - view_rect.top) / (float)target_size.dpi_scale,
    };

    FF_CHECK_RET_VAL(view_size.x != 0.0f && view_size.y != 0.0f, false);
    FF_CHECK_RET_VAL(init_static_geometry(device, commands), false);

    device->commands = commands;
    device->view_matrix = view_matrix;
    device->target_format = target_format;
    device->depth = (depth && ff_dx12_depth_valid(depth)) ? depth : NULL;
    device->target_requires_palette = ff_dx12_format_palette(target_format);
    device->vs_constants_0.view_scale.x = (world_rect.right - world_rect.left) / view_size.x;
    device->vs_constants_0.view_scale.y = (world_rect.bottom - world_rect.top) / view_size.y;
    device->world_matrix = ff_matrix_identity();
    device->draw_depth = 0.0f;
    device->force_no_overlap = 0;
    device->force_opaque = 0;
    device->force_pre_multiplied_alpha = 0;
    device->sampler_stack_count = 1;
    device->sampler_stack[0] = false;

    reset_batch(device);

    device->state = ff_dx12_draw_state_machine_drawing;

    return true;
}

void ff_dx12_draw_device_end(ff_dx12_draw_device* device)
{
    FF_ASSERT_RET(device);
    FF_CHECK_RET(device->state == ff_dx12_draw_state_machine_drawing);

    ff_dx12_draw_device_flush(device);

    device->commands = NULL;
    device->depth = NULL;
    device->state = ff_dx12_draw_state_machine_valid;
}

// Bucket offsets have to be an exact multiple of the bucket's stride, because the GPU addresses
// instances as base + index * stride. The strides here (68, 80, 44, 56) are not powers of two, so
// the bitmask-based ff_math_round_up cannot be used.
static size_t round_up_to_multiple(size_t value, size_t multiple)
{
    FF_ASSERT_RET_VAL(multiple, value);

    const size_t remainder = value % multiple;

    return remainder ? value + (multiple - remainder) : value;
}

// Packs every non-empty bucket into one upload-heap allocation and records where each landed. Each
// bucket's region is aligned to its own stride, since the GPU indexes instances by stride from the
// bound offset. Instance data is read once, by the draws issued immediately after, so it is bound
// straight from upload memory instead of being copied into a default-heap buffer first. The range
// is allocated against this submission's fence value, so the ring cannot recycle it while the GPU
// is still reading, and it must never be freed explicitly.
static bool build_instance_buffer(ff_dx12_draw_device* device)
{
    size_t byte_size = 0;

    for (size_t i = 0; i < ff_dx12_instance_bucket_count; i++)
    {
        ff_dx12_instance_bucket* bucket = &device->buckets[i];
        byte_size = round_up_to_multiple(byte_size, bucket->item_size);
        ff_dx12_instance_bucket_set_render_start(bucket, byte_size / bucket->item_size);
        byte_size += ff_dx12_instance_bucket_byte_size(bucket);
    }

    FF_CHECK_RET_VAL(byte_size, false);

    ff_dx12_mem_range range = ff_dx12_mem_allocator_ring_alloc_buffer(ff_dx12_upload_allocator(),
        byte_size, ff_dx12_commands_next_fence_value(device->commands));

    uint8_t* data = (uint8_t*)ff_dx12_mem_range_cpu_data(&range);
    FF_CHECK_RET_VAL(data, false);

    for (size_t i = 0; i < ff_dx12_instance_bucket_count; i++)
    {
        ff_dx12_instance_bucket* bucket = &device->buckets[i];

        if (bucket->render_count)
        {
            memcpy(data + bucket->render_start * bucket->item_size,
                ff_dx12_instance_bucket_data(bucket),
                ff_dx12_instance_bucket_byte_size(bucket));

            // Cleared here rather than in reset_batch so the copy and the clear cannot drift
            // apart; render_start/render_count keep the draw range alive.
            ff_dx12_instance_bucket_clear(bucket);
        }
    }

    device->instance_address = ff_dx12_mem_range_gpu_data(&range);
    device->instance_byte_size = byte_size;

    // Binding from upload memory skips ff_dx12_commands_update_buffer, which is where the copy
    // path would have registered the heap. The GPU reads this range during ExecuteCommandLists,
    // so the heap behind it has to be resident then.
    ff_dx12_commands_keep_resident(device->commands, ff_dx12_mem_range_residency_data(&range));

    return device->instance_address != 0;
}

// The per-flush model matrices go straight into upload memory and are bound as a root CBV from
// there. Staging them through a default-heap buffer would add a CPU copy and a GPU CopyBufferRegion
// for data the GPU reads exactly once, in the very draws that follow. The range is allocated
// against the fence the commands object will be submitted with, so the ring retires it only after
// the GPU is done reading; it must never be freed explicitly.
static D3D12_GPU_VIRTUAL_ADDRESS update_constants(ff_dx12_draw_device* device)
{
    device->vs_constants_0.projection = device->view_matrix;

    FF_CHECK_RET_VAL(device->matrix_count, 0);

    for (size_t i = 0; i < device->matrix_count; i++)
    {
        device->vs_constants_1.model[device->matrixes[i].index] = device->matrixes[i].matrix;
    }

    // Only the slots actually interned this flush are uploaded; the rest of the array is stale
    // from previous flushes and no instance references it. A root CBV address has to be 256-byte
    // aligned, which the ring allocator's buffer alignment already guarantees.
    const size_t size = sizeof(ff_matrix) * device->matrix_count;

    ff_dx12_mem_range range = ff_dx12_mem_allocator_ring_alloc_buffer(ff_dx12_upload_allocator(),
        size, ff_dx12_commands_next_fence_value(device->commands));

    void* dest = ff_dx12_mem_range_cpu_data(&range);
    FF_CHECK_RET_VAL(dest, 0);

    memcpy(dest, &device->vs_constants_1, size);

    ff_dx12_commands_keep_resident(device->commands, ff_dx12_mem_range_residency_data(&range));

    return ff_dx12_mem_range_gpu_data(&range);
}

typedef struct index_range
{
    size_t start;
    size_t count;
} index_range;

// Which slice of the shared index buffer each pipeline bucket draws. Sprites, filled rectangles
// and lines are all a single quad expanded by the vertex shader, so they share the rectangle
// range.
static index_range bucket_index_range(ff_dx12_draw_bucket bucket)
{
    switch (bucket)
    {
        case ff_dx12_draw_bucket_sprites:
        case ff_dx12_draw_bucket_palette_sprites:
        case ff_dx12_draw_bucket_lines:
        case ff_dx12_draw_bucket_rectangles_filled:
            return (index_range){ .start = FF_DX12_RECTANGLE_INDEX_START, .count = FF_DX12_RECTANGLE_INDEX_COUNT };

        case ff_dx12_draw_bucket_rectangles_outline:
            return (index_range){ .start = FF_DX12_RECTANGLE_OUTLINE_INDEX_START, .count = FF_DX12_RECTANGLE_OUTLINE_INDEX_COUNT };

        case ff_dx12_draw_bucket_triangles:
            return (index_range){ .start = FF_DX12_TRIANGLE_INDEX_START, .count = FF_DX12_TRIANGLE_INDEX_COUNT };

        case ff_dx12_draw_bucket_circles_filled:
            return (index_range){ .start = FF_DX12_CIRCLE_FILLED_INDEX_START, .count = FF_DX12_CIRCLE_FILLED_INDEX_COUNT };

        case ff_dx12_draw_bucket_circles_outline:
            return (index_range){ .start = FF_DX12_CIRCLE_OUTLINE_INDEX_START, .count = FF_DX12_CIRCLE_OUTLINE_INDEX_COUNT };
    }

    FF_DEBUG_FAIL_RET_VAL((index_range){ 0 });
}

static bool apply_bucket(ff_dx12_draw_device* device, const ff_dx12_instance_bucket* bucket)
{
    const ff_dx12_draw_bucket draw_bucket = ff_dx12_instance_bucket_draw_bucket(bucket);

    const ff_dx12_draw_state_flags flags = ff_dx12_draw_state_make_flags(
        device->target_format,
        device->depth != NULL,
        ff_dx12_instance_bucket_transparent(bucket),
        ff_dx12_draw_device_pre_multiplied_alpha(device));

    FF_CHECK_RET_VAL(ff_dx12_draw_state_apply(device->draw_state, device->commands,
        draw_bucket, flags), false);

    // The circle layouts declare a per-vertex element in slot 0 and instance data in slot 1;
    // every other bucket is instance-only in slot 0.
    const bool circle = draw_bucket == ff_dx12_draw_bucket_circles_filled ||
        draw_bucket == ff_dx12_draw_bucket_circles_outline;

    ff_dx12_resource* resources[2];
    D3D12_VERTEX_BUFFER_VIEW views[2];
    size_t count = 0;

    if (circle)
    {
        resources[count] = ff_dx12_buffer_resource(&device->circle_vertex_buffer);
        views[count] = (D3D12_VERTEX_BUFFER_VIEW)
        {
            .BufferLocation = ff_dx12_buffer_gpu_address(&device->circle_vertex_buffer),
            .SizeInBytes = (UINT)ff_dx12_buffer_size(&device->circle_vertex_buffer),
            .StrideInBytes = (UINT)sizeof(ff_point_float),
        };
        count++;
    }

    resources[count] = NULL;
    views[count] = (D3D12_VERTEX_BUFFER_VIEW)
    {
        .BufferLocation = device->instance_address,
        .SizeInBytes = (UINT)device->instance_byte_size,
        .StrideInBytes = (UINT)bucket->item_size,
    };
    count++;

    ff_dx12_commands_vertex_buffers(device->commands, resources, views, 0, count);

    return true;
}

static void draw_opaque(ff_dx12_draw_device* device)
{
    for (size_t i = 0; i < ff_dx12_instance_bucket_first_transparent; i++)
    {
        const ff_dx12_instance_bucket* bucket = &device->buckets[i];

        if (bucket->render_count && apply_bucket(device, bucket))
        {
            const index_range range = bucket_index_range(ff_dx12_instance_bucket_draw_bucket(bucket));

            ff_dx12_commands_draw_indexed(device->commands, 0, range.start, range.count,
                bucket->render_start, bucket->render_count);
        }
    }
}

static void draw_transparent(ff_dx12_draw_device* device)
{
    for (size_t i = 0; i < device->transparent_count; )
    {
        const ff_dx12_transparent_entry entry = device->transparent[i];
        const ff_dx12_instance_bucket* bucket = &device->buckets[entry.bucket_type];
        size_t instance_count = 1;

        // Entries were appended in issue order, and nudge_depth only ever moves depth forward, so
        // this list is already sorted. Adjacent entries that share a bucket and a depth cannot
        // overlap each other, so a contiguous run of them merges into one instanced call.
        for (i++; i < device->transparent_count; i++, instance_count++)
        {
            const ff_dx12_transparent_entry* next = &device->transparent[i];

            if (next->bucket_type != entry.bucket_type ||
                next->depth != entry.depth ||
                next->index != entry.index + instance_count)
            {
                break;
            }
        }

        if (apply_bucket(device, bucket))
        {
            const index_range range = bucket_index_range(ff_dx12_instance_bucket_draw_bucket(bucket));

            ff_dx12_commands_draw_indexed(device->commands, 0, range.start, range.count,
                bucket->render_start + entry.index, instance_count);
        }
    }
}

void ff_dx12_draw_device_flush(ff_dx12_draw_device* device)
{
    FF_ASSERT_RET(device);
    FF_CHECK_RET(device->state == ff_dx12_draw_state_machine_drawing);
    FF_CHECK_RET(device->last_depth_type != ff_dx12_last_depth_none);

    if (build_instance_buffer(device))
    {
        // The root signature and sampler table have to be set before any root argument, and
        // setting a root signature discards previously bound root arguments, so this must come
        // before the constants below rather than once per begin.
        FF_CHECK_RET(ff_dx12_draw_state_bind(device->draw_state, device->commands));

        const D3D12_GPU_VIRTUAL_ADDRESS vs_constants_1_address = update_constants(device);

        if (vs_constants_1_address)
        {
            ff_dx12_commands_root_cbv_address(device->commands, ff_dx12_root_param_vs_constants_1,
                vs_constants_1_address);
        }

        ff_dx12_commands_root_constants(device->commands, ff_dx12_root_param_vs_constants_0,
            &device->vs_constants_0, FF_DX12_VS_CONSTANTS_0_DWORD_COUNT * sizeof(uint32_t), 0);

        const D3D12_INDEX_BUFFER_VIEW index_view =
        {
            .BufferLocation = ff_dx12_buffer_gpu_address(&device->index_buffer),
            .SizeInBytes = (UINT)ff_dx12_buffer_size(&device->index_buffer),
            .Format = DXGI_FORMAT_R16_UINT,
        };

        ff_dx12_commands_index_buffer(device->commands,
            ff_dx12_buffer_resource(&device->index_buffer), &index_view);

        draw_opaque(device);
        draw_transparent(device);
    }

    reset_batch(device);
}

float ff_dx12_draw_device_nudge_depth(ff_dx12_draw_device* device)
{
    FF_ASSERT_RET_VAL(device, 0.0f);

    const ff_dx12_last_depth_type depth_type = device->force_no_overlap
        ? ff_dx12_last_depth_instance_no_overlap
        : ff_dx12_last_depth_instance;

    // A run of no-overlap draws deliberately shares one depth so the flush can merge them into a
    // single instanced call. Anything else has to advance so later geometry sorts in front.
    if (depth_type != ff_dx12_last_depth_instance_no_overlap || device->last_depth_type != depth_type)
    {
        device->draw_depth += FF_DX12_RENDER_DEPTH_DELTA;
    }

    device->last_depth_type = depth_type;

    return device->draw_depth;
}

void ff_dx12_draw_device_set_world_matrix(ff_dx12_draw_device* device, ff_matrix matrix)
{
    FF_ASSERT_RET(device);

    FF_CHECK_RET(!ff_matrix_equal(device->world_matrix, matrix));

    device->world_matrix = matrix;
    device->matrix_index = FF_DX12_INVALID_INDEX;
}

ff_matrix ff_dx12_draw_device_world_matrix(const ff_dx12_draw_device* device)
{
    FF_ASSERT_RET_VAL(device, ff_matrix_identity());

    return device->world_matrix;
}

static uint32_t matrix_index_no_flush(ff_dx12_draw_device* device)
{
    if (device->matrix_index == FF_DX12_INVALID_INDEX)
    {
        // The shader wants column-major, so the transpose happens once here at intern time rather
        // than per instance.
        const ff_matrix matrix = ff_matrix_transpose(device->world_matrix);

        for (size_t i = 0; i < device->matrix_count; i++)
        {
            if (ff_matrix_equal(device->matrixes[i].matrix, matrix))
            {
                device->matrix_index = device->matrixes[i].index;
                return device->matrix_index;
            }
        }

        if (device->matrix_count < FF_DX12_MAX_TRANSFORM_MATRIXES)
        {
            const uint32_t index = (uint32_t)device->matrix_count;
            device->matrixes[index].matrix = matrix;
            device->matrixes[index].index = index;
            device->matrix_count++;
            device->matrix_index = index;
        }
    }

    return device->matrix_index;
}

uint32_t ff_dx12_draw_device_matrix_index(ff_dx12_draw_device* device)
{
    FF_ASSERT_RET_VAL(device, FF_DX12_INVALID_INDEX);

    uint32_t index = matrix_index_no_flush(device);

    if (index == FF_DX12_INVALID_INDEX)
    {
        // The table is per-flush, so emptying it is what makes room.
        ff_dx12_draw_device_flush(device);
        index = matrix_index_no_flush(device);
    }

    return index;
}

void ff_dx12_draw_device_push_no_overlap(ff_dx12_draw_device* device)
{
    FF_ASSERT_RET(device);

    device->force_no_overlap++;
}

void ff_dx12_draw_device_pop_no_overlap(ff_dx12_draw_device* device)
{
    FF_ASSERT_RET(device);
    FF_ASSERT_RET(device->force_no_overlap > 0);

    device->force_no_overlap--;

    // Leaving the no-overlap region must not let the next draw share the depth of the last one
    // inside it, so the run is ended explicitly.
    if (!device->force_no_overlap && device->last_depth_type == ff_dx12_last_depth_instance_no_overlap)
    {
        device->last_depth_type = ff_dx12_last_depth_instance;
    }
}

void ff_dx12_draw_device_push_opaque(ff_dx12_draw_device* device)
{
    FF_ASSERT_RET(device);

    device->force_opaque++;
}

void ff_dx12_draw_device_pop_opaque(ff_dx12_draw_device* device)
{
    FF_ASSERT_RET(device);
    FF_ASSERT_RET(device->force_opaque > 0);

    device->force_opaque--;
}

void ff_dx12_draw_device_push_pre_multiplied_alpha(ff_dx12_draw_device* device)
{
    FF_ASSERT_RET(device);

    // Pre-multiplied alpha selects a different blend state, so whatever is batched under the
    // current one has to be issued before the setting changes.
    if (!device->force_pre_multiplied_alpha)
    {
        ff_dx12_draw_device_flush(device);
    }

    device->force_pre_multiplied_alpha++;
}

void ff_dx12_draw_device_pop_pre_multiplied_alpha(ff_dx12_draw_device* device)
{
    FF_ASSERT_RET(device);
    FF_ASSERT_RET(device->force_pre_multiplied_alpha > 0);

    if (device->force_pre_multiplied_alpha == 1)
    {
        ff_dx12_draw_device_flush(device);
    }

    device->force_pre_multiplied_alpha--;
}

void ff_dx12_draw_device_push_sampler_linear(ff_dx12_draw_device* device, bool linear)
{
    FF_ASSERT_RET(device);
    FF_ASSERT_RET(device->sampler_stack_count < FF_DX12_MAX_SAMPLER_STACK);

    device->sampler_stack[device->sampler_stack_count++] = linear;
}

void ff_dx12_draw_device_pop_sampler_linear(ff_dx12_draw_device* device)
{
    FF_ASSERT_RET(device);
    FF_ASSERT_RET(device->sampler_stack_count > 1);

    device->sampler_stack_count--;
}

bool ff_dx12_draw_device_linear_sampler(const ff_dx12_draw_device* device)
{
    FF_ASSERT_RET_VAL(device && device->sampler_stack_count, false);

    return device->sampler_stack[device->sampler_stack_count - 1];
}

bool ff_dx12_draw_device_pre_multiplied_alpha(const ff_dx12_draw_device* device)
{
    FF_ASSERT_RET_VAL(device, false);

    return device->force_pre_multiplied_alpha > 0;
}

bool ff_dx12_draw_device_allow_transparent(const ff_dx12_draw_device* device)
{
    FF_ASSERT_RET_VAL(device, false);

    // Blending into a palette target would blend index values rather than colors, so a palette
    // target is always treated as opaque.
    return !device->force_opaque && !device->target_requires_palette;
}

static bool transparent_reserve(ff_dx12_draw_device* device)
{
    if (device->transparent_count == device->transparent_capacity)
    {
        const size_t new_capacity = device->transparent_capacity
            ? device->transparent_capacity * 2
            : FF_DX12_MIN_INSTANCE_BUCKET_COUNT;

        ff_dx12_transparent_entry* entries = ff_arena_realloc_type(&device->arena,
            ff_dx12_transparent_entry, device->transparent,
            device->transparent_capacity, new_capacity);

        FF_CHECK_RET_VAL(entries, false);

        device->transparent = entries;
        device->transparent_capacity = new_capacity;
    }

    return true;
}

void* ff_dx12_draw_device_add_instance(ff_dx12_draw_device* device,
    ff_dx12_instance_bucket_type bucket_type, float depth)
{
    FF_ASSERT_RET_VAL(device, NULL);
    FF_ASSERT_RET_VAL(bucket_type < ff_dx12_instance_bucket_count, NULL);

    ff_dx12_instance_bucket* bucket = &device->buckets[bucket_type];

    if (ff_dx12_instance_bucket_transparent(bucket))
    {
        FF_ASSERT_RET_VAL(!device->force_opaque, NULL);
        FF_CHECK_RET_VAL(transparent_reserve(device), NULL);

        ff_dx12_transparent_entry* entry = &device->transparent[device->transparent_count++];
        entry->bucket_type = bucket_type;
        entry->index = bucket->count;
        entry->depth = depth;
    }

    return ff_dx12_instance_bucket_add(bucket);
}
