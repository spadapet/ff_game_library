#include "pch.h"
#include "buffer.h"
#include "commands.h"
#include "device_reset_priority.h"
#include "globals.h"
#include "resource.h"

ff::dx12::buffer::buffer(ff::dxgi::buffer_type type, std::shared_ptr<ff::data_base> initial_data)
    : buffer(nullptr, type, initial_data ? initial_data->data() : nullptr, initial_data ? initial_data->size() : 0, initial_data, {})
{}

ff::dx12::buffer::buffer(
    ff::dx12::commands* commands,
    ff::dxgi::buffer_type type,
    const void* data,
    uint64_t data_size,
    std::shared_ptr<ff::data_base> initial_data,
    std::unique_ptr<std::vector<uint8_t>> mapped_mem)
    : initial_data(initial_data)
    , mapped_mem(std::move(mapped_mem))
    , mapped_context(nullptr)
    , type_(type)
{
    if (data && data_size)
    {
        this->resource_ = std::make_unique<ff::dx12::resource>(std::shared_ptr<ff::dx12::mem_range>(), CD3DX12_RESOURCE_DESC::Buffer(data_size));
        this->resource_->update_buffer(commands, data, 0, data_size);
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
    return true;
}

ff::dx12::resource* ff::dx12::buffer::resource()
{
    return this->resource_.get();
}

ff::dxgi::buffer_type ff::dx12::buffer::type() const
{
    return this->type_;
}

size_t ff::dx12::buffer::size() const
{
    return this->resource_
        ? static_cast<size_t>(this->resource_->mem_range()
            ? this->resource_->mem_range()->size()
            : this->resource_->alloc_info().SizeInBytes)
        : 0;
}

bool ff::dx12::buffer::writable() const
{
    return true;
}

bool ff::dx12::buffer::update(ff::dxgi::command_context_base& context, const void* data, size_t data_size, size_t min_buffer_size)
{
    *this = ff::dx12::buffer(&ff::dx12::commands::get(context), this->type_, data, data_size, this->initial_data, std::move(this->mapped_mem));
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
        *this = ff::dx12::buffer(&commands, this->type_, data, data_size, this->initial_data, std::move(this->mapped_mem));
    }
}

bool ff::dx12::buffer::reset()
{
    *this = ff::dx12::buffer(nullptr, this->type_,
        this->initial_data ? this->initial_data->data() : nullptr,
        this->initial_data ? this->initial_data->size() : 0,
        this->initial_data, std::move(this->mapped_mem));

    return *this;
}
