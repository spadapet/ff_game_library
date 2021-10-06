#include "pch.h"
#include "device_reset_priority.h"
#include "globals.h"
#include "heap.h"

// Don't create resident or zeroed:
// https://devblogs.microsoft.com/directx/coming-to-directx-12-more-control-over-memory-allocation/

ff::dx12::heap::heap(uint64_t size, ff::dx12::heap::usage_t usage)
    : cpu_data_(nullptr)
    , size_(size)
    , usage_(usage)
{
    this->reset();
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::heap);
}

ff::dx12::heap::~heap()
{
    this->cpu_unmap();
    ff::dx12::remove_device_child(this);
}

ff::dx12::heap::operator bool() const
{
    return this->heap_ != nullptr;
}

void* ff::dx12::heap::cpu_data()
{
    if (this->heap_ && this->cpu_usage())
    {
        if (!this->cpu_data_)
        {
            if (!this->cpu_resource && FAILED(ff::dx12::device()->CreatePlacedResource(
                this->heap_.Get(),
                0, // start
                &CD3DX12_RESOURCE_DESC::Buffer(this->size_),
                (this->usage_ == ff::dx12::heap::usage_t::upload) ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr, // clear value
                IID_PPV_ARGS(&this->cpu_resource))))
            {
                assert(false);
                return nullptr;
            }

            if (FAILED(this->cpu_resource->Map(0, nullptr, &this->cpu_data_)))
            {
                assert(false);
                return nullptr;
            }
        }

        return this->cpu_data_;
    }

    return nullptr;
}

uint64_t ff::dx12::heap::size() const
{
    return this->size_;
}

ff::dx12::heap::usage_t ff::dx12::heap::usage() const
{
    return this->usage_;
}

bool ff::dx12::heap::cpu_usage() const
{
    return this->usage_ == ff::dx12::heap::usage_t::upload || this->usage_ == ff::dx12::heap::usage_t::readback;
}

void ff::dx12::heap::before_reset()
{
    this->cpu_unmap();
    this->heap_.Reset();
}

bool ff::dx12::heap::reset()
{
    D3D12_HEAP_PROPERTIES props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE;
    D3D12_RESIDENCY_PRIORITY priority = D3D12_RESIDENCY_PRIORITY_NORMAL;

    switch (this->usage_)
    {
        default:
            assert(false);
            return false;

        case ff::dx12::heap::usage_t::upload:
            props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            priority = D3D12_RESIDENCY_PRIORITY_LOW;
            break;

        case ff::dx12::heap::usage_t::readback:
            props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
            priority = D3D12_RESIDENCY_PRIORITY_LOW;
            break;

        case ff::dx12::heap::usage_t::gpu_buffers:
            flags |= D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES;
            break;

        case ff::dx12::heap::usage_t::gpu_textures:
            flags |= D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
            break;

        case ff::dx12::heap::usage_t::gpu_targets:
            flags |= D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES;
            priority = D3D12_RESIDENCY_PRIORITY_HIGH;
            break;
    }

    CD3DX12_HEAP_DESC desc(this->size_, props, 0, flags);
    if (SUCCEEDED(ff::dx12::device()->CreateHeap(&desc, IID_PPV_ARGS(&this->heap_))))
    {
        ID3D12Pageable* p = this->heap_.Get();
        ff::dx12::device()->SetResidencyPriority(1, &p, &priority);

        return true;
    }

    assert(false);
    return false;
}

void ff::dx12::heap::cpu_unmap()
{
    if (this->cpu_resource)
    {
        if (this->cpu_data_)
        {
            this->cpu_resource->Unmap(0, nullptr);
            this->cpu_data_ = nullptr;
        }

        this->cpu_resource.Reset();
    }
}
