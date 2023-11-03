#pragma once

#include "access.h"
#include "fence_value.h"

namespace ff::dx12
{
    class queue;

    class fence : private ff::dxgi::device_child_base
    {
    public:
        fence(std::string_view name, ff::dx12::queue* queue, uint64_t initial_value = 1);
        fence(fence&& other) noexcept = delete;
        fence(const fence& other) = delete;
        virtual ~fence() override;

        fence& operator=(fence&& other) noexcept = delete;
        fence& operator=(const fence& other) = delete;

        operator bool() const;
        const std::string& name() const;
        ff::dx12::queue* queue() const;
        ff::dx12::fence_value next_value();
        ff::dx12::fence_value signal(ff::dx12::queue* queue);
        ff::dx12::fence_value signal(uint64_t value, ff::dx12::queue* queue);
        ff::dx12::fence_value signal_later();
        void wait(uint64_t value, ff::dx12::queue* queue);
        bool set_event(uint64_t value, HANDLE handle);
        bool complete(uint64_t value);

        std::shared_ptr<ff::dx12::fence_wrapper_t> wrapper() const;

        static void wait(ff::dx12::fence_value* values, size_t count, ff::dx12::queue* queue);
        static bool complete(ff::dx12::fence_value* values, size_t count);

    private:
        friend ID3D12Fence* ff::dx12::get_fence(const ff::dx12::fence& obj);

        // device_child_base
        virtual void before_reset() override;
        virtual bool reset() override;

        Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
        ff::dx12::queue* queue_{};
        std::string name_;
        std::shared_ptr<ff::dx12::fence_wrapper_t> wrapper_;
        std::mutex completed_value_mutex;
        uint64_t completed_value;
        uint64_t next_value_;
    };
}
