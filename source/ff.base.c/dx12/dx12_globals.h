#pragma once

#include "../base/assert.h"
#include "../base/string.h"

typedef struct ff_dx12_queue ff_dx12_queue;
typedef struct ff_dx12_mem_allocator ff_dx12_mem_allocator;
typedef struct ff_dx12_cpu_descriptor_allocator ff_dx12_cpu_descriptor_allocator;
typedef struct ff_dx12_gpu_descriptor_allocator ff_dx12_gpu_descriptor_allocator;
typedef struct ff_dx12_target_window ff_dx12_target_window;

typedef struct ff_dx12_init_params
{
    DXGI_GPU_PREFERENCE gpu_preference;
    D3D_FEATURE_LEVEL feature_level;
} ff_dx12_init_params;

ff_dx12_init_params ff_dx12_init_params_default(void);
bool ff_dx12_init(const ff_dx12_init_params* params);
void ff_dx12_destroy(void);
bool ff_dx12_on_owner_thread(void);
static inline void FF_DX12_ASSERT_OWNER() { FF_ASSERT(ff_dx12_on_owner_thread()); }

IDXGIFactory6* ff_dx12_factory(void);
IDXGIAdapter3* ff_dx12_adapter(void);
ID3D12Device6* ff_dx12_device(void);
D3D_FEATURE_LEVEL ff_dx12_feature_level(void);

bool ff_dx12_device_valid(void);
void ff_dx12_device_fatal_error(ff_string_view reason);
bool ff_dx12_supports_create_heap_not_resident(void);
DXGI_QUERY_VIDEO_MEMORY_INFO ff_dx12_video_memory_info(void);

void ff_dx12_wait_for_idle(void);
bool ff_dx12_prepare_render(void);

ff_dx12_mem_allocator* ff_dx12_upload_allocator(void);
ff_dx12_mem_allocator* ff_dx12_readback_allocator(void);
ff_dx12_mem_allocator* ff_dx12_dynamic_buffer_allocator(void);
ff_dx12_mem_allocator* ff_dx12_static_buffer_allocator(void);
ff_dx12_mem_allocator* ff_dx12_texture_allocator(void);
ff_dx12_mem_allocator* ff_dx12_target_allocator(void);

ff_dx12_cpu_descriptor_allocator* ff_dx12_cpu_buffer_descriptors(void);
ff_dx12_cpu_descriptor_allocator* ff_dx12_cpu_sampler_descriptors(void);
ff_dx12_cpu_descriptor_allocator* ff_dx12_cpu_target_descriptors(void);
ff_dx12_cpu_descriptor_allocator* ff_dx12_cpu_depth_descriptors(void);

ff_dx12_gpu_descriptor_allocator* ff_dx12_gpu_view_descriptors(void);
ff_dx12_gpu_descriptor_allocator* ff_dx12_gpu_sampler_descriptors(void);

ff_dx12_queue* ff_dx12_direct_queue(void);
ff_dx12_queue* ff_dx12_copy_queue(void);
ff_dx12_queue* ff_dx12_compute_queue(void);
ff_dx12_queue* ff_dx12_queue_from_type(D3D12_COMMAND_LIST_TYPE type);

void ff_dx12_defer_resize_target(ff_dx12_target_window* target, size_t width, size_t height);
void ff_dx12_cancel_deferred_target(ff_dx12_target_window* target);
void ff_dx12_defer_reset_device(bool force);
