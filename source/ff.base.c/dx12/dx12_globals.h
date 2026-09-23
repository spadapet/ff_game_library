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

DXGI_QUERY_VIDEO_MEMORY_INFO ff_dx12_video_memory_info(void);
void ff_dx12_update_video_memory_info(void);

ff_string_view ff_dx12_adapter_name(IDXGIAdapter3* adapter, ff_arena* arena);
