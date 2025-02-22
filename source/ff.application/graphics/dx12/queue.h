#pragma once

#include "../dx12/access.h"
#include "../dx12/commands.h"
#include "../dxgi/device_child_base.h"

namespace ff::dx12
{
    enum class gpu_event;

    class queue : private ff::dxgi::device_child_base
    {
    public:
        queue(std::string_view name, D3D12_COMMAND_LIST_TYPE type);
        queue(queue&& other) noexcept = delete;
        queue(const queue& other) = delete;
        virtual ~queue() override;

        queue& operator=(queue&& other) noexcept = delete;
        queue& operator=(const queue& other) = delete;

        operator bool() const;
        const std::string& name() const;
        void wait_for_idle();
        void begin_event(ff::dx12::gpu_event type);
        void end_event();

        std::unique_ptr<ff::dx12::commands> new_commands();
        ff::dx12::fence_value execute(ff::dx12::commands& commands);
        void execute(ff::dx12::commands** commands, size_t count);

    private:
        friend ID3D12CommandQueue* ff::dx12::get_command_queue(const ff::dx12::queue& obj);

        void new_allocators(Microsoft::WRL::ComPtr<ID3D12CommandAllocator>& allocator, Microsoft::WRL::ComPtr<ID3D12CommandAllocator>& allocator_before);
        void wait_for_tasks();

        // device_child_base
        virtual void before_reset() override;
        virtual bool reset() override;

        D3D12_COMMAND_LIST_TYPE type;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue;

        std::mutex mutex;
        std::string name_;
        ff::dx12::fence idle_fence;
        std::list<std::unique_ptr<ff::dx12::commands::data_cache_t>> caches;
        std::list<std::pair<ff::dx12::fence_value, Microsoft::WRL::ComPtr<ID3D12CommandAllocator>>> allocators;
        std::list<std::pair<ff::dx12::fence_value, Microsoft::WRL::ComPtr<ID3D12CommandAllocator>>> allocators_before;
    };
}
