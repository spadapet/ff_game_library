#include "pch.h"
#include "dx12_heap.h"
#include "graphics.h"

#if DXVER == 12

ff::dx12_heap::dx12_heap(size_t size, ff::dx12_heap::usage_t usage)
    : render_frame_complete_connection(ff::internal::graphics::render_frame_complete_sink().connect(std::bind(&ff::dx12_heap::render_frame_complete, this, std::placeholders::_1)))
    , last_used_frame(ff::internal::graphics::render_frame_count())
    , size_(size)
    , usage_(usage)
    , evicted(false)
{
    this->reset();
    ff::internal::graphics::add_child(this);
}

ff::dx12_heap::~dx12_heap()
{
    ff::internal::graphics::remove_child(this);
}

ff::dx12_heap::operator bool() const
{
    return this->heap != nullptr;
}

ID3D12HeapX* ff::dx12_heap::get() const
{
    return this->heap.Get();
}

ID3D12HeapX* ff::dx12_heap::operator->() const
{
    return this->heap.Get();
}

void ff::dx12_heap::used_this_frame()
{
    this->last_used_frame = ff::internal::graphics::render_frame_count();

    if (this->heap && this->evicted)
    {
        ID3D12Pageable* p = this->heap.Get();
        ff::graphics::dx12_device()->MakeResident(1, &p);
        //ff::graphics::dx12_device()->EnqueueMakeResident(...);
        this->evicted = false;
    }
}

size_t ff::dx12_heap::size() const
{
    return this->size_;
}

ff::dx12_heap::usage_t ff::dx12_heap::usage() const
{
    return this->usage_;
}

bool ff::dx12_heap::reset()
{
    this->heap.Reset();
    this->evicted = false;
    this->used_this_frame();

    D3D12_HEAP_PROPERTIES props;
    D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE;
    D3D12_RESIDENCY_PRIORITY priority = D3D12_RESIDENCY_PRIORITY_NORMAL;

    switch (this->usage_)
    {
        default:
            assert(false);
            return false;

        case ff::dx12_heap::usage_t::upload:
            props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            priority = D3D12_RESIDENCY_PRIORITY_LOW;
            break;

        case ff::dx12_heap::usage_t::gpu_buffers:
            props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
            break;

        case ff::dx12_heap::usage_t::gpu_textures:
            props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
            break;
    }

    CD3DX12_HEAP_DESC desc(static_cast<uint64_t>(this->size_), props, 0, flags);
    if (SUCCEEDED(ff::graphics::dx12_device()->CreateHeap(&desc, IID_PPV_ARGS(&this->heap))))
    {
        ID3D12Pageable* p = this->heap.Get();
        ff::graphics::dx12_device()->SetResidencyPriority(1, &p, &priority);

        return true;
    }

    assert(false);
    return false;
}

int ff::dx12_heap::reset_priority() const
{
    return ff::internal::graphics_reset_priorities::dx12_heap;
}

void ff::dx12_heap::render_frame_complete(uint64_t fence_value)
{
    if (this->heap && !this->evicted)
    {
        size_t frame = ff::internal::graphics::render_frame_count();
        if (frame > this->last_used_frame && frame - this->last_used_frame > 120)
        {
            ID3D12Pageable* p = this->heap.Get();
            ff::graphics::dx12_device()->Evict(1, &p);
            this->evicted = true;
        }
    }
}

#endif
