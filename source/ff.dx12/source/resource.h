#pragma once

#include "fence_value.h"

namespace ff::dx12
{
    class commands;
    class mem_range;

    class resource : private ff::dxgi::device_child_base
    {
    public:
        resource(
            const D3D12_RESOURCE_DESC& desc,
            D3D12_RESOURCE_STATES initial_state,
            D3D12_CLEAR_VALUE optimized_clear_value = {},
            std::shared_ptr<ff::dx12::mem_range> mem_range = {});
        resource(resource&& other) noexcept;
        resource(const resource& other) = delete;
        ~resource();

        resource& operator=(resource&& other) noexcept;
        resource& operator=(const resource& other) = delete;

        operator bool() const;

        void active(bool value, ff::dx12::commands* commands);
        bool active() const;
        void activated(); // notification from mem_range
        void deactivated(); // notification from mem_range

        const std::shared_ptr<ff::dx12::mem_range>& mem_range() const;
        D3D12_RESOURCE_STATES state(D3D12_RESOURCE_STATES state, ff::dx12::commands* commands);
        D3D12_RESOURCE_STATES state() const;
        const D3D12_RESOURCE_DESC& desc() const;
        const D3D12_RESOURCE_ALLOCATION_INFO& alloc_info() const;

        ff::dx12::fence_value update_buffer(ff::dx12::commands* commands, const void* data, uint64_t offset, uint64_t size);
        ff::dx12::fence_value update_texture(ff::dx12::commands* commands, const DirectX::ScratchImage& data);
        ff::dx12::fence_value update_texture(ff::dx12::commands* commands, const DirectX::Image& data, size_t sub_index, const D3D12_BOX* dest_box);
        ff::dx12::fence_value capture_buffer(ff::dx12::commands* commands, const std::shared_ptr<std::vector<uint8_t>>& result, uint64_t offset, uint64_t size);
        ff::dx12::fence_value capture_texture(ff::dx12::commands* commands, const std::shared_ptr<DirectX::ScratchImage>& result);
        ff::dx12::fence_value capture_texture(ff::dx12::commands* commands, const std::shared_ptr<DirectX::ScratchImage>& result, size_t sub_index, const D3D12_BOX* box);

    private:
        friend ID3D12ResourceX* ff::dx12::get_resource(const ff::dx12::resource& obj);

        void destroy(bool for_reset);

        // device_child_base
        virtual void before_reset() override;
        virtual bool reset() override;

        Microsoft::WRL::ComPtr<ID3D12ResourceX> resource_;
        std::shared_ptr<ff::dx12::mem_range> mem_range_;
        D3D12_CLEAR_VALUE optimized_clear_value;
        D3D12_RESOURCE_STATES state_;
        D3D12_RESOURCE_DESC desc_;
        D3D12_RESOURCE_ALLOCATION_INFO alloc_info_;
        ff::dx12::fence_values read_fence_values;
        ff::dx12::fence_value write_fence_value;
    };
}
