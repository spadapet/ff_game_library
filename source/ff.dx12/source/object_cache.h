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
        std::shared_ptr<ff::data_base> shader(const std::string& name);

    private:
        // device_child_base
        virtual void before_reset() override;
        virtual bool reset() override;

        std::unordered_map<size_t, Microsoft::WRL::ComPtr<ID3D12RootSignature>, ff::no_hash<size_t>> root_signatures;
        std::unordered_map<size_t, Microsoft::WRL::ComPtr<ID3D12PipelineStateX>, ff::no_hash<size_t>> pipeline_states;
        std::unordered_map<std::string, std::shared_ptr<ff::data_base>, ff::stable_hash<std::string>> shaders;
    };
}
