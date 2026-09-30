#include "pch.h"
#include "base/assert.h"
#include "base/hash.h"
#include "base/log.h"
#include "base/math.h"
#include "base/string.h"
#include "base/string_builder.h"
#include "data/file.h"
#include "dx12/dx12_device_child.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_object_cache.h"
#include "dx12/dx12_shader.h"
#include "windows/module.h"

typedef struct object_cache_key_builder
{
    ff_arena arena;
    uint8_t* data;
    size_t size;
    size_t capacity;
} object_cache_key_builder;

static void key_builder_init(object_cache_key_builder* key)
{
    *key = (object_cache_key_builder){ 0 };
    ff_arena_init_heap_local(&key->arena, 1024);
}

static void key_builder_destroy(object_cache_key_builder* key)
{
    ff_arena_destroy(&key->arena);
    *key = (object_cache_key_builder){ 0 };
}

static bool key_builder_reserve(object_cache_key_builder* key, size_t size)
{
    FF_CHECK_RET_VAL(size >= key->size, false);

    if (size <= key->capacity)
    {
        return true;
    }

    size_t new_capacity = key->capacity ? key->capacity * 2 : 1024;

    while (new_capacity < size)
    {
        new_capacity *= 2;
    }

    uint8_t* data = (uint8_t*)ff_arena_realloc(&key->arena, key->data,
        key->capacity, new_capacity, alignof(uint8_t));
    FF_CHECK_RET_VAL(data, false);

    key->data = data;
    key->capacity = new_capacity;
    return true;
}

static bool key_append_bytes(object_cache_key_builder* key, const void* data, size_t size)
{
    FF_CHECK_RET_VAL(data || !size, false);
    FF_CHECK_RET_VAL(key_builder_reserve(key, key->size + size), false);

    if (size)
    {
        memcpy(key->data + key->size, data, size);
        key->size += size;
    }

    return true;
}

static bool key_append_data(object_cache_key_builder* key, const void* data, size_t size)
{
    FF_CHECK_RET_VAL(key_append_bytes(key, &size, sizeof(size)), false);
    return key_append_bytes(key, data, size);
}

#define KEY_APPEND_VALUE(key, value) key_append_bytes((key), &(value), sizeof(value))

static bool key_append_sz(object_cache_key_builder* key, LPCSTR str)
{
    return key_append_data(key, str, str ? strlen(str) : 0);
}

static bool key_append_shader(object_cache_key_builder* key, const D3D12_SHADER_BYTECODE* shader)
{
    return key_append_data(key, shader->pShaderBytecode, shader->BytecodeLength);
}

static bool key_append_input_layout(object_cache_key_builder* key, const D3D12_INPUT_LAYOUT_DESC* layout)
{
    FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, layout->NumElements), false);

    for (UINT i = 0; i < layout->NumElements; i++)
    {
        const D3D12_INPUT_ELEMENT_DESC* desc = &layout->pInputElementDescs[i];
        FF_CHECK_RET_VAL(key_append_sz(key, desc->SemanticName), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->SemanticIndex), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->Format), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->InputSlot), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->AlignedByteOffset), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->InputSlotClass), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->InstanceDataStepRate), false);
    }

    return true;
}

static bool key_append_stream_output(object_cache_key_builder* key, const D3D12_STREAM_OUTPUT_DESC* desc)
{
    FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->NumEntries), false);

    for (UINT i = 0; i < desc->NumEntries; i++)
    {
        const D3D12_SO_DECLARATION_ENTRY* entry = &desc->pSODeclaration[i];
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, entry->Stream), false);
        FF_CHECK_RET_VAL(key_append_sz(key, entry->SemanticName), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, entry->SemanticIndex), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, entry->StartComponent), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, entry->ComponentCount), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, entry->OutputSlot), false);
    }

    FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->NumStrides), false);

    for (UINT i = 0; i < desc->NumStrides; i++)
    {
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->pBufferStrides[i]), false);
    }

    return KEY_APPEND_VALUE(key, desc->RasterizedStream);
}

static bool key_append_blend(object_cache_key_builder* key, const D3D12_BLEND_DESC* desc, UINT render_target_count)
{
    FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->AlphaToCoverageEnable), false);
    FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->IndependentBlendEnable), false);
    FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, render_target_count), false);

    for (UINT i = 0; i < render_target_count; i++)
    {
        const D3D12_RENDER_TARGET_BLEND_DESC* rt = &desc->RenderTarget[i];
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, rt->BlendEnable), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, rt->LogicOpEnable), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, rt->SrcBlend), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, rt->DestBlend), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, rt->BlendOp), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, rt->SrcBlendAlpha), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, rt->DestBlendAlpha), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, rt->BlendOpAlpha), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, rt->LogicOp), false);
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, rt->RenderTargetWriteMask), false);
    }

    return true;
}

static ff_dx12_object_cache_entry* bucket_find_by_object(ff_dx12_object_cache_entry** buckets, IUnknown* object)
{
    for (size_t i = 0; i < FF_DX12_OBJECT_CACHE_BUCKETS; i++)
    {
        for (ff_dx12_object_cache_entry* entry = buckets[i]; entry; entry = entry->next)
        {
            if (entry->object == object)
            {
                return entry;
            }
        }
    }

    return NULL;
}

static bool build_pipeline_state_key(ff_dx12_object_cache* cache,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc, object_cache_key_builder* key)
{
    ff_dx12_object_cache_entry* root_signature = bucket_find_by_object(cache->root_signatures, (IUnknown*)desc->pRootSignature);
    FF_CHECK_RET_VAL(root_signature, false);
    FF_CHECK_RET_VAL(key_append_data(key, root_signature->key, root_signature->key_size), false);
    FF_CHECK_RET_VAL(key_append_shader(key, &desc->VS), false);
    FF_CHECK_RET_VAL(key_append_shader(key, &desc->PS), false);
    FF_CHECK_RET_VAL(key_append_shader(key, &desc->DS), false);
    FF_CHECK_RET_VAL(key_append_shader(key, &desc->HS), false);
    FF_CHECK_RET_VAL(key_append_shader(key, &desc->GS), false);
    FF_CHECK_RET_VAL(key_append_stream_output(key, &desc->StreamOutput), false);
    FF_CHECK_RET_VAL(key_append_blend(key, &desc->BlendState, desc->NumRenderTargets), false);
    FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->SampleMask), false);
    FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->RasterizerState), false);
    FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->DepthStencilState), false);
    FF_CHECK_RET_VAL(key_append_input_layout(key, &desc->InputLayout), false);
    FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->IBStripCutValue), false);
    FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->PrimitiveTopologyType), false);
    FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->NumRenderTargets), false);

    for (UINT i = 0; i < desc->NumRenderTargets; i++)
    {
        FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->RTVFormats[i]), false);
    }

    FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->DSVFormat), false);
    FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->SampleDesc), false);
    FF_CHECK_RET_VAL(KEY_APPEND_VALUE(key, desc->NodeMask), false);
    FF_CHECK_RET_VAL(key_append_data(key, desc->CachedPSO.pCachedBlob, desc->CachedPSO.CachedBlobSizeInBytes), false);
    return KEY_APPEND_VALUE(key, desc->Flags);
}

static ff_dx12_object_cache_entry* bucket_find(ff_dx12_object_cache_entry** buckets,
    uint64_t hash, const void* key, size_t key_size)
{
    for (ff_dx12_object_cache_entry* entry = buckets[hash % FF_DX12_OBJECT_CACHE_BUCKETS]; entry; entry = entry->next)
    {
        if (entry->hash == hash && entry->key_size == key_size && !memcmp(entry->key, key, key_size))
        {
            return entry;
        }
    }

    return NULL;
}

// Takes ownership of the caller's reference on 'object'.
static bool bucket_add(ff_dx12_object_cache* cache, ff_dx12_object_cache_entry** buckets,
    uint64_t hash, const void* key, size_t key_size, IUnknown* object)
{
    ff_dx12_object_cache_entry* entry = cache->entries_free;
    if (entry)
    {
        cache->entries_free = entry->next;
    }
    else
    {
        entry = ff_arena_alloc_type(&cache->arena, ff_dx12_object_cache_entry, 1);
        if (entry)
        {
            *entry = (ff_dx12_object_cache_entry){ 0 };
        }
    }

    uint8_t* key_copy = entry ? entry->key : NULL;
    if (entry && key_size > entry->key_capacity)
    {
        key_copy = ff_arena_alloc_type(&cache->arena, uint8_t, key_size);
    }

    if (!entry || !key_copy)
    {
        if (entry)
        {
            entry->key_size = 0;
            entry->object = NULL;
            entry->next = cache->entries_free;
            cache->entries_free = entry;
        }

        IUnknown_Release(object);
        FF_DEBUG_FAIL_RET_VAL(false);
    }

    memcpy(key_copy, key, key_size);

    entry->hash = hash;
    entry->key = key_copy;
    entry->key_size = key_size;
    entry->key_capacity = ff_math_max_size(entry->key_capacity, key_size);
    entry->object = object;
    entry->next = buckets[hash % FF_DX12_OBJECT_CACHE_BUCKETS];
    buckets[hash % FF_DX12_OBJECT_CACHE_BUCKETS] = entry;
    return true;
}

static void buckets_release(ff_dx12_object_cache* cache, ff_dx12_object_cache_entry** buckets, bool release_object)
{
    for (size_t i = 0; i < FF_DX12_OBJECT_CACHE_BUCKETS; i++)
    {
        for (ff_dx12_object_cache_entry* entry = buckets[i]; entry; )
        {
            ff_dx12_object_cache_entry* next = entry->next;

            if (release_object && entry->object)
            {
                IUnknown_Release(entry->object);
            }

            entry->key_size = 0;
            entry->object = NULL;
            entry->next = cache->entries_free;
            cache->entries_free = entry;
            entry = next;
        }

        buckets[i] = NULL;
    }
}

void ff_dx12_object_cache_init(ff_dx12_object_cache* cache)
{
    FF_ASSERT_RET(cache);

    *cache = (ff_dx12_object_cache){ 0 };
    ff_arena_init_heap_local(&cache->arena, 4096);
    ff_dx12_add_device_child(&cache->device_child, cache, ff_dx12_device_child_type_object_cache);
}

void ff_dx12_object_cache_destroy(ff_dx12_object_cache* cache)
{
    FF_CHECK_RET(cache);

    ff_dx12_remove_device_child(&cache->device_child);

    buckets_release(cache, cache->root_signatures, true);
    buckets_release(cache, cache->pipeline_states, true);

    for (size_t i = 0; i < ff_dx12_shader_count; i++)
    {
        ff_file_map_destroy(&cache->shaders[i]);
        cache->shaders_tried[i] = false;
    }

    cache->entries_free = NULL;
    ff_arena_destroy(&cache->arena);
}

D3D12_SHADER_BYTECODE ff_dx12_object_cache_shader(ff_dx12_object_cache* cache, ff_dx12_shader shader)
{
    const D3D12_SHADER_BYTECODE empty = { 0 };
    FF_ASSERT_RET_VAL(cache, empty);
    FF_ASSERT_RET_VAL(shader >= 0 && shader < ff_dx12_shader_count, empty);

    if (!cache->shaders_tried[shader])
    {
        // Marked before the load so a missing or unreadable .cso is reported once rather than
        // hitting the disk again on every pipeline state that asks for it.
        cache->shaders_tried[shader] = true;

        ff_arena_declare_stack(temp_arena, 1024);
        // The shaders ship next to the binary that links this library, which is not the process
        // executable when that binary is a DLL loaded by a host process such as a test runner.
        const ff_string_view dir = ff_file_module_dir(ff_module_instance(), &temp_arena);
        const ff_string_view name = ff_dx12_shader_name(shader);

        ff_string_builder sb;
        ff_string_builder_init(&sb, &temp_arena);
        ff_string_builder_append(&sb, dir);
        ff_string_builder_append(&sb, FF_SVL("shaders\\"));
        ff_string_builder_append(&sb, name);
        ff_string_builder_append(&sb, FF_SVL(".cso"));

        const ff_string_view path = ff_string_builder_copy_to(&sb, &temp_arena);

        if (!ff_file_map_init(&cache->shaders[shader], path))
        {
            ff_log_write(ff_log_type_debug, FF_SVL("[dx12] Failed to load shader: %.*s"), FF_SV_FORMAT(path));
            FF_DEBUG_FAIL_MSG("shader .cso is missing or unreadable");
        }

        ff_arena_destroy(&temp_arena);
    }

    // No assert here: a failed load already reported itself once above, and every pipeline state
    // asking for the same missing shader afterward should not repeat it.
    const ff_span data = ff_file_map_data(&cache->shaders[shader]);
    FF_CHECK_RET_VAL(data.data && data.size, empty);

    D3D12_SHADER_BYTECODE bytecode;
    bytecode.pShaderBytecode = data.data;
    bytecode.BytecodeLength = data.size;
    return bytecode;
}

size_t ff_dx12_object_cache_shader_count(const ff_dx12_object_cache* cache)
{
    FF_ASSERT_RET_VAL(cache, 0);

    size_t count = 0;

    for (size_t i = 0; i < ff_dx12_shader_count; i++)
    {
        if (cache->shaders[i].base)
        {
            count++;
        }
    }

    return count;
}

size_t ff_dx12_object_cache_size(const ff_dx12_object_cache* cache)
{
    FF_ASSERT_RET_VAL(cache, 0);

    size_t count = 0;

    for (size_t i = 0; i < FF_DX12_OBJECT_CACHE_BUCKETS; i++)
    {
        for (const ff_dx12_object_cache_entry* entry = cache->root_signatures[i]; entry; entry = entry->next)
        {
            count++;
        }

        for (const ff_dx12_object_cache_entry* entry = cache->pipeline_states[i]; entry; entry = entry->next)
        {
            count++;
        }
    }

    return count;
}

void internal_ff_dx12_object_cache_before_reset(ff_dx12_object_cache* cache)
{
    FF_CHECK_RET(cache);

    // The freed entries go back on entries_free rather than to the arena, so the arena never grows
    // across a reset even though every bucket is emptied.
    buckets_release(cache, cache->root_signatures, true);
    buckets_release(cache, cache->pipeline_states, true);
}

ID3D12RootSignature* ff_dx12_object_cache_root_signature(ff_dx12_object_cache* cache, const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* desc)
{
    FF_ASSERT_RET_VAL(cache && desc, NULL);

    ID3DBlob* data = NULL;
    ID3DBlob* errors = NULL;
    HRESULT hr = D3D12SerializeVersionedRootSignature(desc, &data, &errors);

    ID3D12RootSignature* result = NULL;

    if (SUCCEEDED(hr) && data && ID3D10Blob_GetBufferSize(data))
    {
        const void* key = ID3D10Blob_GetBufferPointer(data);
        const size_t key_size = ID3D10Blob_GetBufferSize(data);
        const uint64_t hash = ff_hash_bytes(key, key_size);
        ff_dx12_object_cache_entry* entry = bucket_find(cache->root_signatures, hash, key, key_size);

        if (entry)
        {
            result = (ID3D12RootSignature*)entry->object;
        }
        else
        {
            HRESULT hr_create = ID3D12Device6_CreateRootSignature(ff_dx12_device(), 0,
                key, key_size, &IID_ID3D12RootSignature, (void**)&result);

            if (FAILED(hr_create))
            {
                FF_ASSERT_HR(hr_create);
            }
            else if (!bucket_add(cache, cache->root_signatures, hash, key, key_size, (IUnknown*)result))
            {
                result = NULL;
            }
        }
    }

    if (!result && errors)
    {
        ff_log_write(ff_log_type_debug, FF_SVL("[dx12] Root signature serialization error: %.*s"),
            (int)(ID3D10Blob_GetBufferSize(errors) ? ID3D10Blob_GetBufferSize(errors) - 1 : 0),
            (const char*)ID3D10Blob_GetBufferPointer(errors));
    }

    if (data)
    {
        ID3D10Blob_Release(data);
    }

    if (errors)
    {
        ID3D10Blob_Release(errors);
    }

    FF_ASSERT(result);
    return result;
}

uint64_t ff_dx12_object_cache_root_signature_hash(ff_dx12_object_cache* cache, ID3D12RootSignature* root_signature)
{
    FF_ASSERT_RET_VAL(cache, 0);
    FF_CHECK_RET_VAL(root_signature, 0);

    ff_dx12_object_cache_entry* entry = bucket_find_by_object(cache->root_signatures, (IUnknown*)root_signature);
    FF_CHECK_RET_VAL(entry, 0);
    return entry->hash;
}

uint64_t ff_dx12_object_cache_pipeline_state_hash(ff_dx12_object_cache* cache, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc)
{
    FF_ASSERT_RET_VAL(cache && desc, 0);

    object_cache_key_builder key;
    key_builder_init(&key);

    const uint64_t hash = build_pipeline_state_key(cache, desc, &key)
        ? ff_hash_bytes(key.data, key.size)
        : 0;

    key_builder_destroy(&key);
    return hash;
}

ID3D12PipelineState* ff_dx12_object_cache_pipeline_state(ff_dx12_object_cache* cache, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc)
{
    FF_ASSERT_RET_VAL(cache && desc, NULL);

    object_cache_key_builder key;
    key_builder_init(&key);

    if (!build_pipeline_state_key(cache, desc, &key))
    {
        key_builder_destroy(&key);
        return NULL;
    }

    const uint64_t hash = ff_hash_bytes(key.data, key.size);
    ff_dx12_object_cache_entry* entry = bucket_find(cache->pipeline_states, hash, key.data, key.size);

    if (entry)
    {
        key_builder_destroy(&key);
        return (ID3D12PipelineState*)entry->object;
    }

    ID3D12PipelineState* state = NULL;
    HRESULT hr = ID3D12Device6_CreateGraphicsPipelineState(ff_dx12_device(), desc,
        &IID_ID3D12PipelineState, (void**)&state);

    if (FAILED(hr))
    {
        key_builder_destroy(&key);
        FF_ASSERT_HR_RET_VAL(hr, NULL);
    }

    if (!bucket_add(cache, cache->pipeline_states, hash, key.data, key.size, (IUnknown*)state))
    {
        key_builder_destroy(&key);
        return NULL;
    }

    key_builder_destroy(&key);
    return state;
}
