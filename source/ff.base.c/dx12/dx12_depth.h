#pragma once

#include "dx12_descriptor_range.h"
#include "dx12_resource.h"

typedef struct ff_dx12_commands ff_dx12_commands;

#define FF_DX12_DEPTH_FORMAT DXGI_FORMAT_D24_UNORM_S8_UINT

// A depth/stencil buffer: one committed resource plus the single DSV that views it.
typedef struct ff_dx12_depth
{
    ff_dx12_resource resource;
    ff_dx12_descriptor_range view;
} ff_dx12_depth;

// size of 0 in either axis is clamped to 1, and sample_count is reduced to something the device
// supports for the depth format.
bool ff_dx12_depth_init(ff_dx12_depth* depth, size_t width, size_t height, size_t sample_count);
void ff_dx12_depth_destroy(ff_dx12_depth* depth);

bool ff_dx12_depth_valid(const ff_dx12_depth* depth);
size_t ff_dx12_depth_width(const ff_dx12_depth* depth);
size_t ff_dx12_depth_height(const ff_dx12_depth* depth);
size_t ff_dx12_depth_sample_count(const ff_dx12_depth* depth);
ff_dx12_resource* ff_dx12_depth_resource(ff_dx12_depth* depth);
D3D12_CPU_DESCRIPTOR_HANDLE ff_dx12_depth_view(const ff_dx12_depth* depth);

// Recreates the resource at a new size when it differs, keeping the same DSV slot. The old
// resource is released through the keep-alive list, so in-flight GPU work stays valid.
bool ff_dx12_depth_set_size(ff_dx12_depth* depth, size_t width, size_t height);

void ff_dx12_depth_clear(ff_dx12_depth* depth, ff_dx12_commands* commands, float depth_value, uint8_t stencil_value);
void ff_dx12_depth_clear_depth(ff_dx12_depth* depth, ff_dx12_commands* commands, float depth_value);
void ff_dx12_depth_clear_stencil(ff_dx12_depth* depth, ff_dx12_commands* commands, uint8_t stencil_value);
void ff_dx12_depth_discard(ff_dx12_depth* depth, ff_dx12_commands* commands);
