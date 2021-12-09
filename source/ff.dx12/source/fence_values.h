#pragma once

#include "fence_value.h"

namespace ff::dx12
{
    class fence_values
    {
    public:
        fence_values() = default;
        fence_values(std::initializer_list<ff::dx12::fence_value> list);
        fence_values(fence_values&& other) noexcept = default;
        fence_values(const fence_values& other) = default;

        fence_values& operator=(fence_values&& other) noexcept = default;
        fence_values& operator=(const fence_values& other) = default;

        void add(ff::dx12::fence_value fence_value);
        void add(const ff::dx12::fence_values& fence_values);
        void add(const ff::dx12::fence_values& read_values, ff::dx12::fence_value write_value);
        void reserve(size_t count);
        void clear();
        void clear_completed();

        void signal(ff::dx12::queue* queue);
        void wait(ff::dx12::queue* queue);
        bool complete();
        const ff::stack_vector<ff::dx12::fence_value, 4>& values() const;

    private:
        void internal_add(ff::dx12::fence_value fence_value);

        ff::stack_vector<ff::dx12::fence_value, 4> values_;
    };
}
