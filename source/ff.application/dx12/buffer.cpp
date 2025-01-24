#include "pch.h"
#include "dx12/buffer.h"
#include "dx12/commands.h"
#include "dx12/descriptor_allocator.h"
#include "dx12/device_reset_priority.h"
#include "dx12/dx12_globals.h"
#include "dx12/mem_allocator.h"
#include "dx12/resource.h"

ff::dx12::buffer_base::buffer_base(ff::dxgi::buffer_type type)
    : type_(type)
{
}

ff::dx12::buffer_base::operator bool() const
{
    return this->valid();
}

size_t ff::dx12::buffer_base::version() const
{
    return 0;
}

ff::dx12::resource* ff::dx12::buffer_base::resource() const
{
    return nullptr;
}

D3D12_GPU_VIRTUAL_ADDRESS ff::dx12::buffer_base::gpu_address() const
{
    return 0;
}

D3D12_VERTEX_BUFFER_VIEW ff::dx12::buffer_base::vertex_view(size_t vertex_stride, uint64_t start_offset, size_t vertex_count) const
{
    check_ret_val(this->type() == ff::dxgi::buffer_type::vertex && this->valid() && this->gpu_address() && this->size(), D3D12_VERTEX_BUFFER_VIEW{});

    return D3D12_VERTEX_BUFFER_VIEW
    {
        .BufferLocation = this->gpu_address() + start_offset,
        .SizeInBytes = static_cast<UINT>(vertex_stride * (vertex_count ? vertex_count : (this->size() - start_offset) / vertex_stride)),
        .StrideInBytes = static_cast<UINT>(vertex_stride),
    };
}

D3D12_INDEX_BUFFER_VIEW ff::dx12::buffer_base::index_view(size_t start, size_t count, DXGI_FORMAT format) const
{
    check_ret_val(this->type() == ff::dxgi::buffer_type::index && this->valid() && this->gpu_address() && this->size(), D3D12_INDEX_BUFFER_VIEW{});

    assert_ret_val(format == DXGI_FORMAT_R32_UINT || format == DXGI_FORMAT_R16_UINT, D3D12_INDEX_BUFFER_VIEW{});
    size_t bytes_per_index = (format == DXGI_FORMAT_R32_UINT) ? 4 : 2;

    return D3D12_INDEX_BUFFER_VIEW
    {
        .BufferLocation = this->gpu_address() + (start * bytes_per_index),
        .SizeInBytes = static_cast<UINT>(bytes_per_index) * static_cast<UINT>(count ? count : (this->size() / bytes_per_index - start)),
        .Format = format,
    };
}

ff::dxgi::buffer_type ff::dx12::buffer_base::type() const
{
    return this->type_;
}

bool ff::dx12::buffer_base::writable() const
{
    return false;
}

bool ff::dx12::buffer_base::update(ff::dxgi::command_context_base& context, const void* data, size_t size)
{
    debug_fail_ret_val(false);
}

void* ff::dx12::buffer_base::map(ff::dxgi::command_context_base& context, size_t size)
{
    debug_fail_ret_val(nullptr);
}

void ff::dx12::buffer_base::unmap(ff::dxgi::command_context_base& context)
{
    debug_fail();
}

ff::dx12::residency_data* ff::dx12::buffer_base::residency_data()
{
    return nullptr;
}

ff::dx12::buffer::buffer(ff::dxgi::buffer_type type, std::shared_ptr<ff::data_base> initial_data)
    : buffer(nullptr, type, initial_data ? initial_data->data() : nullptr, initial_data ? initial_data->size() : 0, 0, 0, initial_data)
{
}

ff::dx12::buffer::buffer(buffer&& other) noexcept
    : buffer_base(other.type())
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
    : ff::dx12::buffer_base(type)
    , resource_(std::move(resource))
    , initial_data(initial_data)
    , mapped_mem(std::move(mapped_mem))
    , mem_start(mem_start)
    , data_size(data_size)
    , data_hash(data_hash ? ff::stable_hash_bytes(data, static_cast<size_t>(data_size)) : data_hash)
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

            this->resource_ = std::make_unique<ff::dx12::resource>("Buffer", std::shared_ptr<ff::dx12::mem_range>(), desc);
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

ff::dx12::resource* ff::dx12::buffer::resource() const
{
    return this->resource_.get();
}

D3D12_GPU_VIRTUAL_ADDRESS ff::dx12::buffer::gpu_address() const
{
    return this->resource_->gpu_address() + this->mem_start;
}

ff::dx12::residency_data* ff::dx12::buffer::residency_data()
{
    return this->resource_->residency_data();
}

size_t ff::dx12::buffer::size() const
{
    return this->resource_ ? static_cast<size_t>(this->data_size) : 0;
}

bool ff::dx12::buffer::writable() const
{
    return true;
}

bool ff::dx12::buffer::update(ff::dxgi::command_context_base& context, const void* data, size_t size)
{
    assert_ret_val(size, false);
    size_t new_hash = ff::stable_hash_bytes(data, size);
    check_ret_val(new_hash != this->data_hash, false);

    if (new_hash != this->data_hash)
    {
        *this = ff::dx12::buffer(
            &ff::dx12::commands::get(context),
            this->type(),
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
    // TODO: Use an upload buffer
    assert_ret_val(size, nullptr);

    if (!this->mapped_mem)
    {
        this->mapped_mem = std::make_unique<std::vector<uint8_t>>();
    }

    this->mapped_mem->resize(size);
    return this->mapped_mem->data();
}

void ff::dx12::buffer::unmap(ff::dxgi::command_context_base& context)
{
    assert_ret(this->mapped_mem);
    this->update(context, this->mapped_mem->data(), this->mapped_mem->size());
}

bool ff::dx12::buffer::reset()
{
    *this = ff::dx12::buffer(
        nullptr,
        this->type(),
        this->initial_data ? this->initial_data->data() : nullptr,
        this->initial_data ? this->initial_data->size() : 0,
        0, // hash
        this->version_ + 1,
        this->initial_data,
        std::move(this->mapped_mem));

    return !this->initial_data || *this;
}

ff::dx12::buffer_upload::buffer_upload(ff::dxgi::buffer_type type)
    : ff::dx12::buffer_base(type)
{
}

bool ff::dx12::buffer_upload::valid() const
{
    return this->mem_gpu_address != 0 && this->mem_size > 0;
}

size_t ff::dx12::buffer_upload::version() const
{
    return this->version_;
}

D3D12_GPU_VIRTUAL_ADDRESS ff::dx12::buffer_upload::gpu_address() const
{
    return this->mem_gpu_address;
}

ff::dx12::residency_data* ff::dx12::buffer_upload::residency_data()
{
    return this->mem_residency_data;
}

size_t ff::dx12::buffer_upload::size() const
{
    return static_cast<size_t>(this->mem_size);
}

bool ff::dx12::buffer_upload::writable() const
{
    return true;
}

bool ff::dx12::buffer_upload::update(ff::dxgi::command_context_base& context, const void* data, size_t size)
{
    assert_ret_val(size, false);
    std::memcpy(this->map(context, size), data, size);
    return true;
}

void* ff::dx12::buffer_upload::map(ff::dxgi::command_context_base& context, size_t size)
{
    assert_ret_val(size, nullptr);
    this->version_++;

    ff::dx12::commands& commands = ff::dx12::commands::get(context);
    ff::dx12::mem_range mem = ff::dx12::upload_allocator().alloc_buffer(size, commands.next_fence_value());

    this->mem_residency_data = mem.residency_data();
    this->mem_gpu_address = mem.gpu_data();
    this->mem_size = mem.size();

    return mem.cpu_data();
}

void ff::dx12::buffer_upload::unmap(ff::dxgi::command_context_base& context)
{
}

ff::dx12::buffer_cpu::buffer_cpu(ff::dxgi::buffer_type type)
    : ff::dx12::buffer_base(type)
    , data_hash(0)
    , version_(1)
{
}

const uint8_t* ff::dx12::buffer_cpu::data() const
{
    return this->valid() ? this->data_.data() : nullptr;
}

std::shared_ptr<ff::data_base> ff::dx12::buffer_cpu::subdata(size_t offset, size_t size) const
{
    return std::make_shared<ff::data_static>(this->data() + offset, size);
}

bool ff::dx12::buffer_cpu::valid() const
{
    return this->size() > 0;
}

size_t ff::dx12::buffer_cpu::version() const
{
    return this->version_;
}

size_t ff::dx12::buffer_cpu::size() const
{
    return this->data_.size();
}

bool ff::dx12::buffer_cpu::writable() const
{
    return true;
}

bool ff::dx12::buffer_cpu::update(ff::dxgi::command_context_base& context, const void* data, size_t size)
{
    assert_ret_val(size, false);
    size_t new_hash = ff::stable_hash_bytes(data, size);
    check_ret_val(new_hash != this->data_hash, false);

    std::memcpy(this->map(context, size), data, size);
    this->data_hash = new_hash;
    this->version_++;

    return true;
}

void* ff::dx12::buffer_cpu::map(ff::dxgi::command_context_base& context, size_t size)
{
    assert_ret_val(size, nullptr);
    this->data_.resize(size);
    return this->data_.data();
}

void ff::dx12::buffer_cpu::unmap(ff::dxgi::command_context_base& context)
{
    size_t new_hash = ff::stable_hash_bytes(this->data(), this->size());
    if (new_hash != this->data_hash)
    {
        this->data_hash = new_hash;
        this->version_++;
    }
}
