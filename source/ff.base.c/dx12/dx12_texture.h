#pragma once

#include "dx12_descriptor_range.h"
#include "dx12_resource.h"

typedef struct ff_dx12_commands ff_dx12_commands;

typedef struct ff_dx12_texture_params
{
    size_t width;
    size_t height;
    DXGI_FORMAT format;
    size_t mip_count;
    size_t array_size;
    size_t sample_count;

    // NULL means no optimized clear value, which is what a plain shader resource wants. Supplying
    // one only helps when the texture is actually used as a render target.
    const float* optimized_clear_color;
} ff_dx12_texture_params;

// mip_count, array_size and sample_count of 0 all default to 1, and format defaults to
// R8G8B8A8_UNORM.
ff_dx12_texture_params ff_dx12_texture_params_default(size_t width, size_t height);

// A GPU-side 2D texture (or texture array) plus the SRV that views the whole thing. CPU-side
// image data and mip generation are not part of this layer: uploads go through
// ff_dx12_commands_update_texture.
typedef struct ff_dx12_texture
{
    ff_dx12_resource resource;

    // Allocated on first use by ff_dx12_texture_view, because a texture that is only ever a
    // render target never needs an SRV.
    ff_dx12_descriptor_range view;
} ff_dx12_texture;

bool ff_dx12_texture_init(ff_dx12_texture* texture, const ff_dx12_texture_params* params);
void ff_dx12_texture_destroy(ff_dx12_texture* texture);

bool ff_dx12_texture_valid(const ff_dx12_texture* texture);
size_t ff_dx12_texture_width(const ff_dx12_texture* texture);
size_t ff_dx12_texture_height(const ff_dx12_texture* texture);
size_t ff_dx12_texture_mip_count(const ff_dx12_texture* texture);
size_t ff_dx12_texture_array_size(const ff_dx12_texture* texture);
size_t ff_dx12_texture_sample_count(const ff_dx12_texture* texture);
DXGI_FORMAT ff_dx12_texture_format(const ff_dx12_texture* texture);
D3D12_CLEAR_VALUE ff_dx12_texture_optimized_clear_value(const ff_dx12_texture* texture);
ff_dx12_resource* ff_dx12_texture_resource(ff_dx12_texture* texture);

D3D12_CPU_DESCRIPTOR_HANDLE ff_dx12_texture_view(ff_dx12_texture* texture);

// Copies a tightly packed block of pixels into one subresource at (dest_x, dest_y). The data is
// staged through the shared upload allocator, so it may be freed as soon as this returns.
bool ff_dx12_texture_update(ff_dx12_texture* texture, ff_dx12_commands* commands,
    size_t array_index, size_t mip_index, size_t dest_x, size_t dest_y,
    const void* data, size_t width, size_t height, size_t row_pitch);
