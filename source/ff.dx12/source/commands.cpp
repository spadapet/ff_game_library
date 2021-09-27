#include "pch.h"
#include "access.h"
#include "commands.h"
#include "descriptor_allocator.h"
#include "fence.h"
#include "globals.h"
#include "mem_range.h"

ff::dx12::commands::commands(
    ff::dx12::queue& queue,
    ID3D12GraphicsCommandListX* list,
    ID3D12CommandAllocatorX* allocator,
    std::unique_ptr<ff::dx12::fence>&& fence,
    ID3D12PipelineStateX* initial_state)
    : queue_(&queue)
    , list(std::move(list))
    , allocator(std::move(allocator))
    , state_(initial_state)
    , fence(std::move(fence))
{
    this->list->Reset(this->allocator.Get(), this->state_.Get());

    if (this->list->GetType() != D3D12_COMMAND_LIST_TYPE_COPY)
    {
        ID3D12DescriptorHeap* heaps[2] =
        {
            ff::dx12::get_descriptor_heap(ff::dx12::gpu_view_descriptors()),
            ff::dx12::get_descriptor_heap(ff::dx12::gpu_sampler_descriptors()),
        };

        this->list->SetDescriptorHeaps(2, heaps);
    }

    ff::internal::dx12::add_device_child(this, ff::internal::dx12::device_reset_priority::commands);
}

ff::dx12::commands::commands(commands&& other) noexcept
{
    *this = std::move(other);
    ff::internal::dx12::add_device_child(this, ff::internal::dx12::device_reset_priority::commands);
}

ff::dx12::commands::~commands()
{
    ff::internal::dx12::remove_device_child(this);
}

ff::dx12::commands::operator bool() const
{
    return this->list && this->allocator;
}

ff::dx12::queue& ff::dx12::commands::queue() const
{
    return *this->queue_;
}

ff::dx12::fence_value ff::dx12::commands::next_fence_value()
{
    return this->fence->next_value();
}

void ff::dx12::commands::state(ID3D12PipelineStateX* state)
{
    if (this->state_.Get() != state)
    {
        this->state_ = state;
        this->list->SetPipelineState(state);
    }
}

void ff::dx12::commands::close()
{
    if (this->list)
    {
        this->list->Close();
        this->list.Reset();
    }

    this->wait_before_execute_.clear();
    this->allocator.Reset();
    this->fence.reset();
    this->state_.Reset();
}

ff::dx12::fence_values& ff::dx12::commands::wait_before_execute()
{
    return this->wait_before_execute_;
}

void ff::dx12::commands::resource_barrier(ff::dx12::resource* resource_before, ff::dx12::resource* resource_after)
{
    D3D12_RESOURCE_BARRIER desc{};
    desc.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
    desc.Aliasing.pResourceBefore = resource_before ? ff::dx12::get_resource(*resource_before) : nullptr;
    desc.Aliasing.pResourceAfter = resource_after ? ff::dx12::get_resource(*resource_after) : nullptr;

    this->list->ResourceBarrier(1, &desc);
}

void ff::dx12::commands::resource_barrier(ff::dx12::resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
{
    assert(resource);

    if (state_before != state_after)
    {
        D3D12_RESOURCE_BARRIER desc{};
        desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        desc.Transition.pResource = ff::dx12::get_resource(*resource);
        desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        desc.Transition.StateBefore = state_before;
        desc.Transition.StateAfter = state_after;

        this->list->ResourceBarrier(1, &desc);
    }
}

void ff::dx12::commands::copy_buffer(ff::dx12::resource* dest, uint64_t dest_offset, ff::dx12::mem_range& source)
{
    assert(dest && source);

    if (source)
    {
        this->list->CopyBufferRegion(ff::dx12::get_resource(*dest), dest_offset, ff::dx12::get_resource(*source.heap()), 0, source.size());
    }
}

void ff::dx12::commands::before_reset()
{
    this->close();
}
