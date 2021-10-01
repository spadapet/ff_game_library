#include "pch.h"
#include "buffer.h"

#if DXVER == 11

static const size_t MIN_BUFFER_SIZE = 16;

ff::buffer::buffer(D3D11_BIND_FLAG type)
    : buffer(type, 0)
{}

ff::buffer::buffer(D3D11_BIND_FLAG type, size_t size)
    : buffer(type, size, nullptr, true)
{}

ff::buffer::buffer(D3D11_BIND_FLAG type, std::shared_ptr<ff::data_base> initial_data, bool writable)
    : buffer(type, 0, initial_data, writable)
{}

ff::buffer::buffer(D3D11_BIND_FLAG type, size_t size, std::shared_ptr<ff::data_base> initial_data, bool writable)
    : initial_data(initial_data)
{
    ff::internal::dx11::add_device_child(this, ff::internal::dx11::device_reset_priority::normal);

    UINT bind_flags = type;
    UINT cpu_flags = writable ? D3D11_CPU_ACCESS_WRITE : 0;
    D3D11_USAGE usage = writable ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA data{};

    size = std::max(size, ::MIN_BUFFER_SIZE);

    if (this->initial_data)
    {
        size = std::max(size, this->initial_data->size());

        data.pSysMem = this->initial_data->data();
        data.SysMemPitch = static_cast<UINT>(this->initial_data->size());
        data.SysMemSlicePitch = 0;
    }

    CD3D11_BUFFER_DESC desc(static_cast<UINT>(size), bind_flags, usage, cpu_flags);
    HRESULT hr = ff::dx11::device()->CreateBuffer(&desc, data.pSysMem ? &data : nullptr, this->buffer_.GetAddressOf());
    assert(SUCCEEDED(hr));
}

ff::buffer::~buffer()
{
    this->unmap();

    ff::internal::dx11::remove_device_child(this);
}

ff::buffer::operator bool() const
{
    return this->buffer_;
}

D3D11_BIND_FLAG ff::buffer::type() const
{
    D3D11_BUFFER_DESC desc;
    this->buffer_->GetDesc(&desc);
    return static_cast<D3D11_BIND_FLAG>(desc.BindFlags);
}

size_t ff::buffer::size() const
{
    D3D11_BUFFER_DESC desc;
    this->buffer_->GetDesc(&desc);
    return static_cast<size_t>(desc.ByteWidth);
}

bool ff::buffer::writable() const
{
    D3D11_BUFFER_DESC desc;
    this->buffer_->GetDesc(&desc);
    return (desc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) != 0;
}

void* ff::buffer::map(size_t size)
{
    if (this->writable())
    {
        this->unmap();

        if (size > this->size())
        {
            size = ff::math::nearest_power_of_two(size);

            buffer new_buffer(this->type(), size);
            if (!new_buffer)
            {
                assert(false);
                return nullptr;
            }

            std::swap(*this, new_buffer);
        }

        this->mapped_device = ff::dx11::device();
        return ff::dx11::get_device_state().map(this->buffer_.Get(), D3D11_MAP_WRITE_DISCARD);
    }

    assert(false);
    return nullptr;
}

void ff::buffer::unmap()
{
    if (this->mapped_device)
    {
        ff::dx11::get_device_state().unmap(this->buffer_.Get());
        this->mapped_device.Reset();
    }
}

bool ff::buffer::update_discard(const void* data, size_t size)
{
    return this->update_discard(data, size, size);
}

bool ff::buffer::update_discard(const void* data, size_t data_size, size_t buffer_size)
{
    void* mapped_data = this->map(std::max(data_size, buffer_size));
    if (mapped_data)
    {
        std::memcpy(mapped_data, data, data_size);
        this->unmap();
        return true;
    }

    return false;
}

ID3D11Buffer* ff::buffer::dx_buffer() const
{
    return this->buffer_.Get();
}

bool ff::buffer::reset()
{
    *this = this->initial_data
        ? buffer(this->type(), this->initial_data, this->writable())
        : buffer(this->type());

    return *this;
}

#endif
