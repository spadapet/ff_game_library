#include "pch.h"
#include "device_reset_priority.h"
#include "globals.h"
#include "object_cache.h"

template<class T, class = std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>>>
static ff::stable_hash_data_t stable_hash(ff::stable_hash_data_t& hash, const T& data)
{
    return ff::stable_hash_incremental(&data, sizeof(T), hash);
}

static ff::stable_hash_data_t stable_hash(ff::stable_hash_data_t& hash, LPCSTR str)
{
    return ff::stable_hash_incremental(str, str ? std::strlen(str) : 0, hash);
}

static ff::stable_hash_data_t stable_hash(ff::stable_hash_data_t& hash, const D3D12_SHADER_BYTECODE& shader)
{
    return ff::stable_hash_incremental(shader.pShaderBytecode, shader.BytecodeLength, hash);
}

static ff::stable_hash_data_t stable_hash(ff::stable_hash_data_t& hash, const D3D12_CACHED_PIPELINE_STATE& state)
{
    return ff::stable_hash_incremental(state.pCachedBlob, state.CachedBlobSizeInBytes, hash);
}

static ff::stable_hash_data_t stable_hash(ff::stable_hash_data_t& hash, const D3D12_INPUT_ELEMENT_DESC& desc)
{
    hash = ::stable_hash(hash, desc.SemanticName);
    hash = ::stable_hash(hash, desc.SemanticIndex);
    hash = ::stable_hash(hash, desc.Format);
    hash = ::stable_hash(hash, desc.InputSlot);
    hash = ::stable_hash(hash, desc.AlignedByteOffset);
    hash = ::stable_hash(hash, desc.InputSlotClass);
    hash = ::stable_hash(hash, desc.InstanceDataStepRate);

    return hash;
}

static ff::stable_hash_data_t stable_hash(ff::stable_hash_data_t& hash, const D3D12_INPUT_LAYOUT_DESC& layout)
{
    for (size_t i = 0; i < layout.NumElements; i++)
    {
        hash = ::stable_hash(hash, layout.pInputElementDescs[i]);
    }

    return hash;
}

static ff::stable_hash_data_t stable_hash(ff::stable_hash_data_t& hash, const D3D12_SO_DECLARATION_ENTRY& desc)
{
    hash = ::stable_hash(hash, desc.Stream);
    hash = ::stable_hash(hash, desc.SemanticName);
    hash = ::stable_hash(hash, desc.SemanticIndex);
    hash = ::stable_hash(hash, desc.StartComponent);
    hash = ::stable_hash(hash, desc.ComponentCount);
    hash = ::stable_hash(hash, desc.OutputSlot);

    return hash;
}

static ff::stable_hash_data_t stable_hash(ff::stable_hash_data_t& hash, const D3D12_STREAM_OUTPUT_DESC& desc)
{
    for (size_t i = 0; i < desc.NumEntries; i++)
    {
        hash = ::stable_hash(hash, desc.pSODeclaration[i]);
    }

    for (size_t i = 0; i < desc.NumStrides; i++)
    {
        hash = ::stable_hash(hash, desc.pBufferStrides[i]);
    }

    hash = ::stable_hash(hash, desc.RasterizedStream);

    return hash;
}

static ff::stable_hash_data_t stable_hash(ff::stable_hash_data_t& hash, const D3D12_RENDER_TARGET_BLEND_DESC& desc)
{
    hash = ::stable_hash(hash, desc.BlendEnable);
    hash = ::stable_hash(hash, desc.LogicOpEnable);
    hash = ::stable_hash(hash, desc.SrcBlend);
    hash = ::stable_hash(hash, desc.DestBlend);
    hash = ::stable_hash(hash, desc.BlendOp);
    hash = ::stable_hash(hash, desc.SrcBlendAlpha);
    hash = ::stable_hash(hash, desc.DestBlendAlpha);
    hash = ::stable_hash(hash, desc.BlendOpAlpha);
    hash = ::stable_hash(hash, desc.LogicOp);
    hash = ::stable_hash(hash, desc.RenderTargetWriteMask);

    return hash;
}

static ff::stable_hash_data_t stable_hash(ff::stable_hash_data_t& hash, const D3D12_BLEND_DESC& desc, size_t render_target_size)
{
    hash = ::stable_hash(hash, desc.AlphaToCoverageEnable);
    hash = ::stable_hash(hash, desc.IndependentBlendEnable);

    for (size_t i = 0; i < render_target_size; i++)
    {
        hash = ::stable_hash(hash, desc.RenderTarget[i]);
    }

    return hash;
}

static ff::stable_hash_data_t stable_hash(ff::stable_hash_data_t& hash, const D3D12_RASTERIZER_DESC& desc)
{
    hash = ::stable_hash(hash, desc.FillMode);
    hash = ::stable_hash(hash, desc.CullMode);
    hash = ::stable_hash(hash, desc.FrontCounterClockwise);
    hash = ::stable_hash(hash, desc.DepthBias);
    hash = ::stable_hash(hash, desc.DepthBiasClamp);
    hash = ::stable_hash(hash, desc.SlopeScaledDepthBias);
    hash = ::stable_hash(hash, desc.DepthClipEnable);
    hash = ::stable_hash(hash, desc.MultisampleEnable);
    hash = ::stable_hash(hash, desc.AntialiasedLineEnable);
    hash = ::stable_hash(hash, desc.ForcedSampleCount);
    hash = ::stable_hash(hash, desc.ConservativeRaster);

    return hash;
}

static ff::stable_hash_data_t stable_hash(ff::stable_hash_data_t& hash, const D3D12_DEPTH_STENCILOP_DESC& desc)
{
    hash = ::stable_hash(hash, desc.StencilFailOp);
    hash = ::stable_hash(hash, desc.StencilDepthFailOp);
    hash = ::stable_hash(hash, desc.StencilPassOp);
    hash = ::stable_hash(hash, desc.StencilFunc);

    return hash;
}

static ff::stable_hash_data_t stable_hash(ff::stable_hash_data_t& hash, const D3D12_DEPTH_STENCIL_DESC& desc)
{
    hash = ::stable_hash(hash, desc.DepthEnable);
    hash = ::stable_hash(hash, desc.DepthWriteMask);
    hash = ::stable_hash(hash, desc.DepthFunc);
    hash = ::stable_hash(hash, desc.StencilEnable);
    hash = ::stable_hash(hash, desc.StencilReadMask);
    hash = ::stable_hash(hash, desc.StencilWriteMask);
    hash = ::stable_hash(hash, desc.FrontFace);
    hash = ::stable_hash(hash, desc.BackFace);

    return hash;
}

static ff::stable_hash_data_t stable_hash(ff::stable_hash_data_t& hash, const DXGI_SAMPLE_DESC& desc)
{
    hash = ::stable_hash(hash, desc.Count);
    hash = ::stable_hash(hash, desc.Quality);

    return hash;
}

static size_t stable_hash(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, size_t root_signature_hash)
{
    ff::stable_hash_data_t hash;

    hash = ::stable_hash(hash, root_signature_hash);
    hash = ::stable_hash(hash, desc.VS);
    hash = ::stable_hash(hash, desc.PS);
    hash = ::stable_hash(hash, desc.DS);
    hash = ::stable_hash(hash, desc.HS);
    hash = ::stable_hash(hash, desc.GS);
    hash = ::stable_hash(hash, desc.StreamOutput);
    hash = ::stable_hash(hash, desc.BlendState, desc.NumRenderTargets);
    hash = ::stable_hash(hash, desc.SampleMask);
    hash = ::stable_hash(hash, desc.RasterizerState);
    hash = ::stable_hash(hash, desc.DepthStencilState);
    hash = ::stable_hash(hash, desc.InputLayout);
    hash = ::stable_hash(hash, desc.IBStripCutValue);
    hash = ::stable_hash(hash, desc.PrimitiveTopologyType);
    hash = ::stable_hash(hash, desc.NumRenderTargets);

    for (size_t i = 0; i < desc.NumRenderTargets; i++)
    {
        hash = ::stable_hash(hash, desc.RTVFormats[i]);
    }

    hash = ::stable_hash(hash, desc.DSVFormat);
    hash = ::stable_hash(hash, desc.SampleDesc);
    hash = ::stable_hash(hash, desc.NodeMask);
    hash = ::stable_hash(hash, desc.CachedPSO);
    hash = ::stable_hash(hash, desc.Flags);

    return hash;
}

static std::string cache_file_name(std::string_view extension)
{
    std::filesystem::path exe_path = ff::filesystem::executable_path();
    std::filesystem::path name = exe_path.filename().replace_extension();
    size_t exe_hash = ff::stable_hash_func(exe_path);
    return ff::string::concat("dx12_pso_", exe_hash, "_", ff::filesystem::to_string(name), extension);
}

ff::dx12::object_cache::object_cache()
    : cache_dir(ff::filesystem::user_local_path() / "ff.cache")
    , cache_library_path(this->cache_dir / ::cache_file_name(".bin"))
    , cache_pack_path(this->cache_dir / ::cache_file_name(".pack"))
{
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
    this->reset();
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
        std::scoped_lock lock(this->mutex);

        auto i = this->root_signatures.find(hash);
        if (i == this->root_signatures.end())
        {
            Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
            if (SUCCEEDED(ff::dx12::device()->CreateRootSignature(0, data->GetBufferPointer(), data->GetBufferSize(), IID_PPV_ARGS(&root_signature))))
            {
                this->root_signature_to_hash.try_emplace(root_signature.Get(), hash);
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

ID3D12PipelineState* ff::dx12::object_cache::pipeline_state(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
{
    size_t hash = this->pipeline_state_hash(desc);
    std::scoped_lock lock(this->mutex);

    auto i = this->pipeline_states.find(hash);
    if (i == this->pipeline_states.end())
    {
        std::string name = std::to_string(hash);

        /*std::filesystem::path cache_path = this->cache_dir / (std::string("pso_") + std::to_string(hash) + std::string(".bin"));
        if (ff::filesystem::exists(cache_path))
        {
            std::shared_ptr<ff::data_base> cache_data = ff::filesystem::map_binary_file(cache_path);
            if (cache_data)
            {
                D3D12_GRAPHICS_PIPELINE_STATE_DESC cache_desc = desc;
                cache_desc.CachedPSO.CachedBlobSizeInBytes = cache_data->size();
                cache_desc.CachedPSO.pCachedBlob = cache_data->data();

                Microsoft::WRL::ComPtr<ID3D12PipelineState> cache_state;
                if (SUCCEEDED(ff::dx12::device()->CreateGraphicsPipelineState(&cache_desc, IID_PPV_ARGS(&cache_state))))
                {
                    i = this->pipeline_states.try_emplace(hash, std::move(cache_state)).first;
                }
            }
        }*/

        Microsoft::WRL::ComPtr<ID3D12PipelineState> state;
        if (i == this->pipeline_states.end() && SUCCEEDED(ff::dx12::device()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&state))))
        {
            Microsoft::WRL::ComPtr<ID3DBlob> cache_blob;
            if (SUCCEEDED(state->GetCachedBlob(&cache_blob)))
            {
                //ff::thread_pool::add_task([cache_path, cache_blob]()
                //{
                //    ff::filesystem::write_binary_file(cache_path, cache_blob->GetBufferPointer(), cache_blob->GetBufferSize());
                //});
            }

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

size_t ff::dx12::object_cache::root_signature_hash(ID3D12RootSignature* root_signature)
{
    std::scoped_lock lock(this->mutex);

    auto i = this->root_signature_to_hash.find(root_signature);
    assert(i != this->root_signature_to_hash.end());
    return (i != this->root_signature_to_hash.end()) ? i->second : 0;
}

size_t ff::dx12::object_cache::pipeline_state_hash(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
{
    return ::stable_hash(desc, this->root_signature_hash(desc.pRootSignature));
}

D3D12_SHADER_BYTECODE ff::dx12::object_cache::shader(const std::string& name)
{
    auto i = this->shaders.find(name);
    if (i == this->shaders.end())
    {
        auto shader_data = ff::dxgi_host().shader_data(name);
        if (shader_data)
        {
            i = this->shaders.try_emplace(name, shader_data).first;
        }
    }

    assert(i != this->shaders.end());
    return (i != this->shaders.end())
        ? D3D12_SHADER_BYTECODE{ i->second->data(), i->second->size() }
        : D3D12_SHADER_BYTECODE{};
}

void ff::dx12::object_cache::save()
{
    if (!this->cache_library && !this->cache_pack)
    {
        return;
    }

    bool changed = false;

    for (auto [hash, state] : pipeline_states)
    {
        if (this->cache_library)
        {
            std::wstring name = std::to_wstring(hash);
            if (SUCCEEDED(this->cache_library->StorePipeline(name.c_str(), state.Get())))
            {
                changed = true;
            }
        }
        else
        {
            std::string name = std::to_string(hash);
            Microsoft::WRL::ComPtr<ID3DBlob> cache_blob;
            if (!this->cache_pack->get<ff::data_base>(name) && SUCCEEDED(state->GetCachedBlob(&cache_blob)))
            {
                std::shared_ptr<ff::data_base> cache_value = std::make_shared<ff::dxgi::data_blob_dx>(cache_blob);
                this->cache_pack->set<ff::data_base>(name, cache_value, ff::saved_data_type::none);
                changed = true;
            }
        }
    }

    if (!changed)
    {
        return;
    }

    std::filesystem::path save_path;
    std::shared_ptr<ff::data_base> save_data;

    if (this->cache_library)
    {
        size_t size = this->cache_library->GetSerializedSize();
        if (size)
        {
            std::vector<uint8_t> save_vector;
            save_vector.resize(size);

            if (SUCCEEDED(this->cache_library->Serialize(save_vector.data(), size)))
            {
                save_path = this->cache_library_path;
                save_data = std::make_shared<ff::data_vector>(std::move(save_vector));
            }
        }
    }
    else if (this->cache_pack)
    {
        // ff::dict::load
    }

    if (save_path.empty() || !save_data)
    {
        return;
    }

    this->cache_data.reset();
    ff::filesystem::remove(this->cache_library_path);
    ff::filesystem::remove(this->cache_pack_path);

    if (ff::filesystem::write_binary_file(save_path, save_data->data(), save_data->size()))
    {
    }
}

void ff::dx12::object_cache::before_reset()
{
    this->save();
    this->root_signatures.clear();
    this->root_signature_to_hash.clear();
    this->pipeline_states.clear();
    this->cache_library.Reset();
    this->cache_pack.reset();
    this->cache_data.reset();
}

bool ff::dx12::object_cache::reset()
{
    this->create_cache();
    return true;
}

void ff::dx12::object_cache::create_cache()
{
    assert(!this->cache_library && !this->cache_pack);

    bool use_library = false;
    {
        D3D12_FEATURE_DATA_SHADER_CACHE feature_data{};
        if (SUCCEEDED(ff::dx12::device()->CheckFeatureSupport(D3D12_FEATURE_SHADER_CACHE, &feature_data, sizeof(feature_data))))
        {
            use_library = ff::flags::has(feature_data.SupportFlags, D3D12_SHADER_CACHE_SUPPORT_LIBRARY);
        }
    }

    if (use_library)
    {
        this->cache_data = ff::filesystem::map_binary_file(this->cache_library_path);
        if (this->cache_data && FAILED(ff::dx12::device()->CreatePipelineLibrary(this->cache_data->data(), this->cache_data->size(), IID_PPV_ARGS(&this->cache_library))))
        {
            this->cache_data.reset();
            ff::filesystem::remove(this->cache_library_path);
            ff::dx12::device()->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&this->cache_library));
        }
    }

    if (!this->cache_library)
    {
        this->cache_data = ff::filesystem::map_binary_file(this->cache_pack_path);
        if (this->cache_data)
        {
            ff::data_reader reader(this->cache_data);
            ff::dict pack;
            if (ff::dict::load(reader, pack))
            {
                this->cache_pack = std::make_unique<ff::dict>(std::move(pack));
            }
            else
            {
                this->cache_data.reset();
                ff::filesystem::remove(this->cache_pack_path);
            }
        }

        if (!this->cache_pack)
        {
            this->cache_pack = std::make_unique<ff::dict>();
        }
    }
}
