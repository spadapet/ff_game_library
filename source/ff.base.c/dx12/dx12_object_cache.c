#include "pch.h"
#include "base/assert.h"
#include "base/hash.h"
#include "base/log.h"
#include "base/string.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_object_cache.h"

static void hash_bytes(ff_hash_data* hash, const void* data, size_t size)
{
    if (data && size)
    {
        ff_hash(hash, data, size);
    }
}

static void hash_sz(ff_hash_data* hash, LPCSTR str)
{
    hash_bytes(hash, str, str ? strlen(str) : 0);
}

static void hash_shader(ff_hash_data* hash, const D3D12_SHADER_BYTECODE* shader)
{
    hash_bytes(hash, shader->pShaderBytecode, shader->BytecodeLength);
}

static void hash_input_layout(ff_hash_data* hash, const D3D12_INPUT_LAYOUT_DESC* layout)
{
    for (UINT i = 0; i < layout->NumElements; i++)
    {
        const D3D12_INPUT_ELEMENT_DESC* desc = &layout->pInputElementDescs[i];
        hash_sz(hash, desc->SemanticName);
        ff_hash(hash, &desc->SemanticIndex, sizeof(desc->SemanticIndex));
        ff_hash(hash, &desc->Format, sizeof(desc->Format));
        ff_hash(hash, &desc->InputSlot, sizeof(desc->InputSlot));
        ff_hash(hash, &desc->AlignedByteOffset, sizeof(desc->AlignedByteOffset));
        ff_hash(hash, &desc->InputSlotClass, sizeof(desc->InputSlotClass));
        ff_hash(hash, &desc->InstanceDataStepRate, sizeof(desc->InstanceDataStepRate));
    }
}

static void hash_stream_output(ff_hash_data* hash, const D3D12_STREAM_OUTPUT_DESC* desc)
{
    for (UINT i = 0; i < desc->NumEntries; i++)
    {
        const D3D12_SO_DECLARATION_ENTRY* entry = &desc->pSODeclaration[i];
        ff_hash(hash, &entry->Stream, sizeof(entry->Stream));
        hash_sz(hash, entry->SemanticName);
        ff_hash(hash, &entry->SemanticIndex, sizeof(entry->SemanticIndex));
        ff_hash(hash, &entry->StartComponent, sizeof(entry->StartComponent));
        ff_hash(hash, &entry->ComponentCount, sizeof(entry->ComponentCount));
        ff_hash(hash, &entry->OutputSlot, sizeof(entry->OutputSlot));
    }

    for (UINT i = 0; i < desc->NumStrides; i++)
    {
        ff_hash(hash, &desc->pBufferStrides[i], sizeof(desc->pBufferStrides[i]));
    }

    ff_hash(hash, &desc->RasterizedStream, sizeof(desc->RasterizedStream));
}

static void hash_blend(ff_hash_data* hash, const D3D12_BLEND_DESC* desc, size_t render_target_size)
{
    ff_hash(hash, &desc->AlphaToCoverageEnable, sizeof(desc->AlphaToCoverageEnable));
    ff_hash(hash, &desc->IndependentBlendEnable, sizeof(desc->IndependentBlendEnable));

    for (size_t i = 0; i < render_target_size; i++)
    {
        const D3D12_RENDER_TARGET_BLEND_DESC* rt = &desc->RenderTarget[i];
        ff_hash(hash, &rt->BlendEnable, sizeof(rt->BlendEnable));
        ff_hash(hash, &rt->LogicOpEnable, sizeof(rt->LogicOpEnable));
        ff_hash(hash, &rt->SrcBlend, sizeof(rt->SrcBlend));
        ff_hash(hash, &rt->DestBlend, sizeof(rt->DestBlend));
        ff_hash(hash, &rt->BlendOp, sizeof(rt->BlendOp));
        ff_hash(hash, &rt->SrcBlendAlpha, sizeof(rt->SrcBlendAlpha));
        ff_hash(hash, &rt->DestBlendAlpha, sizeof(rt->DestBlendAlpha));
        ff_hash(hash, &rt->BlendOpAlpha, sizeof(rt->BlendOpAlpha));
        ff_hash(hash, &rt->LogicOp, sizeof(rt->LogicOp));
        ff_hash(hash, &rt->RenderTargetWriteMask, sizeof(rt->RenderTargetWriteMask));
    }
}

static uint64_t hash_pipeline_state_desc(const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc, uint64_t root_signature_hash)
{
    ff_hash_data hash;
    ff_hash_init(&hash);

    ff_hash(&hash, &root_signature_hash, sizeof(root_signature_hash));
    hash_shader(&hash, &desc->VS);
    hash_shader(&hash, &desc->PS);
    hash_shader(&hash, &desc->DS);
    hash_shader(&hash, &desc->HS);
    hash_shader(&hash, &desc->GS);
    hash_stream_output(&hash, &desc->StreamOutput);
    hash_blend(&hash, &desc->BlendState, desc->NumRenderTargets);
    ff_hash(&hash, &desc->SampleMask, sizeof(desc->SampleMask));
    ff_hash(&hash, &desc->RasterizerState, sizeof(desc->RasterizerState));
    ff_hash(&hash, &desc->DepthStencilState, sizeof(desc->DepthStencilState));
    hash_input_layout(&hash, &desc->InputLayout);
    ff_hash(&hash, &desc->IBStripCutValue, sizeof(desc->IBStripCutValue));
    ff_hash(&hash, &desc->PrimitiveTopologyType, sizeof(desc->PrimitiveTopologyType));
    ff_hash(&hash, &desc->NumRenderTargets, sizeof(desc->NumRenderTargets));

    for (UINT i = 0; i < desc->NumRenderTargets; i++)
    {
        ff_hash(&hash, &desc->RTVFormats[i], sizeof(desc->RTVFormats[i]));
    }

    ff_hash(&hash, &desc->DSVFormat, sizeof(desc->DSVFormat));
    ff_hash(&hash, &desc->SampleDesc, sizeof(desc->SampleDesc));
    ff_hash(&hash, &desc->NodeMask, sizeof(desc->NodeMask));
    hash_bytes(&hash, desc->CachedPSO.pCachedBlob, desc->CachedPSO.CachedBlobSizeInBytes);
    ff_hash(&hash, &desc->Flags, sizeof(desc->Flags));

    return ff_hash_done(&hash);
}

static ff_dx12_object_cache_entry* bucket_find(ff_dx12_object_cache_entry** buckets, uint64_t hash)
{
    for (ff_dx12_object_cache_entry* entry = buckets[hash % FF_DX12_OBJECT_CACHE_BUCKETS]; entry; entry = entry->next)
    {
        if (entry->hash == hash)
        {
            return entry;
        }
    }

    return NULL;
}

// Takes ownership of the caller's reference on 'object'.
static void bucket_add(ff_dx12_object_cache* cache, ff_dx12_object_cache_entry** buckets, uint64_t hash, IUnknown* object)
{
    ff_dx12_object_cache_entry* entry = cache->entries_free;
    if (entry)
    {
        cache->entries_free = entry->next;
    }
    else
    {
        entry = ff_arena_alloc_type(&cache->arena, ff_dx12_object_cache_entry, 1);
    }

    if (!entry)
    {
        IUnknown_Release(object);
        FF_DEBUG_FAIL_RET();
    }

    entry->hash = hash;
    entry->object = object;
    entry->next = buckets[hash % FF_DX12_OBJECT_CACHE_BUCKETS];
    buckets[hash % FF_DX12_OBJECT_CACHE_BUCKETS] = entry;
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
}

void ff_dx12_object_cache_destroy(ff_dx12_object_cache* cache)
{
    FF_CHECK_RET(cache);

    buckets_release(cache, cache->root_signatures, true);
    buckets_release(cache, cache->pipeline_states, true);

    cache->entries_free = NULL;
    ff_arena_destroy(&cache->arena);
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
        const uint64_t hash = ff_hash_bytes(ID3D10Blob_GetBufferPointer(data), ID3D10Blob_GetBufferSize(data));
        ff_dx12_object_cache_entry* entry = bucket_find(cache->root_signatures, hash);

        if (entry)
        {
            result = (ID3D12RootSignature*)entry->object;
        }
        else if (SUCCEEDED(ID3D12Device6_CreateRootSignature(ff_dx12_device(), 0,
            ID3D10Blob_GetBufferPointer(data), ID3D10Blob_GetBufferSize(data), &IID_ID3D12RootSignature, (void**)&result)))
        {
            bucket_add(cache, cache->root_signatures, hash, (IUnknown*)result);
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

    for (size_t i = 0; i < FF_DX12_OBJECT_CACHE_BUCKETS; i++)
    {
        for (ff_dx12_object_cache_entry* entry = cache->root_signatures[i]; entry; entry = entry->next)
        {
            if ((ID3D12RootSignature*)entry->object == root_signature)
            {
                return entry->hash;
            }
        }
    }

    FF_DEBUG_FAIL();
    return 0;
}

uint64_t ff_dx12_object_cache_pipeline_state_hash(ff_dx12_object_cache* cache, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc)
{
    FF_ASSERT_RET_VAL(cache && desc, 0);
    return hash_pipeline_state_desc(desc, ff_dx12_object_cache_root_signature_hash(cache, desc->pRootSignature));
}

ID3D12PipelineState* ff_dx12_object_cache_pipeline_state(ff_dx12_object_cache* cache, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc)
{
    FF_ASSERT_RET_VAL(cache && desc, NULL);

    const uint64_t hash = ff_dx12_object_cache_pipeline_state_hash(cache, desc);
    ff_dx12_object_cache_entry* entry = bucket_find(cache->pipeline_states, hash);

    if (entry)
    {
        return (ID3D12PipelineState*)entry->object;
    }

    ID3D12PipelineState* state = NULL;
    FF_ASSERT_HR_RET_VAL(ID3D12Device6_CreateGraphicsPipelineState(ff_dx12_device(), desc,
        &IID_ID3D12PipelineState, (void**)&state), NULL);

    bucket_add(cache, cache->pipeline_states, hash, (IUnknown*)state);
    return state;
}
