#pragma once

#if DXVER == 12

namespace ff::internal
{
    class dx12_buffer_allocator_base;
}

namespace ff
{
    class dx12_buffer_range
    {
    public:
        dx12_buffer_range();
        dx12_buffer_range(ff::internal::dx12_buffer_allocator_base& owner, size_t start, size_t size);
        dx12_buffer_range(dx12_buffer_range&& other) noexcept;
        dx12_buffer_range(const dx12_buffer_range& other) = delete;
        ~dx12_buffer_range();

        dx12_buffer_range& operator=(dx12_buffer_range&& other) noexcept;
        dx12_buffer_range& operator=(const dx12_buffer_range& other) = delete;

        operator bool() const;
        size_t start() const;
        size_t size() const;
        void free_range();
        void* cpu_address() const;
        D3D12_GPU_VIRTUAL_ADDRESS gpu_address() const;

    private:
        ff::internal::dx12_buffer_allocator_base* owner;
        size_t start_;
        size_t size_;
    };
}

#endif
