#include "pch.h"
#include "buffer_cpu.h"

ff::dx12::buffer_cpu::buffer_cpu(ff::dxgi::buffer_type type)
    : type_(type)
    , data_hash(0)
    , version_(1)
{}

ff::dx12::buffer_cpu::operator bool() const
{
    return this->size() > 0;
}

const std::vector<uint8_t>& ff::dx12::buffer_cpu::data() const
{
    return this->data_;
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
