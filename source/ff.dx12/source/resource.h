#pragma once

#include "access.h"
#include "fence_value.h"
#include "fence_values.h"
#include "mem_range.h"

namespace ff::dx12
{
    class commands;
    class mem_range;

    class resource : private ff::dxgi::device_child_base
    {
    public:
        resource(std::shared_ptr<ff::dx12::mem_range> mem_range, const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON, D3D12_CLEAR_VALUE optimized_clear_value = {}); // placed
        resource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON, D3D12_CLEAR_VALUE optimized_clear_value = {}); // committed
        resource(resource& other, ff::dx12::commands* commands);
        resource(resource&& other) noexcept;
        resource(const resource& other) = delete;
        ~resource();

        resource& operator=(resource&& other) noexcept;
        resource& operator=(const resource& other) = delete;

        operator bool() const;

        const std::shared_ptr<ff::dx12::mem_range>& mem_range() const;
        const D3D12_RESOURCE_DESC& desc() const;
        const D3D12_RESOURCE_ALLOCATION_INFO& alloc_info() const;
        size_t sub_resource_count() const;

        void global_state(const D3D12_RESOURCE_STATES* states, size_t first_sub_resource, size_t count);
        std::pair<D3D12_RESOURCE_STATES, bool> global_state(size_t first_sub_resource, size_t count) const;
        ff::dx12::fence_values& global_reads();
        const ff::dx12::fence_values& global_reads() const;
        ff::dx12::fence_value& global_write();
        const ff::dx12::fence_value& global_write() const;

        struct readback_texture_data
        {
            size_t image_count() const;
            const DirectX::Image& image(size_t index);

            ff::dx12::fence_value fence_value;
            std::vector<std::pair<ff::dx12::mem_range, DirectX::Image>> mem_ranges;
            size_t width;
            size_t height;
            size_t mip_count;
            size_t array_count;
        };

        // Buffers
        ff::dx12::fence_value update_buffer(ff::dx12::commands* commands, const void* data, uint64_t offset, uint64_t size);
        std::pair<ff::dx12::fence_value, ff::dx12::mem_range> readback_buffer(ff::dx12::commands* commands, uint64_t offset, uint64_t size);
        std::vector<uint8_t> capture_buffer(ff::dx12::commands* commands, uint64_t offset, uint64_t size);

        // Textures
        ff::dx12::fence_value update_texture(ff::dx12::commands* commands, const DirectX::Image* images, size_t sub_index, size_t sub_count, ff::point_size dest_pos);
        readback_texture_data readback_texture(ff::dx12::commands* commands, size_t sub_index, size_t sub_count, const ff::rect_size* source_rect);
        DirectX::ScratchImage capture_texture(ff::dx12::commands* commands, size_t sub_index, size_t sub_count, const ff::rect_size* source_rect);

    private:
        friend ID3D12ResourceX* ff::dx12::get_resource(const ff::dx12::resource& obj);

        resource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initial_state, D3D12_CLEAR_VALUE optimized_clear_value, std::shared_ptr<ff::dx12::mem_range> mem_range, bool allocate_mem_range);
        void destroy(bool for_reset);

        // device_child_base
        virtual void before_reset() override;
        virtual bool reset() override;

        D3D12_RESOURCE_DESC desc_;
        D3D12_RESOURCE_ALLOCATION_INFO alloc_info_;
        D3D12_CLEAR_VALUE optimized_clear_value;
        std::shared_ptr<ff::dx12::mem_range> mem_range_;
        Microsoft::WRL::ComPtr<ID3D12ResourceX> resource_;

        // Global state of the resource BETWEEN ExecuteCommandLists. Command lists must keep track of state while building the commands.
        ff::stack_vector<D3D12_RESOURCE_STATES, 12> global_states_;
        ff::dx12::fence_values global_reads_;
        ff::dx12::fence_value global_write_;
    };
}
