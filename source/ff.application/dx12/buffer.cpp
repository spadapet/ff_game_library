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

ff::dx12::resource* ff::dx12::buffer_base::resource()
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

ff::dx12::buffer_gpu_static::buffer_gpu_static(ff::dxgi::buffer_type type, std::shared_ptr<ff::data_base> static_data)
    : ff::dx12::buffer_base(type)
    , static_data(static_data)
    , resource_("StaticBuffer", std::shared_ptr<ff::dx12::mem_range>(), CD3DX12_RESOURCE_DESC::Buffer(static_data->size()))
{
    this->reset();
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::buffer_gpu_static::buffer_gpu_static(buffer_gpu_static&& other) noexcept
    : ff::dx12::buffer_base(other.type())
    , static_data(std::move(other.static_data))
    , resource_(std::move(other.resource_))
{
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::buffer_gpu_static::~buffer_gpu_static()
{
    ff::dx12::remove_device_child(this);
}

bool ff::dx12::buffer_gpu_static::valid() const
{
    return this->resource_;
}

ff::dx12::resource* ff::dx12::buffer_gpu_static::resource()
{
    return &this->resource_;
}

D3D12_GPU_VIRTUAL_ADDRESS ff::dx12::buffer_gpu_static::gpu_address() const
{
    return this->resource_.gpu_address();
}

size_t ff::dx12::buffer_gpu_static::size() const
{
    return this->static_data->size(); // resource size may be larger
}

ff::dx12::residency_data* ff::dx12::buffer_gpu_static::residency_data()
{
    return this->resource_.residency_data();
}

bool ff::dx12::buffer_gpu_static::reset()
{
    assert_ret_val(this->static_data && this->static_data->size(), false);
    this->resource_.update_buffer(nullptr, this->static_data->data(), 0, this->static_data->size());
    return this->valid();
}

ff::dx12::buffer_gpu::buffer_gpu(ff::dxgi::buffer_type type)
    : ff::dx12::buffer_base(type)
{
}

bool ff::dx12::buffer_gpu::valid() const
{
    return this->resource_ != nullptr;
}

size_t ff::dx12::buffer_gpu::version() const
{
    return this->version_;
}

ff::dx12::resource* ff::dx12::buffer_gpu::resource()
{
    return this->resource_.get();
}

D3D12_GPU_VIRTUAL_ADDRESS ff::dx12::buffer_gpu::gpu_address() const
{
    return this->valid() ? this->resource_->gpu_address() : 0;
}

ff::dx12::residency_data* ff::dx12::buffer_gpu::residency_data()
{
    return this->valid() ? this->resource_->residency_data() : nullptr;
}

size_t ff::dx12::buffer_gpu::size() const
{
    return this->valid() ? this->resource_->desc().Width : 0;
}

bool ff::dx12::buffer_gpu::writable() const
{
    return true;
}

bool ff::dx12::buffer_gpu::update(ff::dxgi::command_context_base& context, const void* data, size_t size)
{
    assert_ret_val(size, false);

    size_t new_hash = 0;
    if (size <= 0x10000) // for a lot of data, forget about checking if it changed
    {
        new_hash = ff::stable_hash_bytes(data, size);
        check_ret_val(new_hash != this->data_hash, false);
    }

    std::memcpy(this->map(context, size), data, size);
    this->unmap(context);
    this->data_hash = new_hash;

    return this->valid();
}

void* ff::dx12::buffer_gpu::map(ff::dxgi::command_context_base& context, size_t size)
{
    assert_ret_val(size, nullptr);
    assert_msg(!this->mapped_memory, "Forgot to unmap buffer");

    if (size > this->size())
    {
        CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(std::max(this->size() * 2, size));
        this->resource_ = std::make_unique<ff::dx12::resource>("Buffer", std::shared_ptr<ff::dx12::mem_range>(), desc);
    }

    ff::dx12::commands& commands = ff::dx12::commands::get(context);
    this->mapped_memory = ff::dx12::upload_allocator().alloc_buffer(size, commands.next_fence_value());
    this->data_hash = 0;
    this->version_++;

    return this->mapped_memory.cpu_data();
}

void ff::dx12::buffer_gpu::unmap(ff::dxgi::command_context_base& context)
{
    assert_ret(this->resource_);
    this->resource_->update_buffer(&ff::dx12::commands::get(context), this->mapped_memory.cpu_data(), 0, this->mapped_memory.size());
    this->mapped_memory = {};
}

ff::dx12::buffer_upload::buffer_upload(ff::dxgi::buffer_type type)
    : ff::dx12::buffer_base(type)
{
}

bool ff::dx12::buffer_upload::valid() const
{
    return this->mapped_memory;
}

size_t ff::dx12::buffer_upload::version() const
{
    return this->version_;
}

D3D12_GPU_VIRTUAL_ADDRESS ff::dx12::buffer_upload::gpu_address() const
{
    return this->valid() ? this->mapped_memory.gpu_data() : 0;
}

ff::dx12::residency_data* ff::dx12::buffer_upload::residency_data()
{
    return this->valid() ? this->mapped_memory.residency_data() : nullptr;
}

size_t ff::dx12::buffer_upload::size() const
{
    return this->valid() ? this->mapped_memory.size() : 0;
}

bool ff::dx12::buffer_upload::writable() const
{
    return true;
}

bool ff::dx12::buffer_upload::update(ff::dxgi::command_context_base& context, const void* data, size_t size)
{
    assert_ret_val(size, false);
    std::memcpy(this->map(context, size), data, size);
    this->unmap(context);
    return true;
}

void* ff::dx12::buffer_upload::map(ff::dxgi::command_context_base& context, size_t size)
{
    assert_ret_val(size, nullptr);

    ff::dx12::commands& commands = ff::dx12::commands::get(context);
    this->mapped_memory = ff::dx12::upload_allocator().alloc_buffer(size, commands.next_fence_value());
    this->version_++;

    return this->mapped_memory.cpu_data();
}

void ff::dx12::buffer_upload::unmap(ff::dxgi::command_context_base& context)
{
    this->mapped_memory = {};
}

ff::dx12::buffer_cpu::buffer_cpu(ff::dxgi::buffer_type type)
    : ff::dx12::buffer_base(type)
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

    size_t new_hash = 0;
    if (size <= 0x10000) // for a lot of data, forget about checking if it changed
    {
        new_hash = ff::stable_hash_bytes(data, size);
        check_ret_val(new_hash != this->data_hash, false);
    }

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
