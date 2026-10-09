#include "pch.h"
#include "base/assert.h"
#include "base/math.h"
#include "dx12/dx12_commands.h"
#include "dx12/dx12_descriptor_allocator.h"
#include "dx12/dx12_draw_device.h"
#include "dx12/dx12_format.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_mem_allocator.h"
#include "dx12/dx12_texture.h"

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
    device->texture_count = 0;
    device->palette_texture_count = 0;
    device->palette_count = 0;
    device->palette_remap_count = 0;
    device->palette_index = FF_DX12_INVALID_INDEX;
    device->palette_remap_index = FF_DX12_INVALID_INDEX;
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

    device->sampler_stack[0] = false;
    device->sampler_stack_count = 1;

    device->palette_stack[0] = ff_dx12_palette_make(NULL, 0);
    device->palette_stack_count = 1;
    device->palette_remap_stack[0] = ff_dx12_palette_remap_identity();
    device->palette_remap_stack_count = 1;
    device->palette_index = FF_DX12_INVALID_INDEX;
    device->palette_remap_index = FF_DX12_INVALID_INDEX;

    // The shared palette rows are RGBA colors; the remap rows are raw indexes, so R8_UINT keeps the
    // shader's Load returning the index unscaled rather than normalizing it to 0..1.
    ff_dx12_texture_params palette_params = ff_dx12_texture_params_default(FF_PALETTE_SIZE, FF_DX12_MAX_PALETTES);
    ff_dx12_texture_params remap_params = ff_dx12_texture_params_default(FF_PALETTE_SIZE, FF_DX12_MAX_PALETTE_REMAPS);
    remap_params.format = DXGI_FORMAT_R8_UINT;

    if (!ff_dx12_texture_init(&device->palette_texture, &palette_params) ||
        !ff_dx12_texture_init(&device->palette_remap_texture, &remap_params) ||
        !ff_dx12_texture_view_init(&device->palette_texture_view, &device->palette_texture, 0, 1, 0, 1) ||
        !ff_dx12_texture_view_init(&device->palette_remap_texture_view, &device->palette_remap_texture, 0, 1, 0, 1))
    {
        ff_dx12_draw_device_destroy(device);
        return false;
    }

    device->state = ff_dx12_draw_state_machine_valid;
    ff_dx12_add_device_child(&device->device_child, device, ff_dx12_device_child_type_draw_device);

    return true;
}

void internal_ff_dx12_draw_device_before_reset(ff_dx12_draw_device* device)
{
    FF_ASSERT_RET(device);

    reset_batch(device);

    device->commands = NULL;
    device->state = ff_dx12_draw_state_machine_valid;
}

bool internal_ff_dx12_draw_device_reset(ff_dx12_draw_device* device)
{
    FF_ASSERT_RET_VAL(device, false);

    memset(device->palette_row_hashes, 0, sizeof(device->palette_row_hashes));
    memset(device->palette_remap_row_hashes, 0, sizeof(device->palette_remap_row_hashes));

    return true;
}

void ff_dx12_draw_device_destroy(ff_dx12_draw_device* device)
{
    FF_ASSERT_RET(device);

    ff_dx12_remove_device_child(&device->device_child);

    ff_dx12_buffer_destroy(&device->index_buffer);
    ff_dx12_buffer_destroy(&device->circle_vertex_buffer);

    ff_dx12_texture_view_destroy(&device->palette_texture_view);
    ff_dx12_texture_view_destroy(&device->palette_remap_texture_view);
    ff_dx12_texture_destroy(&device->palette_texture);
    ff_dx12_texture_destroy(&device->palette_remap_texture);

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
    ff_dx12_resource* target, D3D12_CPU_DESCRIPTOR_HANDLE target_view,
    ff_dx12_target_size target_size, DXGI_FORMAT target_format, ff_dx12_depth* depth,
    ff_rect_float view_rect, ff_rect_float world_rect, bool ignore_rotation)
{
    FF_ASSERT_RET_VAL(ff_dx12_draw_device_valid(device), false);
    FF_ASSERT_RET_VAL(commands, false);
    FF_ASSERT_RET_VAL(target, false);
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

    // A draw with no bound render target, viewport, or scissor rect produces no pixels and no
    // error: the default viewport is all zeros, so the rasterizer clips everything away.
    ff_dx12_resource* targets[1] = { target };
    const D3D12_CPU_DESCRIPTOR_HANDLE target_views[1] = { target_view };
    ff_dx12_resource* depth_resource = device->depth ? ff_dx12_depth_resource(device->depth) : NULL;
    const D3D12_CPU_DESCRIPTOR_HANDLE depth_view = device->depth
        ? ff_dx12_depth_view(device->depth)
        : (D3D12_CPU_DESCRIPTOR_HANDLE){ 0 };

    ff_dx12_commands_targets(commands, targets, target_views, NULL, 1,
        depth_resource, depth_resource ? &depth_view : NULL);

    if (device->depth)
    {
        ff_dx12_depth_clear_depth(device->depth, commands, 0.0f);
    }

    const D3D12_VIEWPORT viewport =
    {
        .TopLeftX = view_rect.left,
        .TopLeftY = view_rect.top,
        .Width = view_rect.right - view_rect.left,
        .Height = view_rect.bottom - view_rect.top,
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };

    ff_dx12_commands_viewports(commands, &viewport, 1);
    ff_dx12_commands_scissors(commands, NULL, 1);

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
    // The shader transforms row-vector style (mul(pos, m)) and HLSL cbuffers default to
    // column-major, so the projection is transposed on the way in exactly like the model matrices
    // are at intern time.
    device->vs_constants_0.projection = ff_matrix_transpose(device->view_matrix);

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
    // every other bucket has no per-vertex data at all, but its instance elements still live in
    // FF_DX12_INSTANCE_SLOT, so the binding always starts at slot 0 and pads slot 0 for circles.
    const bool circle = draw_bucket == ff_dx12_draw_bucket_circles_filled ||
        draw_bucket == ff_dx12_draw_bucket_circles_outline;

    ff_dx12_resource* resources[2];
    D3D12_VERTEX_BUFFER_VIEW views[2];

    if (circle)
    {
        resources[FF_DX12_VERTEX_SLOT] = ff_dx12_buffer_resource(&device->circle_vertex_buffer);
        views[FF_DX12_VERTEX_SLOT] = (D3D12_VERTEX_BUFFER_VIEW)
        {
            .BufferLocation = ff_dx12_buffer_gpu_address(&device->circle_vertex_buffer),
            .SizeInBytes = (UINT)ff_dx12_buffer_size(&device->circle_vertex_buffer),
            .StrideInBytes = (UINT)sizeof(ff_point_float),
        };
    }
    else
    {
        resources[FF_DX12_VERTEX_SLOT] = NULL;
        views[FF_DX12_VERTEX_SLOT] = (D3D12_VERTEX_BUFFER_VIEW){ 0 };
    }

    resources[FF_DX12_INSTANCE_SLOT] = NULL;
    views[FF_DX12_INSTANCE_SLOT] = (D3D12_VERTEX_BUFFER_VIEW)
    {
        .BufferLocation = device->instance_address,
        .SizeInBytes = (UINT)device->instance_byte_size,
        .StrideInBytes = (UINT)bucket->item_size,
    };

    ff_dx12_commands_vertex_buffers(device->commands, resources, views, 0, 2);

    return true;
}

// Copies only the palette and remap rows whose contents actually changed. The stored hash is what
// makes a static palette cost one upload for the life of the device rather than one per flush.
static void update_palette_textures(ff_dx12_draw_device* device)
{
    const bool needs_palettes = device->palette_texture_count != 0;
    const bool needs_remaps = device->palette_texture_count || device->target_requires_palette;

    FF_CHECK_RET(needs_palettes || needs_remaps);

    ff_dx12_commands_begin_event(device->commands, ff_dx12_gpu_event_update_palette);

    if (needs_palettes)
    {
        for (size_t i = 0; i < device->palette_count; i++)
        {
            ff_dx12_palette_data* data = device->palettes[i].palette.data;

            if (data && device->palette_row_hashes[i] != device->palettes[i].hash)
            {
                device->palette_row_hashes[i] = device->palettes[i].hash;

                const size_t src_row = device->palettes[i].palette.current_row;
                const D3D12_RECT source_rect =
                {
                    .left = 0,
                    .top = (LONG)src_row,
                    .right = FF_PALETTE_SIZE,
                    .bottom = (LONG)src_row + 1,
                };

                ff_dx12_commands_copy_texture(device->commands,
                    ff_dx12_texture_resource(&device->palette_texture), 0, 0, i,
                    ff_dx12_texture_resource(&data->texture), 0, &source_rect);
            }
        }
    }

    if (needs_remaps)
    {
        for (size_t i = 0; i < device->palette_remap_count; i++)
        {
            if (device->palette_remap_row_hashes[i] != device->palette_remaps[i].hash)
            {
                device->palette_remap_row_hashes[i] = device->palette_remaps[i].hash;

                ff_dx12_texture_update(&device->palette_remap_texture, device->commands, 0, 0, 0, i,
                    device->palette_remaps[i].remap.remap, FF_PALETTE_SIZE, 1, FF_PALETTE_SIZE);
            }
        }
    }

    ff_dx12_commands_end_event(device->commands);
}

static bool apply_texture_table(ff_dx12_draw_device* device, ff_dx12_texture_view** views,
    size_t count, ff_dx12_root_param root_param)
{
    FF_CHECK_RET_VAL(count, true);

    // The sampled textures must be in the shader-resource state before the draws that read them,
    // and resident for the whole submission, since the table only holds descriptors.
    for (size_t i = 0; i < count; i++)
    {
        ff_dx12_texture* texture = ff_dx12_texture_view_texture(views[i]);
        FF_CHECK_RET_VAL(texture, false);

        ff_dx12_commands_resource_state(device->commands, ff_dx12_texture_resource(texture),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            ff_dx12_texture_view_array_start(views[i]), ff_dx12_texture_view_array_count(views[i]),
            ff_dx12_texture_view_mip_start(views[i]), ff_dx12_texture_view_mip_count(views[i]));
    }

    ff_dx12_descriptor_range range = ff_dx12_gpu_descriptor_allocator_alloc(
        ff_dx12_gpu_view_descriptors(), count,
        ff_dx12_commands_next_fence_value(device->commands));

    FF_CHECK_RET_VAL(ff_dx12_descriptor_range_valid(&range), false);

    D3D12_CPU_DESCRIPTOR_HANDLE sources[FF_DX12_MAX_TEXTURES];
    UINT source_counts[FF_DX12_MAX_TEXTURES];

    for (size_t i = 0; i < count; i++)
    {
        sources[i] = ff_dx12_texture_view_cpu_handle(views[i]);
        source_counts[i] = 1;
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE dest = ff_dx12_descriptor_range_cpu_handle(&range, 0);
    const UINT dest_count = (UINT)count;

    ID3D12Device_CopyDescriptors((ID3D12Device*)ff_dx12_device(), 1, &dest, &dest_count,
        dest_count, sources, source_counts, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    ff_dx12_commands_root_descriptors(device->commands, root_param, &range, 0);

    return true;
}

// The palette and remap textures bind as one contiguous two-descriptor table, which is why they
// are copied together rather than through apply_texture_table.
static bool apply_palettes(ff_dx12_draw_device* device)
{
    FF_CHECK_RET_VAL(device->palette_texture_count || device->target_requires_palette, true);

    ff_dx12_commands_resource_state(device->commands,
        ff_dx12_texture_resource(&device->palette_texture),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 1, 0, 1);

    ff_dx12_commands_resource_state(device->commands,
        ff_dx12_texture_resource(&device->palette_remap_texture),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 1, 0, 1);

    ff_dx12_descriptor_range range = ff_dx12_gpu_descriptor_allocator_alloc(
        ff_dx12_gpu_view_descriptors(), 2, ff_dx12_commands_next_fence_value(device->commands));

    FF_CHECK_RET_VAL(ff_dx12_descriptor_range_valid(&range), false);

    D3D12_CPU_DESCRIPTOR_HANDLE sources[2];
    UINT source_counts[2] = { 1, 1 };
    sources[0] = ff_dx12_texture_view_cpu_handle(&device->palette_texture_view);
    sources[1] = ff_dx12_texture_view_cpu_handle(&device->palette_remap_texture_view);

    const D3D12_CPU_DESCRIPTOR_HANDLE dest = ff_dx12_descriptor_range_cpu_handle(&range, 0);
    const UINT dest_count = 2;

    ID3D12Device_CopyDescriptors((ID3D12Device*)ff_dx12_device(), 1, &dest, &dest_count,
        dest_count, sources, source_counts, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    ff_dx12_commands_root_descriptors(device->commands, ff_dx12_root_param_palettes, &range, 0);

    return true;
}

// The palette sprite shader divides the uv by the texture size to turn it back into a texel index,
// so the sizes have to reach the pixel shader for every palette texture in the table.
static void update_ps_constants(ff_dx12_draw_device* device)
{
    FF_CHECK_RET(device->palette_texture_count);

    for (size_t i = 0; i < device->palette_texture_count; i++)
    {
        ff_dx12_texture* texture = ff_dx12_texture_view_texture(device->palette_textures[i]);
        FF_CHECK_RET(texture);

        device->ps_constants_0.texture_palette_sizes[i].left = (float)ff_dx12_texture_width(texture);
        device->ps_constants_0.texture_palette_sizes[i].top = (float)ff_dx12_texture_height(texture);
    }

    // Uploaded whole rather than trimmed to palette_texture_count: 512 bytes at most once per
    // flush isn't worth the trim, and the debug layer validates a CBV read against the resource
    // size, so a short upload draws false out-of-bounds reports on padding no shader reads.
    const size_t size = sizeof(device->ps_constants_0);

    ff_dx12_mem_range range = ff_dx12_mem_allocator_ring_alloc_buffer(ff_dx12_upload_allocator(),
        size, ff_dx12_commands_next_fence_value(device->commands));

    void* dest = ff_dx12_mem_range_cpu_data(&range);
    FF_CHECK_RET(dest);

    memcpy(dest, &device->ps_constants_0, size);

    ff_dx12_commands_keep_resident(device->commands, ff_dx12_mem_range_residency_data(&range));

    ff_dx12_commands_root_cbv_address(device->commands, ff_dx12_root_param_ps_constants_0,
        ff_dx12_mem_range_gpu_data(&range));
}

static bool apply_textures(ff_dx12_draw_device* device)
{
    update_palette_textures(device);
    update_ps_constants(device);

    FF_CHECK_RET_VAL(apply_texture_table(device, device->textures, device->texture_count,
        ff_dx12_root_param_textures), false);

    FF_CHECK_RET_VAL(apply_texture_table(device, device->palette_textures, device->palette_texture_count,
        ff_dx12_root_param_palette_textures), false);

    return apply_palettes(device);
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

        if (apply_textures(device))
        {
            draw_opaque(device);
            draw_transparent(device);
        }
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

        // Past the far plane the depth test rejects everything, so the draw silently disappears
        // instead of sorting in front. Debug-only: guarding this at runtime would cost a branch on
        // every draw, and an unplanned flush to reclaim depth would stall a frame.
        FF_ASSERT_MSG(device->draw_depth <= FF_DX12_MAX_RENDER_DEPTH,
            "Too many draws in one frame; depth exhausted");
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

static uint32_t texture_index_no_flush(ff_dx12_draw_device* device, ff_dx12_texture_view* view)
{
    for (size_t i = device->texture_count; i != 0; i--)
    {
        if (device->textures[i - 1] == view)
        {
            return (uint32_t)(i - 1);
        }
    }

    if (device->texture_count == FF_DX12_MAX_TEXTURES)
    {
        return FF_DX12_INVALID_INDEX;
    }

    const uint32_t index = (uint32_t)device->texture_count;
    device->textures[index] = view;
    device->texture_count++;

    return index;
}

static uint32_t palette_remap_index_no_flush(ff_dx12_draw_device* device);

// The matrix and texture tables are both per-flush and both have to resolve against the same
// flush, so they are interned together: flushing to make room in one would invalidate an index
// already taken from the other.
static uint32_t sprite_indexes(ff_dx12_draw_device* device, ff_dx12_texture_view* view)
{
    uint32_t matrix_index = matrix_index_no_flush(device);
    uint32_t texture_index = texture_index_no_flush(device, view);

    // An RGBA sprite drawn into a palette target still writes indexes, so it goes through the
    // remap just like a palette sprite does.
    uint32_t remap_index = device->target_requires_palette ? palette_remap_index_no_flush(device) : 0;

    if (matrix_index == FF_DX12_INVALID_INDEX || texture_index == FF_DX12_INVALID_INDEX ||
        remap_index == FF_DX12_INVALID_INDEX)
    {
        ff_dx12_draw_device_flush(device);

        matrix_index = matrix_index_no_flush(device);
        texture_index = texture_index_no_flush(device, view);
        remap_index = device->target_requires_palette ? palette_remap_index_no_flush(device) : 0;

        FF_CHECK_RET_VAL(matrix_index != FF_DX12_INVALID_INDEX &&
            texture_index != FF_DX12_INVALID_INDEX &&
            remap_index != FF_DX12_INVALID_INDEX, FF_DX12_INVALID_INDEX);
    }

    const uint32_t sampler_index = (uint32_t)ff_dx12_draw_device_linear_sampler(device);

    return texture_index | (sampler_index << 8) | (remap_index << 16) | (matrix_index << 24);
}

static uint32_t palette_texture_index_no_flush(ff_dx12_draw_device* device, ff_dx12_texture_view* view)
{
    for (size_t i = device->palette_texture_count; i != 0; i--)
    {
        if (device->palette_textures[i - 1] == view)
        {
            return (uint32_t)(i - 1);
        }
    }

    if (device->palette_texture_count == FF_DX12_MAX_PALETTE_TEXTURES)
    {
        return FF_DX12_INVALID_INDEX;
    }

    const uint32_t index = (uint32_t)device->palette_texture_count;
    device->palette_textures[index] = view;
    device->palette_texture_count++;

    return index;
}

static uint32_t palette_index_no_flush(ff_dx12_draw_device* device)
{
    if (device->palette_index == FF_DX12_INVALID_INDEX)
    {
        // Drawing into a palette target means the indexes are the output, so there is nothing to
        // look up and row 0 stands in.
        if (device->target_requires_palette)
        {
            device->palette_index = 0;
            return device->palette_index;
        }

        const ff_dx12_palette* palette = &device->palette_stack[device->palette_stack_count - 1];

        // A palette sprite with no palette pushed has nothing to look up, so it is dropped rather
        // than left pointing at whatever happens to be in row 0.
        FF_CHECK_RET_VAL(palette->data, FF_DX12_INVALID_INDEX);

        const uint64_t hash = ff_dx12_palette_row_hash(palette);

        for (size_t i = 0; i < device->palette_count; i++)
        {
            if (device->palettes[i].hash == hash)
            {
                device->palette_index = (uint32_t)i;
                return device->palette_index;
            }
        }

        if (device->palette_count < FF_DX12_MAX_PALETTES)
        {
            const uint32_t index = (uint32_t)device->palette_count;
            device->palettes[index].palette = *palette;
            device->palettes[index].hash = hash;
            device->palette_count++;
            device->palette_index = index;
        }
    }

    return device->palette_index;
}

static uint32_t palette_remap_index_no_flush(ff_dx12_draw_device* device)
{
    if (device->palette_remap_index == FF_DX12_INVALID_INDEX)
    {
        const ff_dx12_palette_remap* remap = &device->palette_remap_stack[device->palette_remap_stack_count - 1];

        for (size_t i = 0; i < device->palette_remap_count; i++)
        {
            if (device->palette_remaps[i].hash == remap->hash)
            {
                device->palette_remap_index = (uint32_t)i;
                return device->palette_remap_index;
            }
        }

        if (device->palette_remap_count < FF_DX12_MAX_PALETTE_REMAPS)
        {
            const uint32_t index = (uint32_t)device->palette_remap_count;
            device->palette_remaps[index].remap = *remap;
            device->palette_remaps[index].hash = remap->hash;
            device->palette_remap_count++;
            device->palette_remap_index = index;
        }
    }

    return device->palette_remap_index;
}

// Four per-flush tables have to resolve against the same flush, so they are all interned before
// any flush happens. Flushing to make room in one would invalidate an index already taken from
// another.
static uint32_t palette_sprite_indexes(ff_dx12_draw_device* device, ff_dx12_texture_view* view)
{
    uint32_t matrix_index = matrix_index_no_flush(device);
    uint32_t texture_index = palette_texture_index_no_flush(device, view);
    uint32_t palette_index = palette_index_no_flush(device);
    uint32_t remap_index = palette_remap_index_no_flush(device);

    if (matrix_index == FF_DX12_INVALID_INDEX || texture_index == FF_DX12_INVALID_INDEX ||
        palette_index == FF_DX12_INVALID_INDEX || remap_index == FF_DX12_INVALID_INDEX)
    {
        ff_dx12_draw_device_flush(device);

        matrix_index = matrix_index_no_flush(device);
        texture_index = palette_texture_index_no_flush(device, view);
        palette_index = palette_index_no_flush(device);
        remap_index = palette_remap_index_no_flush(device);

        FF_CHECK_RET_VAL(matrix_index != FF_DX12_INVALID_INDEX &&
            texture_index != FF_DX12_INVALID_INDEX &&
            palette_index != FF_DX12_INVALID_INDEX &&
            remap_index != FF_DX12_INVALID_INDEX, FF_DX12_INVALID_INDEX);
    }

    return texture_index | (palette_index << 8) | (remap_index << 16) | (matrix_index << 24);
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

static void transparent_reserve(ff_dx12_draw_device* device)
{
    if (device->transparent_count == device->transparent_capacity)
    {
        const size_t new_capacity = device->transparent_capacity
            ? device->transparent_capacity * 2
            : FF_DX12_MIN_INSTANCE_BUCKET_COUNT;

        ff_dx12_transparent_entry* entries = ff_arena_realloc_type(&device->arena,
            ff_dx12_transparent_entry, device->transparent,
            device->transparent_capacity, new_capacity);

        device->transparent = entries;
        device->transparent_capacity = new_capacity;
    }
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
        transparent_reserve(device);

        ff_dx12_transparent_entry* entry = &device->transparent[device->transparent_count++];
        entry->bucket_type = bucket_type;
        entry->index = bucket->count;
        entry->depth = depth;
    }

    return ff_dx12_instance_bucket_add(bucket);
}

typedef enum alpha_type
{
    alpha_type_opaque,
    alpha_type_transparent,
    alpha_type_invisible,
} alpha_type;

static alpha_type get_alpha_type(float alpha, bool allow_transparent)
{
    if (alpha == 0.0f)
    {
        return alpha_type_invisible;
    }

    return (alpha == 1.0f || !allow_transparent) ? alpha_type_opaque : alpha_type_transparent;
}

static alpha_type merge_alpha_type(float alpha, bool allow_transparent, alpha_type previous)
{
    const alpha_type type = get_alpha_type(alpha, allow_transparent);
    return (type == previous) ? type : (allow_transparent ? alpha_type_transparent : alpha_type_opaque);
}

// A sprite whose own pixels have partial alpha needs blending even when the tint is fully opaque.
// The reverse doesn't hold: an opaque sprite under a translucent tint is already transparent from
// the tint alone.
static alpha_type sprite_alpha_type(float alpha, bool allow_transparent, bool sprite_transparent)
{
    const alpha_type type = get_alpha_type(alpha, allow_transparent);

    return (type == alpha_type_opaque && sprite_transparent && allow_transparent)
        ? alpha_type_transparent
        : type;
}

static ff_dx12_instance_bucket_type transparent_bucket(ff_dx12_instance_bucket_type opaque_bucket)
{
    return (ff_dx12_instance_bucket_type)(opaque_bucket + ff_dx12_instance_bucket_first_transparent);
}

static ff_dx12_instance_bucket_type pick_bucket(ff_dx12_instance_bucket_type opaque_bucket, alpha_type type)
{
    return (type == alpha_type_transparent) ? transparent_bucket(opaque_bucket) : opaque_bucket;
}

// Geometry colors need CPU remap because ps_color_out_palette writes the vertex color directly.
static const uint8_t* active_remap(const ff_dx12_draw_device* device)
{
    return device->palette_remap_stack[device->palette_remap_stack_count - 1].remap;
}

static void store_color(float dest[4], ff_color color, const uint8_t* index_remap)
{
    const ff_color_shader shader = ff_color_to_shader(color, index_remap);
    dest[0] = shader.r;
    dest[1] = shader.g;
    dest[2] = shader.b;
    dest[3] = shader.a;
}

void ff_dx12_draw_device_draw_lines(ff_dx12_draw_device* device,
    const ff_dx12_draw_endpoint* points, size_t count)
{
    FF_ASSERT_RET(device);
    FF_ASSERT_RET(points || !count);
    FF_CHECK_RET(count > 1);

    const bool allow_transparent = ff_dx12_draw_device_allow_transparent(device);
    const bool closed = count > 2 &&
        points[0].pos.x == points[count - 1].pos.x &&
        points[0].pos.y == points[count - 1].pos.y;

    const uint32_t matrix_index = ff_dx12_draw_device_matrix_index(device);
    FF_CHECK_RET(matrix_index != FF_DX12_INVALID_INDEX);

    // One depth for the whole polyline, so that all of its segments merge into a single instanced
    // draw. Segments of one line are not expected to overlap each other.
    const float depth = ff_dx12_draw_device_nudge_depth(device);

    for (size_t i = 0; i + 1 < count; i++)
    {
        const ff_dx12_draw_endpoint* p0 = &points[i];
        const ff_dx12_draw_endpoint* p1 = &points[i + 1];

        const bool degenerate = (p0->pos.x == p1->pos.x && p0->pos.y == p1->pos.y) ||
            (p0->size == 0.0f && p1->size == 0.0f);

        if (degenerate)
        {
            continue;
        }

        alpha_type type = get_alpha_type(ff_color_alpha(p0->color), allow_transparent);
        type = merge_alpha_type(ff_color_alpha(p1->color), allow_transparent, type);

        if (type == alpha_type_invisible)
        {
            continue;
        }

        ff_dx12_line_instance* instance = (ff_dx12_line_instance*)ff_dx12_draw_device_add_instance(
            device, pick_bucket(ff_dx12_instance_bucket_lines, type), depth);

        instance->start = p0->pos;
        instance->end = p1->pos;

        // The miter neighbors. A closed polyline wraps to the other end, skipping the duplicated
        // point that closes it; an open one repeats its own endpoint, which makes a flat joint.
        instance->before_start = (i == 0)
            ? (closed ? points[count - 2].pos : p0->pos)
            : points[i - 1].pos;

        instance->after_end = (i + 2 == count)
            ? (closed ? points[1].pos : p1->pos)
            : points[i + 2].pos;

        store_color(instance->start_color, p0->color, active_remap(device));
        store_color(instance->end_color, p1->color, active_remap(device));
        instance->start_thickness = fabsf(p0->size);
        instance->end_thickness = fabsf(p1->size);
        instance->depth = depth;
        instance->matrix_index = matrix_index;
    }
}

void ff_dx12_draw_device_draw_triangles(ff_dx12_draw_device* device,
    const ff_dx12_draw_endpoint* points, size_t count)
{
    FF_ASSERT_RET(device);
    FF_ASSERT_RET(points || !count);
    FF_CHECK_RET(count >= 3);

    const bool allow_transparent = ff_dx12_draw_device_allow_transparent(device);
    const uint32_t matrix_index = ff_dx12_draw_device_matrix_index(device);
    FF_CHECK_RET(matrix_index != FF_DX12_INVALID_INDEX);

    const float depth = ff_dx12_draw_device_nudge_depth(device);

    for (size_t i = 0; i + 2 < count; i += 3)
    {
        alpha_type type = get_alpha_type(ff_color_alpha(points[i].color), allow_transparent);
        type = merge_alpha_type(ff_color_alpha(points[i + 1].color), allow_transparent, type);
        type = merge_alpha_type(ff_color_alpha(points[i + 2].color), allow_transparent, type);

        if (type == alpha_type_invisible)
        {
            continue;
        }

        ff_dx12_triangle_instance* instance = (ff_dx12_triangle_instance*)ff_dx12_draw_device_add_instance(
            device, pick_bucket(ff_dx12_instance_bucket_triangles, type), depth);

        for (size_t corner = 0; corner < 3; corner++)
        {
            instance->position[corner] = points[i + corner].pos;
            store_color(instance->color[corner], points[i + corner].color, active_remap(device));
        }

        instance->depth = depth;
        instance->matrix_index = matrix_index;
    }
}

void ff_dx12_draw_device_draw_rectangle(ff_dx12_draw_device* device,
    ff_rect_float rect, ff_color color, float thickness)
{
    FF_ASSERT_RET(device);

    const alpha_type type = get_alpha_type(ff_color_alpha(color),
        ff_dx12_draw_device_allow_transparent(device));

    FF_CHECK_RET(type != alpha_type_invisible);

    ff_rect_float normalized = ff_rect_float_normalize(rect);
    FF_CHECK_RET(ff_rect_float_area(normalized) != 0.0f);

    if (thickness < 0.0f)
    {
        normalized = ff_rect_float_deflate(normalized, thickness, thickness);
        thickness = -thickness;
    }

    // An outline thick enough to meet itself has no hole left, so draw it as a fill instead. The
    // outline shader would otherwise produce overlapping self-intersecting geometry.
    if (thickness * 2.0f >= ff_rect_float_width(normalized) ||
        thickness * 2.0f >= ff_rect_float_height(normalized))
    {
        thickness = 0.0f;
    }

    const ff_dx12_instance_bucket_type opaque_bucket = thickness
        ? ff_dx12_instance_bucket_rectangles_outline
        : ff_dx12_instance_bucket_rectangles_filled;

    const uint32_t matrix_index = ff_dx12_draw_device_matrix_index(device);
    FF_CHECK_RET(matrix_index != FF_DX12_INVALID_INDEX);

    const float depth = ff_dx12_draw_device_nudge_depth(device);

    ff_dx12_rectangle_instance* instance = (ff_dx12_rectangle_instance*)ff_dx12_draw_device_add_instance(
        device, pick_bucket(opaque_bucket, type), depth);

    instance->rect = normalized;
    store_color(instance->color, color, active_remap(device));
    instance->depth = depth;
    instance->thickness = thickness;
    instance->matrix_index = matrix_index;
}

void ff_dx12_draw_device_draw_circle(ff_dx12_draw_device* device,
    ff_dx12_draw_endpoint pos, float thickness, ff_color outside_color)
{
    FF_ASSERT_RET(device);

    float radius = fabsf(pos.size);
    FF_CHECK_RET(radius != 0.0f);

    const bool allow_transparent = ff_dx12_draw_device_allow_transparent(device);
    alpha_type type = get_alpha_type(ff_color_alpha(pos.color), allow_transparent);
    type = merge_alpha_type(ff_color_alpha(outside_color), allow_transparent, type);
    FF_CHECK_RET(type != alpha_type_invisible);

    if (thickness < 0.0f)
    {
        radius -= thickness;
        thickness = -thickness;
    }

    // An outline at least as thick as the radius leaves no hole, so it is a filled circle.
    if (thickness >= radius)
    {
        thickness = 0.0f;
    }

    const ff_dx12_instance_bucket_type opaque_bucket = thickness
        ? ff_dx12_instance_bucket_circles_outline
        : ff_dx12_instance_bucket_circles_filled;

    const uint32_t matrix_index = ff_dx12_draw_device_matrix_index(device);
    FF_CHECK_RET(matrix_index != FF_DX12_INVALID_INDEX);

    const float depth = ff_dx12_draw_device_nudge_depth(device);

    ff_dx12_circle_instance* instance = (ff_dx12_circle_instance*)ff_dx12_draw_device_add_instance(
        device, pick_bucket(opaque_bucket, type), depth);

    instance->position_radius[0] = pos.pos.x;
    instance->position_radius[1] = pos.pos.y;
    instance->position_radius[2] = depth;
    instance->position_radius[3] = radius;
    store_color(instance->inside_color, pos.color, active_remap(device));
    store_color(instance->outside_color, outside_color, active_remap(device));
    instance->thickness = thickness;
    instance->matrix_index = matrix_index;
}

ff_dx12_sprite_transform ff_dx12_sprite_transform_default(void)
{
    return (ff_dx12_sprite_transform)
    {
        .position = { .x = 0.0f, .y = 0.0f },
        .scale = { .x = 1.0f, .y = 1.0f },
        .rotation_radians = 0.0f,
        .color = ff_color_white(),
    };
}

void ff_dx12_draw_device_draw_sprite(ff_dx12_draw_device* device,
    const ff_dx12_sprite* sprite, const ff_dx12_sprite_transform* transform)
{
    FF_ASSERT_RET(device);
    FF_ASSERT_RET(sprite && transform);
    FF_CHECK_RET(sprite->view);

    const bool allow_transparent = ff_dx12_draw_device_allow_transparent(device);
    const alpha_type type = sprite_alpha_type(ff_color_alpha(transform->color),
        allow_transparent, sprite->transparent);
    FF_CHECK_RET(type != alpha_type_invisible);

    const uint32_t indexes = sprite_indexes(device, sprite->view);
    FF_CHECK_RET(indexes != FF_DX12_INVALID_INDEX);

    const float depth = ff_dx12_draw_device_nudge_depth(device);

    ff_dx12_sprite_instance* instance = (ff_dx12_sprite_instance*)ff_dx12_draw_device_add_instance(
        device, pick_bucket(ff_dx12_instance_bucket_sprites, type), depth);

    instance->rect = ff_rect_float_scale(sprite->world, transform->scale);
    instance->uv_rect = sprite->texture_uv;
    store_color(instance->color, transform->color, NULL);
    instance->pos_rot[0] = transform->position.x;
    instance->pos_rot[1] = transform->position.y;
    instance->pos_rot[2] = depth;
    instance->pos_rot[3] = transform->rotation_radians;
    instance->indexes = indexes;
}

void ff_dx12_draw_device_draw_palette_sprite(ff_dx12_draw_device* device,
    const ff_dx12_sprite* sprite, const ff_dx12_sprite_transform* transform)
{
    FF_ASSERT_RET(device);
    FF_ASSERT_RET(sprite && transform);
    FF_CHECK_RET(sprite->view);

    const bool allow_transparent = ff_dx12_draw_device_allow_transparent(device);
    const alpha_type type = sprite_alpha_type(ff_color_alpha(transform->color), allow_transparent, sprite->transparent);
    FF_CHECK_RET(type != alpha_type_invisible);

    const uint32_t indexes = palette_sprite_indexes(device, sprite->view);
    FF_CHECK_RET(indexes != FF_DX12_INVALID_INDEX);

    const float depth = ff_dx12_draw_device_nudge_depth(device);

    ff_dx12_sprite_instance* instance = (ff_dx12_sprite_instance*)ff_dx12_draw_device_add_instance(
        device, pick_bucket(ff_dx12_instance_bucket_palette_sprites, type), depth);

    instance->rect = ff_rect_float_scale(sprite->world, transform->scale);
    instance->uv_rect = sprite->texture_uv;
    store_color(instance->color, transform->color, NULL);
    instance->pos_rot[0] = transform->position.x;
    instance->pos_rot[1] = transform->position.y;
    instance->pos_rot[2] = depth;
    instance->pos_rot[3] = transform->rotation_radians;
    instance->indexes = indexes;
}

void ff_dx12_draw_device_push_palette(ff_dx12_draw_device* device, ff_dx12_palette palette)
{
    FF_ASSERT_RET(device);
    FF_ASSERT_RET(device->palette_stack_count < FF_DX12_MAX_PALETTE_STACK);

    device->palette_stack[device->palette_stack_count++] = palette;
    device->palette_index = FF_DX12_INVALID_INDEX;
}

void ff_dx12_draw_device_pop_palette(ff_dx12_draw_device* device)
{
    FF_ASSERT_RET(device);
    FF_ASSERT_RET(device->palette_stack_count > 1);

    device->palette_stack_count--;
    device->palette_index = FF_DX12_INVALID_INDEX;
}

void ff_dx12_draw_device_push_palette_remap(ff_dx12_draw_device* device, const ff_dx12_palette_remap* remap)
{
    FF_ASSERT_RET(device);
    FF_ASSERT_RET(device->palette_remap_stack_count < FF_DX12_MAX_PALETTE_STACK);

    device->palette_remap_stack[device->palette_remap_stack_count++] =
        remap ? *remap : ff_dx12_palette_remap_identity();

    device->palette_remap_index = FF_DX12_INVALID_INDEX;
}

void ff_dx12_draw_device_pop_palette_remap(ff_dx12_draw_device* device)
{
    FF_ASSERT_RET(device);
    FF_ASSERT_RET(device->palette_remap_stack_count > 1);

    device->palette_remap_stack_count--;
    device->palette_remap_index = FF_DX12_INVALID_INDEX;
}
