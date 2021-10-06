#pragma once

#include "access.h"

namespace ff::dx12
{
    class heap : private ff::dxgi::device_child_base
    {
    public:
        enum class usage_t
        {
            upload,
            readback,
            gpu_buffers,
            gpu_textures,
            gpu_targets,
        };

        heap(uint64_t size, ff::dx12::heap::usage_t usage);
        heap(heap&& other) noexcept = default;
        heap(const heap& other) = delete;
        virtual ~heap() override;

        heap& operator=(heap&& other) noexcept = default;
        heap& operator=(const heap& other) = delete;

        operator bool() const;
        void* cpu_data();
        uint64_t size() const;
        usage_t usage() const;
        bool cpu_usage() const;

    private:
        friend ID3D12HeapX* ff::dx12::get_heap(const ff::dx12::heap& obj);
        friend ID3D12ResourceX* ff::dx12::get_resource(ff::dx12::heap& obj);

        // device_child_base
        virtual void before_reset() override;
        virtual bool reset() override;

        void cpu_unmap();

        Microsoft::WRL::ComPtr<ID3D12HeapX> heap_;
        Microsoft::WRL::ComPtr<ID3D12ResourceX> cpu_resource;
        void* cpu_data_;
        uint64_t size_;
        usage_t usage_;
    };
}
