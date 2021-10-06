#pragma once

#include "access.h"
#include "fence_values.h"

namespace ff::dx12
{
    class mem_range;
    class resource;
    class queue;

    class commands : public ff::dxgi::command_context, private ff::dxgi::device_child_base
    {
    public:
        commands(
            ff::dx12::queue& queue,
            ID3D12GraphicsCommandListX* list,
            ID3D12CommandAllocatorX* allocator,
            std::unique_ptr<ff::dx12::fence>&& fence,
            ID3D12PipelineStateX* initial_state);
        commands(commands&& other) noexcept;
        commands(const commands& other) = delete;
        virtual ~commands() override;

        static commands& get(ff::dxgi::command_context& obj);
        commands& operator=(commands&& other) noexcept = default;
        commands& operator=(const commands& other) = delete;

        operator bool() const;
        ff::dx12::queue& queue() const;
        ff::dx12::fence_value next_fence_value();
        void state(ID3D12PipelineStateX* state);
        void close();

        ff::dx12::fence_values& wait_before_execute();
        void resource_barrier(ff::dx12::resource* resource_before, ff::dx12::resource* resource_after);
        void resource_barrier(ff::dx12::resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);
        void copy_buffer(ff::dx12::resource* dest, uint64_t dest_offset, ff::dx12::mem_range& source);

    private:
        friend ID3D12GraphicsCommandListX* ff::dx12::get_command_list(const ff::dx12::commands& obj);
        friend ID3D12CommandAllocatorX* ff::dx12::get_command_allocator(const ff::dx12::commands& obj);
        friend std::unique_ptr<ff::dx12::fence>&& ff::dx12::move_fence(ff::dx12::commands& obj);

        // device_child_base
        virtual void before_reset() override;

        ff::dx12::queue* queue_;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandListX> list;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocatorX> allocator;
        Microsoft::WRL::ComPtr<ID3D12PipelineStateX> state_;
        ff::dx12::fence_values wait_before_execute_;
        std::unique_ptr<ff::dx12::fence> fence;
    };
}
