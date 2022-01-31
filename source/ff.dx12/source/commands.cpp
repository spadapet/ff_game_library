#include "pch.h"
#include "access.h"
#include "buffer.h"
#include "commands.h"
#include "depth.h"
#include "descriptor_allocator.h"
#include "device_reset_priority.h"
#include "device_reset_priority.h"
#include "fence.h"
#include "globals.h"
#include "gpu_event.h"
#include "heap.h"
#include "mem_range.h"
#include "queue.h"
#include "resource.h"
#include "resource_tracker.h"
#include "target_access.h"

#ifdef _WIN64
#include <pix3.h>
#endif

static std::atomic_int data_counter;

ff::dx12::commands::data_cache_t::data_cache_t(ff::dx12::queue* queue)
    : fence(queue->name() + " fence", queue)
    , lists_reset_event(ff::win_handle::create_event(true))
{}

ff::dx12::commands::commands(ff::dx12::queue& queue, std::unique_ptr<ff::dx12::commands::data_cache_t>&& data_cache)
    : type_(data_cache->list->GetType())
    , queue_(&queue)
    , data_cache(std::move(data_cache))
{
    if (this->type_ != D3D12_COMMAND_LIST_TYPE_COPY)
    {
        ID3D12DescriptorHeap* heaps[2] =
        {
            ff::dx12::get_descriptor_heap(ff::dx12::gpu_view_descriptors()),
            ff::dx12::get_descriptor_heap(ff::dx12::gpu_sampler_descriptors()),
        };

        this->list(false)->SetDescriptorHeaps(2, heaps);
    }
}

ff::dx12::commands::~commands()
{
    if (*this)
    {
        this->queue_->execute(*this);
        assert(!*this);
    }
}

ff::dx12::commands& ff::dx12::commands::get(ff::dxgi::command_context_base& obj)
{
    return *static_cast<ff::dx12::commands*>(&obj);
}

ff::dx12::commands::operator bool() const
{
    return this->data_cache && this->data_cache->allocator;
}

ff::dx12::queue& ff::dx12::commands::queue() const
{
    return *this->queue_;
}

ff::dx12::fence_value ff::dx12::commands::next_fence_value()
{
    return this->data_cache->fence.next_value();
}

void ff::dx12::commands::begin_event(ff::dx12::gpu_event type)
{
#ifdef _WIN64
    ::PIXBeginEvent(this->list(false), ff::dx12::gpu_event_color(type), ff::dx12::gpu_event_name(type));
#endif
}

void ff::dx12::commands::end_event()
{
#ifdef _WIN64
    ::PIXEndEvent(this->list(false));
#endif
}

void ff::dx12::commands::close_command_lists(ff::dx12::commands* prev_commands, ff::dx12::commands* next_commands, ff::dx12::fence_values& wait_before_execute)
{
    wait_before_execute.add(this->wait_before_execute);
    this->wait_before_execute.clear();
    this->list()->Close();

    ff::dx12::resource_tracker* prev_tracker = prev_commands ? prev_commands->tracker() : nullptr;
    ff::dx12::resource_tracker* next_tracker = next_commands ? next_commands->tracker() : nullptr;
    this->tracker()->close(this->data_cache->list_before.Get(), prev_tracker, next_tracker);
    this->data_cache->list_before->Close();

    ::ResetEvent(this->data_cache->lists_reset_event);
}

std::unique_ptr<ff::dx12::commands::data_cache_t> ff::dx12::commands::take_data()
{
    this->tracker()->reset();
    this->wait_before_execute.clear();
    this->pipeline_state_.Reset();
    this->root_signature_.Reset();

    return std::move(this->data_cache);
}

void ff::dx12::commands::pipeline_state(ID3D12PipelineState* state)
{
    if (this->pipeline_state_.Get() != state)
    {
        this->pipeline_state_ = state;
        this->list(false)->SetPipelineState(state);
    }
}

void ff::dx12::commands::resource_state(ff::dx12::resource& resource, D3D12_RESOURCE_STATES state, size_t array_start, size_t array_size, size_t mip_start, size_t mip_size)
{
    this->keep_resident(resource);
    resource.prepare_state(this->wait_before_execute, this->next_fence_value(), *this->tracker(), state, array_start, array_size, mip_start, mip_size);
}

void ff::dx12::commands::resource_state_sub_index(ff::dx12::resource& resource, D3D12_RESOURCE_STATES state, size_t sub_index)
{
    this->keep_resident(resource);

    std::div_t dr = std::div(static_cast<int>(sub_index), static_cast<int>(resource.mip_size()));
    resource.prepare_state(this->wait_before_execute, this->next_fence_value(), *this->tracker(), state,
        static_cast<size_t>(dr.quot), 1, static_cast<size_t>(dr.rem), 1);
}

void ff::dx12::commands::resource_alias(ff::dx12::resource* resource_before, ff::dx12::resource* resource_after)
{
    if (resource_before)
    {
        this->resource_state(*resource_before, D3D12_RESOURCE_STATE_COMMON);
    }

    if (resource_after)
    {
        this->resource_state(*resource_after, D3D12_RESOURCE_STATE_COMMON);
    }

    this->tracker()->alias(resource_before, resource_after);
}

void ff::dx12::commands::root_signature(ID3D12RootSignature* signature)
{
    if (this->root_signature_.Get() != signature)
    {
        this->root_signature_ = signature;

        if (this->type_ == D3D12_COMMAND_LIST_TYPE_COMPUTE)
        {
            this->list(false)->SetComputeRootSignature(signature);
        }
        else
        {
            this->list(false)->SetGraphicsRootSignature(signature);
        }
    }
}

void ff::dx12::commands::root_descriptors(size_t index, ff::dx12::descriptor_range& range, size_t base_index)
{
    this->keep_resident(range);

    if (this->type_ == D3D12_COMMAND_LIST_TYPE_COMPUTE)
    {
        this->list(false)->SetComputeRootDescriptorTable(static_cast<UINT>(index), range.gpu_handle(base_index));
    }
    else
    {
        this->list(false)->SetGraphicsRootDescriptorTable(static_cast<UINT>(index), range.gpu_handle(base_index));
    }
}

void ff::dx12::commands::root_constant(size_t index, uint32_t data, size_t data_index)
{
    if (this->type_ == D3D12_COMMAND_LIST_TYPE_COMPUTE)
    {
        this->list(false)->SetComputeRoot32BitConstant(static_cast<UINT>(index), data, static_cast<UINT>(data_index));
    }
    else
    {
        this->list(false)->SetGraphicsRoot32BitConstant(static_cast<UINT>(index), data, static_cast<UINT>(data_index));
    }
}

void ff::dx12::commands::root_constants(size_t index, const void* data, size_t size, size_t data_index)
{
    if (this->type_ == D3D12_COMMAND_LIST_TYPE_COMPUTE)
    {
        this->list(false)->SetComputeRoot32BitConstants(static_cast<UINT>(index), static_cast<UINT>(size / 4), data, static_cast<UINT>(data_index));
    }
    else
    {
        this->list(false)->SetGraphicsRoot32BitConstants(static_cast<UINT>(index), static_cast<UINT>(size / 4), data, static_cast<UINT>(data_index));
    }
}

void ff::dx12::commands::root_cbv(size_t index, ff::dx12::buffer& buffer, size_t offset)
{
    if (buffer)
    {
        this->root_cbv(index, *buffer.resource(), buffer.gpu_address() + offset);
    }
}

void ff::dx12::commands::root_cbv(size_t index, ff::dx12::resource& resource, D3D12_GPU_VIRTUAL_ADDRESS data)
{
    this->resource_state(resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    if (this->type_ == D3D12_COMMAND_LIST_TYPE_COMPUTE)
    {
        this->list()->SetComputeRootConstantBufferView(static_cast<UINT>(index), data);
    }
    else
    {
        this->list()->SetGraphicsRootConstantBufferView(static_cast<UINT>(index), data);
    }
}

void ff::dx12::commands::root_srv(size_t index, ff::dx12::resource& resource, D3D12_GPU_VIRTUAL_ADDRESS data, bool ps_access, bool non_ps_access)
{
    this->resource_state(resource, static_cast<D3D12_RESOURCE_STATES>(
        (ps_access ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : 0),
        ((non_ps_access || !ps_access) ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : 0)));

    if (this->type_ == D3D12_COMMAND_LIST_TYPE_COMPUTE)
    {
        this->list()->SetComputeRootShaderResourceView(static_cast<UINT>(index), data);
    }
    else
    {
        this->list()->SetGraphicsRootShaderResourceView(static_cast<UINT>(index), data);
    }
}

void ff::dx12::commands::root_uav(size_t index, ff::dx12::resource& resource, D3D12_GPU_VIRTUAL_ADDRESS data)
{
    this->resource_state(resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    if (this->type_ == D3D12_COMMAND_LIST_TYPE_COMPUTE)
    {
        this->list()->SetComputeRootUnorderedAccessView(static_cast<UINT>(index), data);
    }
    else
    {
        this->list()->SetGraphicsRootUnorderedAccessView(static_cast<UINT>(index), data);
    }
}

void ff::dx12::commands::targets(ff::dxgi::target_base** targets, size_t count, ff::dxgi::depth_base* depth)
{
    ff::stack_vector<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> target_views;
    for (size_t i = 0; i < count; i++)
    {
        ff::dxgi::target_base* target = targets[i];
        ff::dx12::target_access& access = ff::dx12::target_access::get(*target);

        this->resource_state(access.dx12_target_texture(), D3D12_RESOURCE_STATE_RENDER_TARGET,
            target->target_array_start(), target->target_array_size(), target->target_mip_start(), target->target_mip_size());
        target_views.push_back(access.dx12_target_view());
    }

    D3D12_CPU_DESCRIPTOR_HANDLE depth_view{};
    if (depth)
    {
        ff::dx12::depth& dx12_depth = ff::dx12::depth::get(*depth);
        assert(dx12_depth);

        if (dx12_depth)
        {
            depth_view = dx12_depth.view();
            this->resource_state(*dx12_depth.resource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
        }
    }

    this->list()->OMSetRenderTargets(static_cast<UINT>(count), target_views.data(), FALSE, depth ? &depth_view : nullptr);
}

void ff::dx12::commands::viewports(const D3D12_VIEWPORT* viewports, size_t count)
{
    static D3D12_RECT scissor_rects[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] =
    {
        D3D12_RECT{ 0, 0, LONG_MAX, LONG_MAX },
        D3D12_RECT{ 0, 0, LONG_MAX, LONG_MAX },
        D3D12_RECT{ 0, 0, LONG_MAX, LONG_MAX },
        D3D12_RECT{ 0, 0, LONG_MAX, LONG_MAX },
        D3D12_RECT{ 0, 0, LONG_MAX, LONG_MAX },
        D3D12_RECT{ 0, 0, LONG_MAX, LONG_MAX },
        D3D12_RECT{ 0, 0, LONG_MAX, LONG_MAX },
        D3D12_RECT{ 0, 0, LONG_MAX, LONG_MAX },
        D3D12_RECT{ 0, 0, LONG_MAX, LONG_MAX },
        D3D12_RECT{ 0, 0, LONG_MAX, LONG_MAX },
        D3D12_RECT{ 0, 0, LONG_MAX, LONG_MAX },
        D3D12_RECT{ 0, 0, LONG_MAX, LONG_MAX },
        D3D12_RECT{ 0, 0, LONG_MAX, LONG_MAX },
        D3D12_RECT{ 0, 0, LONG_MAX, LONG_MAX },
        D3D12_RECT{ 0, 0, LONG_MAX, LONG_MAX },
        D3D12_RECT{ 0, 0, LONG_MAX, LONG_MAX },
    };

    this->list(false)->RSSetViewports(static_cast<UINT>(count), viewports);
    this->list(false)->RSSetScissorRects(static_cast<UINT>(count), scissor_rects);
}

void ff::dx12::commands::vertex_buffers(ff::dx12::buffer_base** buffers, const D3D12_VERTEX_BUFFER_VIEW* views, size_t start, size_t count)
{
    for (size_t i = 0; buffers && i < count; i++)
    {
        if (buffers[i]->resource())
        {
            this->resource_state(*buffers[i]->resource(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        }
        else
        {
            this->keep_resident(*buffers[i]);
        }
    }

    this->list()->IASetVertexBuffers(static_cast<UINT>(start), static_cast<UINT>(count), views);
}

void ff::dx12::commands::index_buffer(ff::dx12::buffer_base& buffer, const D3D12_INDEX_BUFFER_VIEW& view)
{
    if (buffer.resource())
    {
        this->resource_state(*buffer.resource(), D3D12_RESOURCE_STATE_INDEX_BUFFER);
    }
    else
    {
        this->keep_resident(buffer);
    }

    this->list()->IASetIndexBuffer(&view);
}

void ff::dx12::commands::primitive_topology(D3D12_PRIMITIVE_TOPOLOGY topology)
{
    this->list(false)->IASetPrimitiveTopology(topology);
}

void ff::dx12::commands::stencil(uint32_t value)
{
    this->list(false)->OMSetStencilRef(value);
}

void ff::dx12::commands::draw(size_t start, size_t count)
{
    this->list()->DrawInstanced(static_cast<UINT>(count), 1, static_cast<UINT>(start), 0);
}

void ff::dx12::commands::draw(size_t start_index, size_t index_count, size_t start_vertex)
{
    this->list()->DrawIndexedInstanced(static_cast<UINT>(index_count), 1, static_cast<UINT>(start_index), static_cast<UINT>(start_vertex), 0);
}

void ff::dx12::commands::resolve(ff::dx12::resource& dest_resource, size_t dest_sub_resource, ff::point_size dest_pos, ff::dx12::resource& src_resource, size_t src_sub_resource, ff::rect_size src_rect)
{
    this->resource_state_sub_index(dest_resource, D3D12_RESOURCE_STATE_RESOLVE_DEST, dest_sub_resource);
    this->resource_state_sub_index(src_resource, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, src_sub_resource);

    ff::rect_t<LONG> src_rect2 = src_rect.cast<LONG>();
    D3D12_RECT src_rect3{ src_rect2.left, src_rect2.top, src_rect2.right, src_rect2.bottom };

    this->list()->ResolveSubresourceRegion(
        ff::dx12::get_resource(dest_resource), static_cast<UINT>(dest_sub_resource), static_cast<UINT>(dest_pos.x), static_cast<UINT>(dest_pos.y),
        ff::dx12::get_resource(src_resource), static_cast<UINT>(src_sub_resource), &src_rect3, dest_resource.desc().Format, D3D12_RESOLVE_MODE_AVERAGE);
}

void ff::dx12::commands::clear(const ff::dx12::depth& depth, const float* depth_value, const BYTE* stencil_value)
{
    this->resource_state(*depth.resource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

    this->list()->ClearDepthStencilView(depth.view(),
        (depth_value ? D3D12_CLEAR_FLAG_DEPTH : static_cast<D3D12_CLEAR_FLAGS>(0)) | (stencil_value ? D3D12_CLEAR_FLAG_STENCIL : static_cast<D3D12_CLEAR_FLAGS>(0)),
        depth_value ? *depth_value : 0.0f,
        stencil_value ? *stencil_value : 0,
        0, nullptr);
}

void ff::dx12::commands::discard(const ff::dx12::depth& depth)
{
    this->resource_state(*depth.resource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
    this->list()->DiscardResource(ff::dx12::get_resource(*depth.resource()), nullptr);
}

void ff::dx12::commands::clear(ff::dxgi::target_base& target, const DirectX::XMFLOAT4& color)
{
    ff::dx12::target_access& access = ff::dx12::target_access::get(target);
    this->resource_state(access.dx12_target_texture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    this->list()->ClearRenderTargetView(access.dx12_target_view(), static_cast<const float*>(&color.x), 0, nullptr);
}

void ff::dx12::commands::discard(ff::dxgi::target_base& target)
{
    ff::dx12::target_access& access = ff::dx12::target_access::get(target);
    this->resource_state(access.dx12_target_texture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    this->list()->DiscardResource(ff::dx12::get_resource(access.dx12_target_texture()), nullptr);
}

void ff::dx12::commands::update_buffer(ff::dx12::resource& dest, uint64_t dest_offset, ff::dx12::mem_range& source)
{
    assert(source.heap() && source.heap()->cpu_usage());

    this->keep_resident(source);
    this->resource_state(dest, D3D12_RESOURCE_STATE_COPY_DEST);
    this->list()->CopyBufferRegion(ff::dx12::get_resource(dest), dest_offset, ff::dx12::get_resource(*source.heap()), source.start(), source.size());
}

void ff::dx12::commands::readback_buffer(ff::dx12::mem_range& dest, ff::dx12::resource& source, uint64_t source_offset)
{
    assert(dest.heap() && dest.heap()->cpu_usage());

    this->keep_resident(dest);
    this->resource_state(source, D3D12_RESOURCE_STATE_COPY_SOURCE);
    this->list()->CopyBufferRegion(ff::dx12::get_resource(*dest.heap()), dest.start(), ff::dx12::get_resource(source), source_offset, dest.size());
}

void ff::dx12::commands::update_texture(ff::dx12::resource& dest, size_t dest_sub_index, ff::point_size dest_pos, ff::dx12::mem_range& source, const D3D12_SUBRESOURCE_FOOTPRINT& source_layout)
{
    assert(source.cpu_data());

    this->keep_resident(source);
    this->resource_state_sub_index(dest, D3D12_RESOURCE_STATE_COPY_DEST, dest_sub_index);

    D3D12_TEXTURE_COPY_LOCATION source_location;
    source_location.pResource = ff::dx12::get_resource(*source.heap());
    source_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source_location.PlacedFootprint = D3D12_PLACED_SUBRESOURCE_FOOTPRINT{ source.start(), source_layout };

    this->list()->CopyTextureRegion(
        &CD3DX12_TEXTURE_COPY_LOCATION(ff::dx12::get_resource(dest), static_cast<UINT>(dest_sub_index)),
        static_cast<UINT>(dest_pos.x), static_cast<UINT>(dest_pos.y), 0, // Z
        &source_location, nullptr); // source box
}

void ff::dx12::commands::readback_texture(ff::dx12::mem_range& dest, const D3D12_SUBRESOURCE_FOOTPRINT& dest_layout, ff::dx12::resource& source, size_t source_sub_index, ff::rect_size source_rect)
{
    assert(dest.cpu_data());

    this->keep_resident(dest);
    this->resource_state_sub_index(source, D3D12_RESOURCE_STATE_COPY_SOURCE, source_sub_index);

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

void ff::dx12::commands::copy_resource(ff::dx12::resource& dest_resource, ff::dx12::resource& source_resource)
{
    this->resource_state(dest_resource, D3D12_RESOURCE_STATE_COPY_DEST);
    this->resource_state(source_resource, D3D12_RESOURCE_STATE_COPY_SOURCE);
    this->list()->CopyResource(ff::dx12::get_resource(dest_resource), ff::dx12::get_resource(source_resource));
}

void ff::dx12::commands::copy_texture(ff::dx12::resource& dest, size_t dest_sub_index, ff::point_size dest_pos, ff::dx12::resource& source, size_t source_sub_index, ff::rect_size source_rect)
{
    this->resource_state_sub_index(dest, D3D12_RESOURCE_STATE_COPY_DEST, dest_sub_index);
    this->resource_state_sub_index(source, D3D12_RESOURCE_STATE_COPY_SOURCE, source_sub_index);

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

ID3D12GraphicsCommandList1* ff::dx12::commands::list(bool flush_resource_state) const
{
    ID3D12GraphicsCommandList1* list = this->data_cache->list.Get();

    if (flush_resource_state)
    {
        this->tracker()->flush(list);
    }

    return list;
}

ff::dx12::resource_tracker* ff::dx12::commands::tracker() const
{
    return &this->data_cache->resource_tracker;
}

void ff::dx12::commands::keep_resident(ff::dx12::residency_access& access)
{
    ff::dx12::residency_data* data = access.residency_data();
    if (data)
    {
        this->data_cache->residency_set.insert(data);
    }
}
