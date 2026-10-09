#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/hash.h"
#include "dx12/dx12_internal.h"
#include "dx12/dx12_palette.h"

// Zero is the "this slot has never been uploaded" sentinel in the draw device's row-hash cache, so
// no real hash is allowed to be zero.
static uint64_t palette_hash_bytes(const void* data, size_t size)
{
    const uint64_t hash = ff_hash_bytes(data, size);
    return hash ? hash : 1;
}

bool ff_dx12_palette_data_init(ff_dx12_palette_data* data, ff_dx12_commands* commands,
    const uint32_t* colors, size_t row_count)
{
    FF_ASSERT_RET_VAL(data, false);

    *data = (ff_dx12_palette_data){ 0 };
    FF_CHECK_RET_VAL(colors && row_count, false);

    ff_arena_init_heap_local(&data->arena, 0);
    data->row_hashes = ff_arena_alloc_type(&data->arena, uint64_t, row_count);
    data->colors = ff_arena_alloc_type(&data->arena, uint32_t, row_count * FF_PALETTE_SIZE);

    memcpy(data->colors, colors, row_count * FF_PALETTE_SIZE * sizeof(uint32_t));

    for (size_t i = 0; i < row_count; i++)
    {
        data->row_hashes[i] = palette_hash_bytes(colors + i * FF_PALETTE_SIZE, FF_PALETTE_SIZE * sizeof(uint32_t));
    }

    ff_dx12_texture_params params = ff_dx12_texture_params_default(FF_PALETTE_SIZE, row_count);

    if (!ff_dx12_texture_init(&data->texture, &params) ||
        !ff_dx12_texture_update(&data->texture, commands, 0, 0, 0, 0,
            colors, FF_PALETTE_SIZE, row_count, FF_PALETTE_SIZE * sizeof(uint32_t)))
    {
        ff_dx12_palette_data_destroy(data);
        return false;
    }

    data->row_count = row_count;
    ff_dx12_add_device_child(&data->device_child, data, ff_dx12_device_child_type_palette_data);

    return true;
}

void ff_dx12_palette_data_destroy(ff_dx12_palette_data* data)
{
    FF_CHECK_RET(data);

    ff_dx12_remove_device_child(&data->device_child);
    ff_dx12_texture_destroy(&data->texture);
    ff_arena_destroy(&data->arena);

    data->row_hashes = NULL;
    data->colors = NULL;
    data->row_count = 0;
}

bool internal_ff_dx12_palette_data_reset(ff_dx12_palette_data* data, ff_dx12_commands* commands)
{
    FF_ASSERT_RET_VAL(data, false);
    FF_CHECK_RET_VAL(data->row_count, false);
    FF_ASSERT_RET_VAL(commands, false);

    return ff_dx12_texture_update(&data->texture, commands, 0, 0, 0, 0,
        data->colors, FF_PALETTE_SIZE, data->row_count, FF_PALETTE_SIZE * sizeof(uint32_t));
}

bool ff_dx12_palette_data_valid(const ff_dx12_palette_data* data)
{
    return data && data->row_count && ff_dx12_texture_valid(&data->texture);
}

size_t ff_dx12_palette_data_row_count(const ff_dx12_palette_data* data)
{
    return data ? data->row_count : 0;
}

uint64_t ff_dx12_palette_data_row_hash(const ff_dx12_palette_data* data, size_t row)
{
    FF_CHECK_RET_VAL(data && row < data->row_count, 0);
    return data->row_hashes[row];
}

ff_dx12_palette ff_dx12_palette_make(ff_dx12_palette_data* data, size_t current_row)
{
    return (ff_dx12_palette)
    {
        .data = data,
        .current_row = (data && current_row < data->row_count) ? current_row : 0,
    };
}

uint64_t ff_dx12_palette_row_hash(const ff_dx12_palette* palette)
{
    FF_CHECK_RET_VAL(palette && palette->data, 0);
    return ff_dx12_palette_data_row_hash(palette->data, palette->current_row);
}

ff_dx12_palette_remap ff_dx12_palette_remap_identity(void)
{
    ff_dx12_palette_remap remap;

    for (size_t i = 0; i < FF_PALETTE_SIZE; i++)
    {
        remap.remap[i] = (uint8_t)i;
    }

    remap.hash = palette_hash_bytes(remap.remap, FF_PALETTE_SIZE);

    return remap;
}

ff_dx12_palette_remap ff_dx12_palette_remap_make(ff_span remap)
{
    ff_dx12_palette_remap result = ff_dx12_palette_remap_identity();
    FF_CHECK_RET_VAL(remap.data && remap.size, result);

    const size_t count = (remap.size < FF_PALETTE_SIZE) ? remap.size : FF_PALETTE_SIZE;
    memcpy(result.remap, remap.data, count);
    result.hash = palette_hash_bytes(result.remap, FF_PALETTE_SIZE);

    return result;
}
