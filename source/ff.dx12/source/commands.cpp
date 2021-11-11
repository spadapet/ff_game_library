#include "pch.h"
#include "access.h"
#include "commands.h"
#include "descriptor_allocator.h"
#include "device_reset_priority.h"
#include "device_reset_priority.h"
#include "fence.h"
#include "globals.h"
#include "heap.h"
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

    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::commands);
}

ff::dx12::commands::commands(commands&& other) noexcept
{
    *this = std::move(other);
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::commands);
}

ff::dx12::commands::~commands()
{
    ff::dx12::remove_device_child(this);
}

ff::dx12::commands& ff::dx12::commands::get(ff::dxgi::command_context_base& obj)
{
    return *static_cast<ff::dx12::commands*>(&obj);
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

void ff::dx12::commands::resource_barrier(const ff::dx12::resource* resource_before, const ff::dx12::resource* resource_after)
{
    D3D12_RESOURCE_BARRIER desc{};
    desc.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
    desc.Aliasing.pResourceBefore = resource_before ? ff::dx12::get_resource(*resource_before) : nullptr;
    desc.Aliasing.pResourceAfter = resource_after ? ff::dx12::get_resource(*resource_after) : nullptr;

    if (desc.Aliasing.pResourceBefore != desc.Aliasing.pResourceAfter)
    {
        this->list->ResourceBarrier(1, &desc);
    }
}

void ff::dx12::commands::resource_barrier(const ff::dx12::resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
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

void ff::dx12::commands::copy_resource(const ff::dx12::resource* dest_resource, const ff::dx12::resource* source_resource)
{
    ID3D12ResourceX* dest = ff::dx12::get_resource(*dest_resource);
    ID3D12ResourceX* source = ff::dx12::get_resource(*source_resource);

    if (dest != source)
    {
        this->list->CopyResource(dest, source);
    }
}

void ff::dx12::commands::update_buffer(ff::dx12::resource* dest, uint64_t dest_offset, ff::dx12::mem_range& source)
{
    assert(source.heap() && source.heap()->cpu_usage());
    this->list->CopyBufferRegion(ff::dx12::get_resource(*dest), dest_offset, ff::dx12::get_resource(*source.heap()), source.start(), source.size());
}

void ff::dx12::commands::readback_buffer(ff::dx12::mem_range& dest, ff::dx12::resource* source, uint64_t source_offset)
{
    assert(dest.heap() && dest.heap()->cpu_usage());
    this->list->CopyBufferRegion(ff::dx12::get_resource(*dest.heap()), dest.start(), ff::dx12::get_resource(*source), source_offset, dest.size());
}

void ff::dx12::commands::update_texture(ff::dx12::resource* dest, size_t dest_sub_index, ff::point_size dest_pos, ff::dx12::mem_range& source, const D3D12_SUBRESOURCE_FOOTPRINT& source_layout)
{
    assert(source.cpu_data());

    D3D12_TEXTURE_COPY_LOCATION source_location;
    source_location.pResource = ff::dx12::get_resource(*source.heap());
    source_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source_location.PlacedFootprint = D3D12_PLACED_SUBRESOURCE_FOOTPRINT{ source.start(), source_layout };

    this->list->CopyTextureRegion(
        &CD3DX12_TEXTURE_COPY_LOCATION(ff::dx12::get_resource(*dest), static_cast<UINT>(dest_sub_index)),
        static_cast<UINT>(dest_pos.x), static_cast<UINT>(dest_pos.y), 0, // Z
        &source_location, nullptr); // source box
}

void ff::dx12::commands::readback_texture(ff::dx12::mem_range& dest, const D3D12_SUBRESOURCE_FOOTPRINT& dest_layout, ff::dx12::resource* source, size_t source_sub_index, ff::rect_size source_rect)
{
    assert(dest.cpu_data());

    const D3D12_BOX source_box
    {
        static_cast<UINT>(source_rect.left),
        static_cast<UINT>(source_rect.top),
        0,
        static_cast<UINT>(source_rect.right),
        static_cast<UINT>(source_rect.bottom),
        1,
    };

    D3D12_TEXTURE_COPY_LOCATION dest_location;
    dest_location.pResource = ff::dx12::get_resource(*dest.heap());
    dest_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dest_location.PlacedFootprint = D3D12_PLACED_SUBRESOURCE_FOOTPRINT{ dest.start(), dest_layout };

    this->list->CopyTextureRegion(
        &dest_location, 0, 0, 0, // X, Y, Z
        &CD3DX12_TEXTURE_COPY_LOCATION(ff::dx12::get_resource(*source), static_cast<UINT>(source_sub_index)), &source_box);
}

void ff::dx12::commands::copy_texture(ff::dx12::resource* dest, size_t dest_sub_index, ff::point_size dest_pos, ff::dx12::resource* source, size_t source_sub_index, ff::rect_size source_rect)
{
    const D3D12_BOX source_box
    {
        static_cast<UINT>(source_rect.left),
        static_cast<UINT>(source_rect.top),
        0,
        static_cast<UINT>(source_rect.right),
        static_cast<UINT>(source_rect.bottom),
        1,
    };

    this->list->CopyTextureRegion(
        &CD3DX12_TEXTURE_COPY_LOCATION(ff::dx12::get_resource(*dest), static_cast<UINT>(dest_sub_index)),
        static_cast<UINT>(dest_pos.x), static_cast<UINT>(dest_pos.y), 0, // Z
        &CD3DX12_TEXTURE_COPY_LOCATION(ff::dx12::get_resource(*source), static_cast<UINT>(source_sub_index)), &source_box);
}

void ff::dx12::commands::before_reset()
{
    this->close();
}
