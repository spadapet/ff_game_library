#pragma once

#include "../base/string.h"

typedef struct ff_arena ff_arena;

typedef struct ff_dx12_init_params
{
    DXGI_GPU_PREFERENCE gpu_preference;
    D3D_FEATURE_LEVEL feature_level;
} ff_dx12_init_params;

ff_dx12_init_params ff_dx12_init_params_default(void);

bool ff_dx12_init(const ff_dx12_init_params* params);
void ff_dx12_destroy(void);

IDXGIFactory6* ff_dx12_factory(void);
IDXGIAdapter3* ff_dx12_adapter(void);
ID3D12Device6* ff_dx12_device(void);
D3D_FEATURE_LEVEL ff_dx12_feature_level(void);

bool ff_dx12_device_valid(void);
void ff_dx12_device_fatal_error(ff_string_view reason);
bool ff_dx12_supports_create_heap_not_resident(void);

// Resource binding tier 3 plus shader model 6.6, which is what ResourceDescriptorHeap[] indexing
// requires. The renderer picks bindless or classic descriptor tables based on this.
bool ff_dx12_supports_bindless(void);

// Adapter list changes (like a GPU being added or removed) make the factory stale.
bool ff_dx12_factory_current(void);
uint64_t ff_dx12_adapters_hash(void);

// Testing hook: makes the next reset take the stale-factory path without needing real hardware
// to change. Cleared when the factory is recreated.
void ff_dx12_simulate_factory_stale(void);

DXGI_QUERY_VIDEO_MEMORY_INFO ff_dx12_video_memory_info(void);
void ff_dx12_update_video_memory_info(void);

ff_string_view ff_dx12_adapter_name(IDXGIAdapter3* adapter, ff_arena* arena);

typedef struct ff_dx12_queue ff_dx12_queue;
typedef struct ff_dx12_mem_range ff_dx12_mem_range;
typedef struct ff_dx12_residency_data ff_dx12_residency_data;
typedef struct ff_dx12_fence_values ff_dx12_fence_values;
typedef struct ff_dx12_mem_allocator ff_dx12_mem_allocator;
typedef struct ff_dx12_cpu_descriptor_allocator ff_dx12_cpu_descriptor_allocator;
typedef struct ff_dx12_gpu_descriptor_allocator ff_dx12_gpu_descriptor_allocator;

// Shared memory allocators. The first three hand out transient per-frame ranges keyed on a fence
// value; the last three are long-lived free-list allocators. All are created on first use and
// destroyed with the device.
ff_dx12_mem_allocator* ff_dx12_upload_allocator(void);
ff_dx12_mem_allocator* ff_dx12_readback_allocator(void);
ff_dx12_mem_allocator* ff_dx12_dynamic_buffer_allocator(void);
ff_dx12_mem_allocator* ff_dx12_static_buffer_allocator(void);
ff_dx12_mem_allocator* ff_dx12_texture_allocator(void);
ff_dx12_mem_allocator* ff_dx12_target_allocator(void);

// Staging (non-shader-visible) descriptors, one allocator per heap type.
ff_dx12_cpu_descriptor_allocator* ff_dx12_cpu_buffer_descriptors(void);
ff_dx12_cpu_descriptor_allocator* ff_dx12_cpu_sampler_descriptors(void);
ff_dx12_cpu_descriptor_allocator* ff_dx12_cpu_target_descriptors(void);
ff_dx12_cpu_descriptor_allocator* ff_dx12_cpu_depth_descriptors(void);

// Shader-visible descriptors. Only one heap of each type can be bound at a time, so these are the
// heaps that every non-copy command list binds through SetDescriptorHeaps.
ff_dx12_gpu_descriptor_allocator* ff_dx12_gpu_view_descriptors(void);
ff_dx12_gpu_descriptor_allocator* ff_dx12_gpu_sampler_descriptors(void);

ff_dx12_queue* ff_dx12_direct_queue(void);
ff_dx12_queue* ff_dx12_copy_queue(void);
ff_dx12_queue* ff_dx12_compute_queue(void);
ff_dx12_queue* ff_dx12_queue_from_type(D3D12_COMMAND_LIST_TYPE type);

void ff_dx12_wait_for_idle(void);

// Scrubs residency data out of every queue's command caches. Called when the data is destroyed
// while command lists may still reference it, since the data lives inside the dying resource.
void ff_dx12_forget_residency_data(ff_dx12_residency_data* data);

// Frame lifecycle. frame_started drains the keep-alive list and refreshes the video memory
// budget; frame_complete advances the frame counter.
void ff_dx12_frame_started(void);
void ff_dx12_frame_complete(void);
uint64_t ff_dx12_frame_count(void);

// Largest power-of-two sample count at or below 'sample_count' that the device actually supports
// for 'format'. Always at least 1.
size_t ff_dx12_fix_sample_count(DXGI_FORMAT format, size_t sample_count);

// Defers releasing an ID3D12Resource (and freeing its mem_range) until the GPU work named by
// 'fence_values' has retired. When that work is already complete the release happens immediately
// with no bookkeeping. Anything queued here is released by ff_dx12_flush_keep_alive, which runs
// from frame_started and wait_for_idle.
void ff_dx12_keep_alive_resource(ID3D12Resource* resource, const ff_dx12_mem_range* mem_range,
    const ff_dx12_fence_values* fence_values);
void ff_dx12_flush_keep_alive(void);

// Device reset internals, used only by dx12_reset.c. init_dxgi/init_d3d are the same routines
// ff_dx12_init uses; the 'for_reset' flag skips the one-time process-wide setup (adapter removal
// support, the debug layer) that must not be repeated.
bool internal_ff_dx12_init_dxgi(bool for_reset);
void internal_ff_dx12_destroy_dxgi(void);
bool internal_ff_dx12_init_d3d(bool for_reset);
void internal_ff_dx12_destroy_d3d(bool for_reset);

// Clears the simulated-failure flag set by ff_dx12_device_fatal_error, so that a successful reset
// makes the device usable again.
void internal_ff_dx12_clear_fatal_error(void);

// Releases (and rebuilds) the GPU objects inside the already-created shared allocators, leaving
// their buffer/bucket structure and offsets intact so outstanding ranges stay valid.
void internal_ff_dx12_allocators_before_reset(void);
bool internal_ff_dx12_allocators_reset(void);
