#pragma once

namespace ff::dx12
{
    class fence;
    class queue;

    class fence_value
    {
    public:
        fence_value();
        fence_value(ff::dx12::fence* fence, uint64_t value);
        fence_value(fence_value&& other) noexcept;
        fence_value(const fence_value& other) = default;

        fence_value& operator=(fence_value&& other) noexcept;
        fence_value& operator=(const fence_value& other) = default;
        bool operator==(const fence_value& other) const;
        bool operator!=(const fence_value& other) const;

        operator bool() const;
        ff::dx12::fence* fence() const;
        uint64_t get() const;

        void signal(ff::dx12::queue* queue);
        void wait(ff::dx12::queue* queue);
        bool complete();

    private:
        ff::dx12::fence* fence_;
        uint64_t value_;
    };
}
