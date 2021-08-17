#pragma once

#if DXVER == 12

namespace ff::internal
{
    class dx12_descriptor_buffer_base;
}

namespace ff
{
    class dx12_descriptor_range
    {
    public:
        dx12_descriptor_range();
        dx12_descriptor_range(ff::internal::dx12_descriptor_buffer_base& owner, size_t start, size_t count);
        dx12_descriptor_range(dx12_descriptor_range&& other) noexcept;
        dx12_descriptor_range(const dx12_descriptor_range& other) = delete;
        ~dx12_descriptor_range();

        dx12_descriptor_range& operator=(dx12_descriptor_range&& other) noexcept;
        dx12_descriptor_range& operator=(const dx12_descriptor_range& other) = delete;

        operator bool() const;
        size_t start() const;
        size_t count() const;
        void free_range();
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle(size_t index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle(size_t index) const;

    private:
        ff::internal::dx12_descriptor_buffer_base* owner;
        size_t start_;
        size_t count_;
    };
}

#endif
