#pragma once

#include "dx12_resource.h"
#include "graphics_child_base.h"

#if DXVER == 12

namespace ff::internal
{
}

namespace ff
{
    //class dx12_buffer_allocator_cpu : public ff::internal::graphics_child_base
    //{
    //public:
    //    dx12_buffer_allocator_cpu(size_t ring_size);
    //    dx12_buffer_allocator_cpu(dx12_buffer_allocator_cpu&& other) noexcept = default;
    //    dx12_buffer_allocator_cpu(const dx12_buffer_allocator_cpu& other) = delete;
    //    ~dx12_buffer_allocator_cpu();
    //
    //    dx12_buffer_allocator_cpu& operator=(dx12_buffer_allocator_cpu&& other) noexcept = default;
    //    dx12_buffer_allocator_cpu& operator=(const dx12_buffer_allocator_cpu& other) = delete;
    //
    //    ff::dx12_descriptor_range alloc_bytes(size_t size, size_t align);
    //    void fence(uint64_t value);
    //
    //    // graphics_child_base
    //    virtual bool reset() override;
    //    virtual int reset_priority() const override;
    //
    //private:
    //    Microsoft::WRL::ComPtr<ID3D12DescriptorHeapX> descriptor_heap;
    //    std::unique_ptr<ff::internal::dx12_descriptor_ring> ring;
    //    D3D12_DESCRIPTOR_HEAP_TYPE type;
    //    size_t ring_size;
    //};
}

#endif
