#include "pch.h"
#include "buffer.h"
#include "commands.h"
#include "descriptor_allocator.h"
#include "device_reset_priority.h"
#include "globals.h"
#include "resource.h"

ff::dx12::buffer::buffer(ff::dxgi::buffer_type type, std::shared_ptr<ff::data_base> initial_data)
    : buffer(nullptr, type, initial_data ? initial_data->data() : nullptr, initial_data ? initial_data->size() : 0, 0, 0, initial_data, {})
{}

ff::dx12::buffer::buffer(
    ff::dx12::commands* commands,
    ff::dxgi::buffer_type type,
    const void* data,
    uint64_t data_size,
    size_t data_hash,
    size_t version,
    std::shared_ptr<ff::data_base> initial_data,
    std::unique_ptr<std::vector<uint8_t>> mapped_mem)
    : initial_data(initial_data)
    , mapped_mem(std::move(mapped_mem))
    , mapped_context(nullptr)
    , type_(type)
    , data_size(data_size)
    , data_hash(data_hash)
    , version_(version ? version : 1)
{
    if (data && data_size)
    {
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(data_size);
        D3D12_RESOURCE_ALLOCATION_INFO alloc_info = ff::dx12::device()->GetResourceAllocationInfo(0, 1, &desc);
        desc.Width = alloc_info.SizeInBytes;

        this->data_hash = !this->data_hash ? ff::stable_hash_bytes(data, data_size) : this->data_hash;
        this->resource_ = std::make_unique<ff::dx12::resource>(std::shared_ptr<ff::dx12::mem_range>(), desc);
        this->resource_->update_buffer(commands, data, 0, data_size);

        if (this->type_ == ff::dxgi::buffer_type::constant)
        {
            this->constant_view_ = ff::dx12::cpu_buffer_descriptors().alloc_range(1);
            D3D12_CONSTANT_BUFFER_VIEW_DESC view_desc{};
            view_desc.BufferLocation = this->gpu_address();
            view_desc.SizeInBytes = static_cast<UINT>(desc.Width);
            ff::dx12::device()->CreateConstantBufferView(&view_desc, this->constant_view_.cpu_handle(0));
        }
    }

    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::buffer::~buffer()
{
    ff::dx12::remove_device_child(this);
}

ff::dx12::buffer& ff::dx12::buffer::get(ff::dxgi::buffer_base& obj)
{
    return static_cast<ff::dx12::buffer&>(obj);
}

const ff::dx12::buffer& ff::dx12::buffer::get(const ff::dxgi::buffer_base& obj)
{
    return static_cast<const ff::dx12::buffer&>(obj);
}

ff::dx12::buffer::operator bool() const
{
    return this->resource_ != nullptr;
}

ff::dx12::resource* ff::dx12::buffer::resource()
{
    return this->resource_.get();
}

D3D12_VERTEX_BUFFER_VIEW ff::dx12::buffer::vertex_view(size_t vertex_stride, uint64_t start_offset, size_t vertex_count) const
{
    assert(this->type_ == ff::dxgi::buffer_type::vertex);

    D3D12_VERTEX_BUFFER_VIEW view;
    view.BufferLocation = this->gpu_address() + start_offset;
    view.StrideInBytes = static_cast<UINT>(vertex_stride);
    view.SizeInBytes = static_cast<UINT>(vertex_stride * (vertex_count ? vertex_count : (this->size() - start_offset) / vertex_stride));

    return view;
}

D3D12_INDEX_BUFFER_VIEW ff::dx12::buffer::index_view(size_t start, size_t count) const
{
    assert(this->type_ == ff::dxgi::buffer_type::index);

    D3D12_INDEX_BUFFER_VIEW view;
    view.BufferLocation = this->gpu_address() + (start * 2);
    view.SizeInBytes = 2 * static_cast<UINT>(count ? count : (this->size() / 2 - start));
    view.Format = DXGI_FORMAT_R16_UINT;
    return view;
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::dx12::buffer::constant_view() const
{
    assert(this->type_ == ff::dxgi::buffer_type::constant);
    return this->constant_view_ ? this->constant_view_.cpu_handle(0) : D3D12_CPU_DESCRIPTOR_HANDLE{};
}

D3D12_GPU_VIRTUAL_ADDRESS ff::dx12::buffer::gpu_address() const
{
    return this->resource_->gpu_address();
}

size_t ff::dx12::buffer::version() const
{
    return this->version_;
}

ff::dxgi::buffer_type ff::dx12::buffer::type() const
{
    return this->type_;
}

size_t ff::dx12::buffer::size() const
{
    return  this->resource_ ? this->data_size : 0;
}

bool ff::dx12::buffer::writable() const
{
    return true;
}

bool ff::dx12::buffer::update(ff::dxgi::command_context_base& context, const void* data, size_t data_size, size_t min_buffer_size)
{
    size_t new_hash = data_size ? ff::stable_hash_bytes(data, data_size) : 0;
    if (new_hash != this->data_hash)
    {
        *this = ff::dx12::buffer(
            &ff::dx12::commands::get(context),
            this->type_,
            data,
            data_size,
            new_hash,
            this->version_ + 1,
            this->initial_data,
            std::move(this->mapped_mem));
    }

    return true;
}

void* ff::dx12::buffer::map(ff::dxgi::command_context_base& context, size_t size)
{
    if (size)
    {
        if (!this->mapped_mem)
        {
            this->mapped_mem = std::make_unique<std::vector<uint8_t>>();
        }

        if (this->mapped_mem->size() < size)
        {
            this->mapped_mem->resize(size);
        }

        this->mapped_context = &context;

        return this->mapped_mem->data();
    }

    return nullptr;
}

void ff::dx12::buffer::unmap()
{
    if (this->mapped_context)
    {
        ff::dx12::commands& commands = ff::dx12::commands::get(*this->mapped_context);
        const void* data = this->mapped_mem->data();
        uint64_t data_size = this->mapped_mem->size();
        size_t new_hash = data_size ? ff::stable_hash_bytes(data, data_size) : 0;

        if (new_hash != this->data_hash)
        {
            *this = ff::dx12::buffer(
                &commands,
                this->type_,
                data,
                data_size,
                new_hash,
                this->version_ + 1,
                this->initial_data,
                std::move(this->mapped_mem));
        }
    }
}

bool ff::dx12::buffer::reset()
{
    *this = ff::dx12::buffer(
        nullptr,
        this->type_,
        this->initial_data ? this->initial_data->data() : nullptr,
        this->initial_data ? this->initial_data->size() : 0,
        0, // hash
        this->version_ + 1,
        this->initial_data,
        std::move(this->mapped_mem));

    return *this;
}
