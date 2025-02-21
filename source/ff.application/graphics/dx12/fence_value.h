#pragma once

namespace ff::dx12
{
    class fence;
    class queue;

    struct fence_wrapper_t
    {
        ff::dx12::fence* fence;
    };

    class fence_value
    {
    public:
        fence_value() = default;
        fence_value(ff::dx12::fence* fence, uint64_t value);
        fence_value(fence_value&& other) noexcept = default;
        fence_value(const fence_value& other) = default;

        fence_value& operator=(fence_value&& other) noexcept = default;
        fence_value& operator=(const fence_value& other) = default;
        bool operator==(const fence_value& other) const;
        bool operator!=(const fence_value& other) const;

        operator bool() const;
        ff::dx12::fence* fence() const;
        uint64_t get() const;

        void signal(ff::dx12::queue* queue) const;
        void wait(ff::dx12::queue* queue) const;
        bool set_event(HANDLE handle) const;
        bool complete() const;

    private:
        std::shared_ptr<ff::dx12::fence_wrapper_t> fence_{};
        uint64_t value_{};
    };
}
