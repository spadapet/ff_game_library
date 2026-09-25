#pragma once

#include "dx12_descriptor_range.h"
#include "dx12_device_child.h"

typedef struct ff_dx12_texture ff_dx12_texture;

// An SRV over part of an ff_dx12_texture: one contiguous run of array slices and one run of mip
// levels. ff_dx12_texture itself only ever views the whole resource, which is all a render target
// or a full-surface blit needs, but sprites view one slice of an atlas at a time.
//
// The texture is referenced, not owned, so it has to outlive every view of it.
typedef struct ff_dx12_texture_view
{
    ff_dx12_texture* texture;
    ff_dx12_device_child device_child;

    // Allocated on first ff_dx12_texture_view_cpu_handle call, since a view can be created up front
    // and never sampled.
    ff_dx12_descriptor_range view;

    // Resolved against the texture at init, so these are never 0 and never mean "the rest".
    size_t array_start;
    size_t array_count;
    size_t mip_start;
    size_t mip_count;

    size_t reset_count;
} ff_dx12_texture_view;

// array_count and mip_count of 0 mean "the rest of the texture" starting at the matching start.
bool ff_dx12_texture_view_init(ff_dx12_texture_view* view, ff_dx12_texture* texture,
    size_t array_start, size_t array_count, size_t mip_start, size_t mip_count);
void ff_dx12_texture_view_destroy(ff_dx12_texture_view* view);

bool ff_dx12_texture_view_valid(const ff_dx12_texture_view* view);
ff_dx12_texture* ff_dx12_texture_view_texture(ff_dx12_texture_view* view);
size_t ff_dx12_texture_view_array_start(const ff_dx12_texture_view* view);
size_t ff_dx12_texture_view_array_count(const ff_dx12_texture_view* view);
size_t ff_dx12_texture_view_mip_start(const ff_dx12_texture_view* view);
size_t ff_dx12_texture_view_mip_count(const ff_dx12_texture_view* view);

D3D12_CPU_DESCRIPTOR_HANDLE ff_dx12_texture_view_cpu_handle(ff_dx12_texture_view* view);

// Device reset: re-creates the SRV against the rebuilt texture, keeping the same descriptor slot.
// There is no before_reset because the descriptor allocators survive a reset in place and the
// texture handles its own teardown. This mirrors ff_dx12_texture. The old C++ freed the range and
// reallocated on next use instead, which is equivalent but churns the allocator for no reason.
bool internal_ff_dx12_texture_view_reset(ff_dx12_texture_view* view);

// Counts how many times the SRV has actually been rewritten against a rebuilt texture. Without
// this a reset test can only observe that the view still returns a usable handle, which stays
// true even if the descriptor was never refreshed and still describes the destroyed resource.
size_t ff_dx12_texture_view_reset_count(const ff_dx12_texture_view* view);
