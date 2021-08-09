#pragma once

#include "graphics_child_base.h"

#if DXVER == 12

namespace ff
{
    class dx12_command_queue;
    struct dx12_resource;

    class dx12_commands : public ff::internal::graphics_child_base
    {
    public:
        dx12_commands(
            dx12_command_queue& owner,
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandListX>&& list,
            Microsoft::WRL::ComPtr<ID3D12CommandAllocatorX>&& allocator,
            ID3D12PipelineStateX* initial_state);
        dx12_commands(dx12_commands&& other) noexcept;
        dx12_commands(const dx12_commands& other) = delete;
        ~dx12_commands();

        dx12_commands& operator=(dx12_commands&& other) noexcept;
        dx12_commands& operator=(const dx12_commands& other) = delete;

        operator bool() const;
        ID3D12GraphicsCommandListX* get() const;
        ID3D12GraphicsCommandListX* operator->() const;
        void state(ID3D12PipelineStateX* state);
        void transition(dx12_resource& resource, D3D12_RESOURCE_STATES new_state);

        uint64_t execute(bool reopen);

        // graphics_child_base
        virtual bool reset() override;
        virtual int reset_priority() const override;

    private:
        void destroy();
        uint64_t execute();
        bool close();
        bool open();

        ff::dx12_command_queue* owner;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandListX> list;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocatorX> allocator;
        Microsoft::WRL::ComPtr<ID3D12PipelineStateX> state_;
        uint64_t allocator_fence_value;
        bool open_;
    };
}

#endif
