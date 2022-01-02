#pragma once

#include "residency.h"

namespace ff::dx12
{
    class descriptor_buffer_base;

    class descriptor_range : public ff::dx12::residency_access
    {
    public:
        descriptor_range();
        descriptor_range(ff::dx12::descriptor_buffer_base& owner, size_t start, size_t count);
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

        // ff::dx12::residency_access
        virtual ff::dx12::residency_data* residency_data() override;

    private:
        ff::dx12::descriptor_buffer_base* owner;
        size_t start_;
        size_t count_;
    };
}
