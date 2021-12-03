#include "pch.h"
#include "access.h"
#include "commands.h"
#include "depth.h"
#include "descriptor_allocator.h"
#include "device_reset_priority.h"
#include "device_reset_priority.h"
#include "fence.h"
#include "globals.h"
#include "heap.h"
#include "mem_range.h"
#include "queue.h"
#include "resource_tracker.h"

ff::dx12::commands::commands(ff::dx12::queue& queue, ff::dx12::commands::data_cache_t&& data_cache, ID3D12PipelineStateX* initial_state)
    : queue_(&queue)
    , data_cache(std::move(data_cache))
    , state_(initial_state)
{
    this->list()->Reset(this->data_cache.allocator.Get(), this->state_.Get());

    if (this->list()->GetType() != D3D12_COMMAND_LIST_TYPE_COPY)
    {
        ID3D12DescriptorHeap* heaps[2] =
        {
            ff::dx12::get_descriptor_heap(ff::dx12::gpu_view_descriptors()),
            ff::dx12::get_descriptor_heap(ff::dx12::gpu_sampler_descriptors()),
        };

        this->list()->SetDescriptorHeaps(2, heaps);
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
    if (*this)
    {
        this->queue_->execute(*this);
        assert(!*this);
    }

    ff::dx12::remove_device_child(this);
}

ff::dx12::commands& ff::dx12::commands::get(ff::dxgi::command_context_base& obj)
{
    return *static_cast<ff::dx12::commands*>(&obj);
}

ff::dx12::commands::operator bool() const
{
    return this->data_cache.allocator;
}

ff::dx12::queue& ff::dx12::commands::queue() const
{
    return *this->queue_;
}

ff::dx12::fence_value ff::dx12::commands::next_fence_value()
{
    return this->data_cache.fence->next_value();
}

void ff::dx12::commands::state(ID3D12PipelineStateX* state)
{
    if (this->state_.Get() != state)
    {
        this->state_ = state;
        this->list()->SetPipelineState(state);
    }
}

void ff::dx12::commands::flush(ff::dx12::commands* prev_commands, ff::dx12::commands* next_commands, ff::dx12::fence_values& wait_before_execute)
{
    ff::dx12::resource_tracker* prev_tracker = prev_commands ? prev_commands->resource_tracker() : nullptr;

    this->resource_tracker()->flush(this->list());
    this->list()->Close();

    this->list_before()->Reset(this->data_cache.allocator.Get(), nullptr);
    this->resource_tracker()->close(this->list_before(), prev_tracker);
    this->list_before()->Close();

    if (!next_commands)
    {
        this->resource_tracker()->finalize(this->list()->GetType());
        this->resource_tracker()->reset();
    }
    else if (prev_tracker)
    {
        prev_tracker->reset();
    }
}

ff::dx12::commands::data_cache_t ff::dx12::commands::close()
{
    if (this->data_cache.list)
    {
        this->data_cache.list->Close();
    }

    this->data_cache.allocator.Reset();
    this->data_cache.resource_tracker->reset();
    this->wait_before_execute.clear();
    this->state_.Reset();

    return data_cache_t(std::move(this->data_cache));
}

void ff::dx12::commands::clear(const ff::dx12::depth& depth, const float* depth_value, const BYTE* stencil_value)
{
    // TODO: transition, wait for reads/writes

    if (depth_value || stencil_value)
    {
        this->list()->ClearDepthStencilView(depth.view(),
            (depth_value ? D3D12_CLEAR_FLAG_DEPTH : static_cast<D3D12_CLEAR_FLAGS>(0)) | (stencil_value ? D3D12_CLEAR_FLAG_STENCIL : static_cast<D3D12_CLEAR_FLAGS>(0)),
            depth_value ? *depth_value : 0.0f,
            stencil_value ? *stencil_value : 0,
            0, nullptr);
    }
}

void ff::dx12::commands::discard(const ff::dx12::depth& depth)
{
    // TODO: transition, wait for reads/writes
    // MSDN: All subresources in a resource must be in the RENDER_TARGET state, or DEPTH_WRITE state,
    // for render targets / depth - stencil resources respectively, when ID3D12GraphicsCommandList::DiscardResource is called

    this->list()->DiscardResource(ff::dx12::get_resource(*depth.resource()), nullptr);
}

void ff::dx12::commands::update_buffer(const ff::dx12::resource& dest, uint64_t dest_offset, const ff::dx12::mem_range& source)
{
    // TODO: transition, wait for reads/writes
    // D3D12_RESOURCE_STATE_COPY_DEST

    assert(source.heap() && source.heap()->cpu_usage());
    this->list()->CopyBufferRegion(ff::dx12::get_resource(dest), dest_offset, ff::dx12::get_resource(*source.heap()), source.start(), source.size());
}

void ff::dx12::commands::readback_buffer(const ff::dx12::mem_range& dest, const ff::dx12::resource& source, uint64_t source_offset)
{
    // TODO: transition, wait for reads/writes
    // D3D12_RESOURCE_STATE_COPY_SOURCE

    assert(dest.heap() && dest.heap()->cpu_usage());
    this->list()->CopyBufferRegion(ff::dx12::get_resource(*dest.heap()), dest.start(), ff::dx12::get_resource(source), source_offset, dest.size());
}

void ff::dx12::commands::update_texture(const ff::dx12::resource& dest, size_t dest_sub_index, ff::point_size dest_pos, const ff::dx12::mem_range& source, const D3D12_SUBRESOURCE_FOOTPRINT& source_layout)
{
    // TODO: transition, wait for reads/writes
    // D3D12_RESOURCE_STATE_COPY_DEST

    assert(source.cpu_data());

    D3D12_TEXTURE_COPY_LOCATION source_location;
    source_location.pResource = ff::dx12::get_resource(*source.heap());
    source_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source_location.PlacedFootprint = D3D12_PLACED_SUBRESOURCE_FOOTPRINT{ source.start(), source_layout };

    this->list()->CopyTextureRegion(
        &CD3DX12_TEXTURE_COPY_LOCATION(ff::dx12::get_resource(dest), static_cast<UINT>(dest_sub_index)),
        static_cast<UINT>(dest_pos.x), static_cast<UINT>(dest_pos.y), 0, // Z
        &source_location, nullptr); // source box
}

void ff::dx12::commands::readback_texture(const ff::dx12::mem_range& dest, const D3D12_SUBRESOURCE_FOOTPRINT& dest_layout, const ff::dx12::resource& source, size_t source_sub_index, ff::rect_size source_rect)
{
    // TODO: transition, wait for reads/writes

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

    this->list()->CopyTextureRegion(
        &dest_location, 0, 0, 0, // X, Y, Z
        &CD3DX12_TEXTURE_COPY_LOCATION(ff::dx12::get_resource(source), static_cast<UINT>(source_sub_index)), &source_box);
}

void ff::dx12::commands::copy_resource(const ff::dx12::resource& dest_resource, const ff::dx12::resource& source_resource)
{
    // TODO: transition, wait for reads/writes
    // D3D12_RESOURCE_STATE_COPY_SOURCE
    // D3D12_RESOURCE_STATE_COPY_DEST
    // this->write_fence_value = commands->next_fence_value();

    ID3D12ResourceX* dest = ff::dx12::get_resource(dest_resource);
    ID3D12ResourceX* source = ff::dx12::get_resource(source_resource);

    if (dest != source)
    {
        this->list()->CopyResource(dest, source);
    }
}

void ff::dx12::commands::copy_texture(const ff::dx12::resource& dest, size_t dest_sub_index, ff::point_size dest_pos, const ff::dx12::resource& source, size_t source_sub_index, ff::rect_size source_rect)
{
    // TODO: transition, wait for reads/writes

    const D3D12_BOX source_box
    {
        static_cast<UINT>(source_rect.left),
        static_cast<UINT>(source_rect.top),
        0,
        static_cast<UINT>(source_rect.right),
        static_cast<UINT>(source_rect.bottom),
        1,
    };

    this->list()->CopyTextureRegion(
        &CD3DX12_TEXTURE_COPY_LOCATION(ff::dx12::get_resource(dest), static_cast<UINT>(dest_sub_index)),
        static_cast<UINT>(dest_pos.x), static_cast<UINT>(dest_pos.y), 0, // Z
        &CD3DX12_TEXTURE_COPY_LOCATION(ff::dx12::get_resource(source), static_cast<UINT>(source_sub_index)), &source_box);
}

ID3D12GraphicsCommandListX* ff::dx12::commands::list() const
{
    return this->data_cache.list.Get();
}

ID3D12GraphicsCommandListX* ff::dx12::commands::list_before() const
{
    return this->data_cache.list_before.Get();
}

ff::dx12::resource_tracker* ff::dx12::commands::resource_tracker() const
{
    return this->data_cache.resource_tracker.get();
}

void ff::dx12::commands::before_reset()
{
    this->close();
    assert(!*this);
}
