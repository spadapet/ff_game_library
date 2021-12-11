#include "pch.h"
#include "device_reset_priority.h"
#include "globals.h"
#include "object_cache.h"

static size_t stable_hash(const D3D12_SHADER_BYTECODE& shader)
{
    return shader.BytecodeLength && shader.pShaderBytecode
        ? ff::stable_hash_bytes(shader.pShaderBytecode, shader.BytecodeLength)
        : 0;
}

static size_t stable_hash(const D3D12_CACHED_PIPELINE_STATE& state)
{
    return state.CachedBlobSizeInBytes && state.pCachedBlob
        ? ff::stable_hash_bytes(state.pCachedBlob, state.CachedBlobSizeInBytes)
        : 0;
}

static size_t stable_hash(const D3D12_INPUT_ELEMENT_DESC& desc)
{
    return
        ff::stable_hash_bytes(desc.SemanticName, std::strlen(desc.SemanticName)) ^
        ff::stable_hash_func(desc.SemanticIndex) ^
        ff::stable_hash_func(desc.Format) ^
        ff::stable_hash_func(desc.InputSlot) ^
        ff::stable_hash_func(desc.AlignedByteOffset) ^
        ff::stable_hash_func(desc.InputSlotClass) ^
        ff::stable_hash_func(desc.InstanceDataStepRate);
}

static size_t stable_hash(const D3D12_INPUT_LAYOUT_DESC& layout)
{
    size_t hash = 0;

    for (size_t i = 0; i < layout.NumElements; i++)
    {
        hash ^= ::stable_hash(layout.pInputElementDescs[i]);
    }

    return hash;
}

static size_t stable_hash(const D3D12_SO_DECLARATION_ENTRY& desc)
{
    return
        ff::stable_hash_func(desc.Stream) ^
        ff::stable_hash_bytes(desc.SemanticName, std::strlen(desc.SemanticName)) ^
        ff::stable_hash_func(desc.SemanticIndex) ^
        ff::stable_hash_func(desc.StartComponent) ^
        ff::stable_hash_func(desc.ComponentCount) ^
        ff::stable_hash_func(desc.OutputSlot);
}

static size_t stable_hash(const D3D12_STREAM_OUTPUT_DESC& desc)
{
    size_t hash = ff::stable_hash_func(desc.RasterizedStream);

    for (size_t i = 0; i < desc.NumEntries; i++)
    {
        hash ^= ::stable_hash(desc.pSODeclaration[i]);
    }

    for (size_t i = 0; i < desc.NumStrides; i++)
    {
        hash ^= ff::stable_hash_func(desc.pBufferStrides[i]);
    }

    return hash;
}

static size_t stable_hash(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
{
    return
        ::stable_hash(desc.VS) ^
        ::stable_hash(desc.PS) ^
        ::stable_hash(desc.DS) ^
        ::stable_hash(desc.HS) ^
        ::stable_hash(desc.GS) ^
        ::stable_hash(desc.InputLayout) ^
        ::stable_hash(desc.CachedPSO) ^
        ff::stable_hash_func(desc.BlendState) ^
        ff::stable_hash_func(desc.SampleMask) ^
        ff::stable_hash_func(desc.RasterizerState) ^
        ff::stable_hash_func(desc.DepthStencilState) ^
        ff::stable_hash_func(desc.IBStripCutValue) ^
        ff::stable_hash_func(desc.PrimitiveTopologyType) ^
        ff::stable_hash_func(desc.NumRenderTargets) ^
        ff::stable_hash_func(desc.RTVFormats) ^
        ff::stable_hash_func(desc.DSVFormat) ^
        ff::stable_hash_func(desc.SampleDesc) ^
        ff::stable_hash_func(desc.NodeMask) ^
        ff::stable_hash_func(desc.Flags);
}

ff::dx12::object_cache::object_cache()
{
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::object_cache::~object_cache()
{
    ff::dx12::remove_device_child(this);
}

ID3D12RootSignature* ff::dx12::object_cache::root_signature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& desc)
{
    Microsoft::WRL::ComPtr<ID3DBlob> data, errors;
    if (SUCCEEDED(::D3D12SerializeVersionedRootSignature(&desc, data.GetAddressOf(), errors.GetAddressOf())) && data && data->GetBufferSize())
    {
        size_t hash = ff::stable_hash_bytes(data->GetBufferPointer(), data->GetBufferSize());
        auto i = this->root_signatures.find(hash);
        if (i == this->root_signatures.end())
        {
            Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
            if (SUCCEEDED(ff::dx12::device()->CreateRootSignature(0, data->GetBufferPointer(), data->GetBufferSize(), IID_PPV_ARGS(&root_signature))))
            {
                i = this->root_signatures.try_emplace(hash, std::move(root_signature)).first;
            }
        }

        if (i != this->root_signatures.end())
        {
            return i->second.Get();
        }
    }

    assert(false);
    return nullptr;
}

ID3D12PipelineStateX* ff::dx12::object_cache::pipeline_state(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
{
    size_t hash = ::stable_hash(desc);

    auto i = this->pipeline_states.find(hash);
    if (i == this->pipeline_states.end())
    {
        Microsoft::WRL::ComPtr<ID3D12PipelineStateX> state;
        if (SUCCEEDED(ff::dx12::device()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&state))))
        {
            i = this->pipeline_states.try_emplace(hash, std::move(state)).first;
        }
    }

    if (i != this->pipeline_states.end())
    {
        return i->second.Get();
    }

    assert(false);
    return nullptr;
}

std::shared_ptr<ff::data_base> ff::dx12::object_cache::shader(const std::string& name)
{
    auto i = this->shaders.find(name);
    if (i == this->shaders.end())
    {
        ff::auto_resource<ff::resource_file> res(name);
        if (res.object() && res.object()->loaded_data())
        {
            i = this->shaders.try_emplace(name, res.object()->loaded_data()).first;
        }
    }

    assert(i != this->shaders.end());
    return (i != this->shaders.end()) ? i->second : std::shared_ptr<ff::data_base>();
}

void ff::dx12::object_cache::before_reset()
{
    this->root_signatures.clear();
    this->pipeline_states.clear();
}

bool ff::dx12::object_cache::reset()
{
    return true;
}
