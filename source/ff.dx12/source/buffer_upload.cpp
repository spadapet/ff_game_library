#include "pch.h"
#include "buffer_upload.h"
#include "commands.h"
#include "globals.h"
#include "heap.h"
#include "mem_allocator.h"

ff::dx12::buffer_upload::buffer_upload(ff::dxgi::buffer_type type)
    : type_(type)
    , version_(0)
{}

ff::dx12::buffer_upload::operator bool() const
{
    return this->mem_range;
}

D3D12_VERTEX_BUFFER_VIEW ff::dx12::buffer_upload::vertex_view(size_t vertex_stride, uint64_t start_offset, size_t vertex_count) const
{
    if (this->mem_range)
    {
        assert(this->type_ == ff::dxgi::buffer_type::vertex);

        D3D12_VERTEX_BUFFER_VIEW view;
        view.BufferLocation = this->gpu_address() + start_offset;
        view.StrideInBytes = static_cast<UINT>(vertex_stride);
        view.SizeInBytes = static_cast<UINT>(vertex_stride * (vertex_count ? vertex_count : (this->size() - start_offset) / vertex_stride));

        return view;
    }

    return {};
}

D3D12_INDEX_BUFFER_VIEW ff::dx12::buffer_upload::index_view(size_t start, size_t count, DXGI_FORMAT format) const
{
    if (this->mem_range)
    {
        assert(this->type_ == ff::dxgi::buffer_type::index);

        D3D12_INDEX_BUFFER_VIEW view;
        view.BufferLocation = this->gpu_address() + (start * 2);
        view.SizeInBytes = 2 * static_cast<UINT>(count ? count : (this->size() / 2 - start));
        view.Format = format;
        return view;
    }

    return {};
}

D3D12_GPU_VIRTUAL_ADDRESS ff::dx12::buffer_upload::gpu_address() const
{
    return this->mem_range ? this->mem_range.gpu_data() : 0;
}

size_t ff::dx12::buffer_upload::version() const
{
    return this->version_;
}

ff::dxgi::buffer_type ff::dx12::buffer_upload::type() const
{
    return this->type_;
}

size_t ff::dx12::buffer_upload::size() const
{
    return this->mem_range ? this->mem_range.size() : 0;
}

bool ff::dx12::buffer_upload::writable() const
{
    return true;
}

bool ff::dx12::buffer_upload::update(ff::dxgi::command_context_base& context, const void* data, size_t size, size_t min_buffer_size)
{
    void* dest = this->map(context, size);
    if (dest)
    {
        std::memcpy(dest, data, size);
        return true;
    }

    return false;
}

void* ff::dx12::buffer_upload::map(ff::dxgi::command_context_base& context, size_t size)
{
    if (size)
    {
        ff::dx12::commands& commands = ff::dx12::commands::get(context);
        this->mem_range = ff::dx12::upload_allocator().alloc_buffer(size, commands.next_fence_value());
        this->version_ = (this->version_ + 1) ? this->version_ + 1 : 1;
        return this->mem_range.cpu_data();
    }
    else
    {
        this->mem_range = {};
        return nullptr;
    }
}

void ff::dx12::buffer_upload::unmap()
{}
