#pragma once

namespace ff::dx12
{
    class object_cache : private ff::dxgi::device_child_base
    {
    public:
        object_cache();
        object_cache(object_cache&& other) noexcept = delete;
        object_cache(const object_cache& other) = delete;
        ~object_cache();

        object_cache& operator=(object_cache&& other) noexcept = delete;
        object_cache& operator=(const object_cache& other) = delete;

        ID3D12RootSignature* root_signature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& desc);
        ID3D12PipelineStateX* pipeline_state(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);
        size_t root_signature_hash(ID3D12RootSignature* root_signature);
        size_t pipeline_state_hash(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);
        D3D12_SHADER_BYTECODE shader(const std::string& name);

    private:
        // device_child_base
        virtual void before_reset() override;
        virtual bool reset() override;

        std::mutex mutex;
        std::filesystem::path cache_dir;
        std::unordered_map<size_t, Microsoft::WRL::ComPtr<ID3D12RootSignature>, ff::no_hash<size_t>> root_signatures;
        std::unordered_map<ID3D12RootSignature*, size_t> root_signature_to_hash;
        std::unordered_map<size_t, Microsoft::WRL::ComPtr<ID3D12PipelineStateX>, ff::no_hash<size_t>> pipeline_states;
        std::unordered_map<std::string, std::shared_ptr<ff::data_base>, ff::stable_hash<std::string>> shaders;
    };
}