#pragma once

namespace ff::dx12
{
    class commands;
    class heap;
    class mem_buffer_base;
    class resource;

    class mem_range
    {
    public:
        mem_range();
        mem_range(ff::dx12::mem_buffer_base& owner, uint64_t start, uint64_t size, uint64_t allocated_start, uint64_t allocated_size);
        mem_range(mem_range&& other) noexcept;
        mem_range(const mem_range& other) = delete;
        ~mem_range();

        mem_range& operator=(mem_range&& other) noexcept;
        mem_range& operator=(const mem_range& other) = delete;

        operator bool() const;
        uint64_t start() const;
        uint64_t size() const;
        uint64_t allocated_start() const;
        uint64_t allocated_size() const;
        void* cpu_data() const;
        D3D12_GPU_VIRTUAL_ADDRESS gpu_data() const;
        ff::dx12::heap* heap() const;

    private:
        void free_range();

        ff::dx12::mem_buffer_base* owner;
        uint64_t start_;
        uint64_t size_;
        uint64_t allocated_start_;
        uint64_t allocated_size_;
    };
}
