#pragma once

namespace ff::internal::dx12
{
    class descriptor_buffer_base;
}

namespace ff::dx12
{
    class descriptor_range
    {
    public:
        descriptor_range();
        descriptor_range(ff::internal::dx12::descriptor_buffer_base& owner, size_t start, size_t count);
        descriptor_range(descriptor_range&& other) noexcept;
        descriptor_range(const descriptor_range& other) = delete;
        ~descriptor_range();

        descriptor_range& operator=(descriptor_range&& other) noexcept;
        descriptor_range& operator=(const descriptor_range& other) = delete;

        operator bool() const;
        size_t start() const;
        size_t count() const;
        void free_range();
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle(size_t index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle(size_t index) const;

    private:
        ff::internal::dx12::descriptor_buffer_base* owner;
        size_t start_;
        size_t count_;
    };
}
