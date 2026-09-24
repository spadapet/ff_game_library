#pragma once

#include "dx12_commands.h"
#include "dx12_descriptor_range.h"
#include "dx12_texture.h"

// A render target view over part of a texture. The texture is borrowed, not owned, so it has to
// outlive the target.
typedef struct ff_dx12_target_texture
{
    ff_dx12_texture* texture;
    ff_dx12_descriptor_range view;

    // The sub-range this target renders to. A target always covers exactly one mip level, which
    // is why there is no mip_count here.
    size_t array_start;
    size_t array_count;
    size_t mip_level;
} ff_dx12_target_texture;

// array_count of 0 means "the rest of the array starting at array_start".
bool ff_dx12_target_texture_init(ff_dx12_target_texture* target, ff_dx12_texture* texture,
    size_t array_start, size_t array_count, size_t mip_level);
void ff_dx12_target_texture_destroy(ff_dx12_target_texture* target);

bool ff_dx12_target_texture_valid(const ff_dx12_target_texture* target);
ff_dx12_resource* ff_dx12_target_texture_resource(ff_dx12_target_texture* target);
D3D12_CPU_DESCRIPTOR_HANDLE ff_dx12_target_texture_view(const ff_dx12_target_texture* target);

// The sub-range to hand to ff_dx12_commands_targets so that state transitions only touch the
// subresources this target actually covers.
ff_dx12_target_range ff_dx12_target_texture_range(const ff_dx12_target_texture* target);

size_t ff_dx12_target_texture_width(const ff_dx12_target_texture* target);
size_t ff_dx12_target_texture_height(const ff_dx12_target_texture* target);
size_t ff_dx12_target_texture_sample_count(const ff_dx12_target_texture* target);
DXGI_FORMAT ff_dx12_target_texture_format(const ff_dx12_target_texture* target);

void ff_dx12_target_texture_clear(ff_dx12_target_texture* target, ff_dx12_commands* commands, const float color[4]);
void ff_dx12_target_texture_discard(ff_dx12_target_texture* target, ff_dx12_commands* commands);

// clear_color of NULL discards the previous contents instead of clearing them.
bool ff_dx12_target_texture_begin_render(ff_dx12_target_texture* target, ff_dx12_commands* commands, const float* clear_color);
bool ff_dx12_target_texture_end_render(ff_dx12_target_texture* target, ff_dx12_commands* commands);
