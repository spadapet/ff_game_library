#pragma once

#include "access.h"
#include "fence_values.h"
#include "resource_tracker.h"

namespace ff::dx12
{
    class depth;
    class mem_range;
    class resource;
    class queue;

    class commands : public ff::dxgi::command_context_base, private ff::dxgi::device_child_base
    {
    public:
        struct data_cache_t
        {
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandListX> list;
            ff::dx12::resource_tracker resource_tracker;
            std::unique_ptr<ff::dx12::fence> fence;
        };

        commands(ff::dx12::queue& queue, ff::dx12::commands::data_cache_t&& data_cache, ID3D12CommandAllocatorX* allocator, ID3D12PipelineStateX* initial_state);
        commands(commands&& other) noexcept;
        commands(const commands& other) = delete;
        virtual ~commands() override;

        static commands& get(ff::dxgi::command_context_base& obj);
        commands& operator=(commands&& other) noexcept = default;
        commands& operator=(const commands& other) = delete;

        operator bool() const;
        ff::dx12::queue& queue() const;
        ff::dx12::fence_value next_fence_value();
        void state(ID3D12PipelineStateX* state);
        ff::dx12::commands::data_cache_t close();

        void clear(const ff::dx12::depth& depth, const float* depth_value, const BYTE* stencil_value);
        void discard(const ff::dx12::depth& depth);

        void update_buffer(const ff::dx12::resource& dest, uint64_t dest_offset, const ff::dx12::mem_range& source);
        void readback_buffer(const ff::dx12::mem_range& dest, const ff::dx12::resource& source, uint64_t source_offset);

        void update_texture(const ff::dx12::resource& dest, size_t dest_sub_index, ff::point_size dest_pos, const ff::dx12::mem_range& source, const D3D12_SUBRESOURCE_FOOTPRINT& source_layout);
        void readback_texture(const ff::dx12::mem_range& dest, const D3D12_SUBRESOURCE_FOOTPRINT& dest_layout, const ff::dx12::resource& source, size_t source_sub_index, ff::rect_size source_rect);

        void copy_resource(const ff::dx12::resource& dest_resource, const ff::dx12::resource& source_resource);
        void copy_texture(const ff::dx12::resource& dest, size_t dest_sub_index, ff::point_size dest_pos, const ff::dx12::resource& source, size_t source_sub_index, ff::rect_size source_rect);

    private:
        friend ID3D12GraphicsCommandListX* ff::dx12::get_command_list(const ff::dx12::commands& obj);
        friend ID3D12CommandAllocatorX* ff::dx12::get_command_allocator(const ff::dx12::commands& obj);

        ID3D12GraphicsCommandListX* list() const;

        // device_child_base
        virtual void before_reset() override;

        ff::dx12::queue* queue_;
        ff::dx12::commands::data_cache_t data_cache;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocatorX> allocator;
        Microsoft::WRL::ComPtr<ID3D12PipelineStateX> state_;
    };
}
