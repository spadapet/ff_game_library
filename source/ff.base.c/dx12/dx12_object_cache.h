#pragma once

#include "../base/arena.h"
#include "dx12_device_child.h"

// Caches root signatures and pipeline states by a hash of their description, so that repeatedly
// asking for the same pipeline returns the same object instead of creating a duplicate.
//
// The old C++ version also persisted compiled pipelines to an on-disk ID3D12PipelineLibrary and
// resolved named shader blobs through the resource system. Both of those depend on shader
// delivery, so they land with that milestone; this is the in-memory half.
typedef struct ff_dx12_object_cache_entry
{
    struct ff_dx12_object_cache_entry* next;
    uint64_t hash;
    IUnknown* object;
} ff_dx12_object_cache_entry;

#define FF_DX12_OBJECT_CACHE_BUCKETS 64

typedef struct ff_dx12_object_cache
{
    ff_arena arena;
    ff_dx12_device_child device_child;
    ff_dx12_object_cache_entry* root_signatures[FF_DX12_OBJECT_CACHE_BUCKETS];
    ff_dx12_object_cache_entry* pipeline_states[FF_DX12_OBJECT_CACHE_BUCKETS];
    ff_dx12_object_cache_entry* entries_free;
} ff_dx12_object_cache;

void ff_dx12_object_cache_init(ff_dx12_object_cache* cache);
void ff_dx12_object_cache_destroy(ff_dx12_object_cache* cache);

// The cache keeps ownership; callers must not release the returned objects.
ID3D12RootSignature* ff_dx12_object_cache_root_signature(ff_dx12_object_cache* cache, const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* desc);
ID3D12PipelineState* ff_dx12_object_cache_pipeline_state(ff_dx12_object_cache* cache, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc);

uint64_t ff_dx12_object_cache_root_signature_hash(ff_dx12_object_cache* cache, ID3D12RootSignature* root_signature);
uint64_t ff_dx12_object_cache_pipeline_state_hash(ff_dx12_object_cache* cache, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc);

// Device reset: root signatures and pipeline states are owned by the device, so every cached
// object is released while the old device is still alive and the buckets are emptied. Nothing is
// rebuilt afterward. The cache is a pure memo of what callers happened to ask for, and the descs
// it was built from belong to those callers, so the entries are simply recreated on the next
// miss against the new device.
void internal_ff_dx12_object_cache_before_reset(ff_dx12_object_cache* cache);

// Number of cached objects, for callers (and tests) that need to see the cache actually empty.
size_t ff_dx12_object_cache_size(const ff_dx12_object_cache* cache);
