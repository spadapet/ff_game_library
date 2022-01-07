#include "pch.h"
#include "buffer.h"
#include "commands.h"
#include "descriptor_allocator.h"
#include "device_reset_priority.h"
#include "globals.h"
#include "mem_allocator.h"
#include "resource.h"

ff::dx12::buffer_base& ff::dx12::buffer_base::get(ff::dxgi::buffer_base& obj)
{
    return static_cast<ff::dx12::buffer_base&>(obj);
}

const ff::dx12::buffer_base& ff::dx12::buffer_base::get(const ff::dxgi::buffer_base& obj)
{
    return static_cast<const ff::dx12::buffer_base&>(obj);
}

ff::dx12::buffer_base::operator bool() const
{
    return this->valid();
}

ff::dx12::resource* ff::dx12::buffer_base::resource()
{
    return nullptr;
}

ff::dx12::residency_data* ff::dx12::buffer_base::residency_data()
{
    return nullptr;
}

D3D12_VERTEX_BUFFER_VIEW ff::dx12::buffer_base::vertex_view(size_t vertex_stride, uint64_t start_offset, size_t vertex_count) const
{
    assert(false);
    return {};
}

D3D12_INDEX_BUFFER_VIEW ff::dx12::buffer_base::index_view(size_t start, size_t count, DXGI_FORMAT format) const
{
    assert(false);
    return {};
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::dx12::buffer_base::constant_view()
{
    assert(false);
    return {};
}

D3D12_GPU_VIRTUAL_ADDRESS ff::dx12::buffer_base::gpu_address() const
{
    assert(false);
    return 0;
}

ff::dx12::buffer::buffer(ff::dxgi::buffer_type type, std::shared_ptr<ff::data_base> initial_data)
    : buffer(nullptr, type, initial_data ? initial_data->data() : nullptr, initial_data ? initial_data->size() : 0, 0, 0, initial_data)
{}

ff::dx12::buffer::buffer(buffer&& other) noexcept
{
    *this = std::move(other);
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::buffer::buffer(
    ff::dx12::commands* commands,
    ff::dxgi::buffer_type type,
    const void* data,
    uint64_t data_size,
    size_t data_hash,
    size_t version,
    std::shared_ptr<ff::data_base> initial_data,
    std::unique_ptr<std::vector<uint8_t>> mapped_mem,
    std::unique_ptr<ff::dx12::resource>&& resource,
    uint64_t mem_start)
    : resource_(std::move(resource))
    , initial_data(initial_data)
    , mapped_mem(std::move(mapped_mem))
    , mapped_context(nullptr)
    , type_(type)
    , mem_start(mem_start)
    , data_size(data_size)
    , data_hash(data_hash ? ff::stable_hash_bytes(data, data_size) : data_hash)
    , version_(version ? version : 1)
{
    assert(mem_start == ff::math::align_up<uint64_t>(mem_start, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));

    if (data && data_size)
    {
        if (!this->resource_ || this->mem_start + this->data_size > this->resource_->mem_range()->size())
        {
            D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(data_size);
            D3D12_RESOURCE_ALLOCATION_INFO alloc_info = ff::dx12::device()->GetResourceAllocationInfo(0, 1, &desc);
            desc.Width = alloc_info.SizeInBytes;

            this->resource_ = std::make_unique<ff::dx12::resource>(std::shared_ptr<ff::dx12::mem_range>(), desc);
            this->mem_start = 0;
        }

        this->resource_->update_buffer(commands, data, this->mem_start, data_size);
    }

    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::buffer::~buffer()
{
    ff::dx12::remove_device_child(this);
}

bool ff::dx12::buffer::valid() const
{
    return this->resource_ != nullptr;
}

size_t ff::dx12::buffer::version() const
{
    return this->version_;
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

D3D12_INDEX_BUFFER_VIEW ff::dx12::buffer::index_view(size_t start, size_t count, DXGI_FORMAT format) const
{
    assert(this->type_ == ff::dxgi::buffer_type::index);

    D3D12_INDEX_BUFFER_VIEW view;
    view.BufferLocation = this->gpu_address() + (start * 2);
    view.SizeInBytes = 2 * static_cast<UINT>(count ? count : (this->size() / 2 - start));
    view.Format = format;
    return view;
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::dx12::buffer::constant_view()
{
    assert(this->type_ == ff::dxgi::buffer_type::constant);

    if (!this->constant_view_)
    {
        this->constant_view_ = ff::dx12::cpu_buffer_descriptors().alloc_range(1);

        D3D12_CONSTANT_BUFFER_VIEW_DESC view_desc{};
        view_desc.BufferLocation = this->gpu_address();
        view_desc.SizeInBytes = static_cast<UINT>(this->size());
        ff::dx12::device()->CreateConstantBufferView(&view_desc, this->constant_view_.cpu_handle(0));
    }

    return this->constant_view_.cpu_handle(0);
}

D3D12_GPU_VIRTUAL_ADDRESS ff::dx12::buffer::gpu_address() const
{
    return this->resource_->gpu_address() + this->mem_start;
}

ff::dx12::residency_data* ff::dx12::buffer::residency_data()
{
    return this->resource_->residency_data();
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
            std::move(this->mapped_mem),
            std::move(this->resource_),
            ff::math::align_up<uint64_t>(this->mem_start + this->data_size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
    }

    return this->valid();
}

void* ff::dx12::buffer::map(ff::dxgi::command_context_base& context, size_t size)
{
    if (size)
    {
        if (!this->mapped_mem)
        {
            this->mapped_mem = std::make_unique<std::vector<uint8_t>>();
        }

        this->mapped_context = &context;
        this->mapped_mem->resize(size);
        return this->mapped_mem->data();
    }

    assert(false);
    return nullptr;
}

void ff::dx12::buffer::unmap()
{
    assert(this->mapped_context);
    if (this->mapped_context)
    {
        ff::dxgi::command_context_base& context = *this->mapped_context;
        this->mapped_context = nullptr;

        this->update(context, this->mapped_mem->data(), this->mapped_mem->size());
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

ff::dx12::buffer_upload::buffer_upload(ff::dxgi::buffer_type type)
    : type_(type)
{}

bool ff::dx12::buffer_upload::valid() const
{
    return this->mem_gpu_address != 0 && this->mem_size > 0;
}

size_t ff::dx12::buffer_upload::version() const
{
    return this->version_;
}

D3D12_VERTEX_BUFFER_VIEW ff::dx12::buffer_upload::vertex_view(size_t vertex_stride, uint64_t start_offset, size_t vertex_count) const
{
    if (this->valid())
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
    if (this->valid())
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
    return this->mem_gpu_address;
}

ff::dx12::residency_data* ff::dx12::buffer_upload::residency_data()
{
    return this->mem_residency_data;
}

ff::dxgi::buffer_type ff::dx12::buffer_upload::type() const
{
    return this->type_;
}

size_t ff::dx12::buffer_upload::size() const
{
    return this->mem_size;
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
    this->version_ = (this->version_ + 1) ? this->version_ + 1 : 1;

    if (size)
    {
        ff::dx12::commands& commands = ff::dx12::commands::get(context);
        ff::dx12::mem_range mem = ff::dx12::upload_allocator().alloc_buffer(size, commands.next_fence_value());

        this->mem_residency_data = mem.residency_data();
        this->mem_gpu_address = mem.gpu_data();
        this->mem_size = mem.size();

        return mem.cpu_data();
    }
    else
    {
        this->mem_residency_data = nullptr;
        this->mem_gpu_address = 0;
        this->mem_size = 0;

        return nullptr;
    }
}

void ff::dx12::buffer_upload::unmap()
{}

ff::dx12::buffer_cpu::buffer_cpu(ff::dxgi::buffer_type type)
    : type_(type)
    , data_hash(0)
    , version_(1)
{}

const std::vector<uint8_t>& ff::dx12::buffer_cpu::data() const
{
    return this->data_;
}

bool ff::dx12::buffer_cpu::valid() const
{
    return this->size() > 0;
}

size_t ff::dx12::buffer_cpu::version() const
{
    return this->version_;
}

ff::dxgi::buffer_type ff::dx12::buffer_cpu::type() const
{
    return this->type_;
}

size_t ff::dx12::buffer_cpu::size() const
{
    return this->data_.size();
}

bool ff::dx12::buffer_cpu::writable() const
{
    return true;
}

bool ff::dx12::buffer_cpu::update(ff::dxgi::command_context_base& context, const void* data, size_t size, size_t min_buffer_size)
{
    std::memcpy(this->map(context, size), data, size);
    this->unmap();
    return true;
}

void* ff::dx12::buffer_cpu::map(ff::dxgi::command_context_base& context, size_t size)
{
    this->data_.resize(size);
    return this->data_.data();
}

void ff::dx12::buffer_cpu::unmap()
{
    size_t new_hash = this->data_.size() ? ff::stable_hash_bytes(this->data_.data(), this->data_.size()) : 0;
    if (new_hash != this->data_hash)
    {
        this->data_hash = new_hash;
        this->version_ = (this->version_ + 1) ? (this->version_ + 1) : 1;
    }
}
