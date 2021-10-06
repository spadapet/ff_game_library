#pragma once

#include "access.h"
#include "fence_value.h"

namespace ff::dx12
{
    class queue;

    class fence : private ff::dxgi::device_child_base
    {
    public:
        fence(ff::dx12::queue* queue, uint64_t initial_value = 1);
        fence(fence&& other) noexcept = delete;
        fence(const fence& other) = delete;
        virtual ~fence() override;

        fence& operator=(fence&& other) noexcept = delete;
        fence& operator=(const fence& other) = delete;

        operator bool() const;
        ff::dx12::queue* queue() const;
        ff::dx12::fence_value next_value();
        ff::dx12::fence_value signal(ff::dx12::queue* queue);
        ff::dx12::fence_value signal(uint64_t value, ff::dx12::queue* queue);
        ff::dx12::fence_value signal_later();
        void wait(uint64_t value, ff::dx12::queue* queue);
        bool complete(uint64_t value);

        static void wait(ff::dx12::fence_value* values, size_t count, ff::dx12::queue* queue);
        static bool complete(ff::dx12::fence_value* values, size_t count);

    private:
        friend ID3D12FenceX* ff::dx12::get_fence(const ff::dx12::fence& obj);

        // device_child_base
        virtual void before_reset() override;
        virtual bool reset() override;

        Microsoft::WRL::ComPtr<ID3D12FenceX> fence_;
        ff::dx12::queue* queue_;
        std::mutex completed_value_mutex;
        uint64_t completed_value;
        std::atomic_uint64_t next_value_;
    };
}
