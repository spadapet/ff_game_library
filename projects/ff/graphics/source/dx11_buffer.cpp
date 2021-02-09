#include "pch.h"
#include "dx11_buffer.h"
#include "dx11_device_state.h"
#include "graphics.h"

static const size_t MIN_BUFFER_SIZE = 4;

ff::dx11_buffer::dx11_buffer(D3D11_BIND_FLAG type)
    : dx11_buffer(type, 0)
{}

ff::dx11_buffer::dx11_buffer(D3D11_BIND_FLAG type, size_t size)
    : dx11_buffer(type, size, nullptr)
{}

ff::dx11_buffer::dx11_buffer(D3D11_BIND_FLAG type, std::shared_ptr<ff::data_base> read_only_data)
    : dx11_buffer(type, 0, read_only_data)
{}

ff::dx11_buffer::dx11_buffer(D3D11_BIND_FLAG type, size_t size, std::shared_ptr<ff::data_base> read_only_data)
    : initial_data(read_only_data)
{
    ff::graphics::internal::add_child(this);

    UINT bind_flags = type;
    UINT cpu_flags = !read_only_data ? D3D11_CPU_ACCESS_WRITE : 0;
    D3D11_USAGE usage = !read_only_data ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA data{};

    size = std::max(size, ::MIN_BUFFER_SIZE);

    if (read_only_data)
    {
        size = std::max(size, read_only_data->size());

        data.pSysMem = read_only_data->data();
        data.SysMemPitch = static_cast<UINT>(read_only_data->size());
        data.SysMemSlicePitch = 0;
    }

    CD3D11_BUFFER_DESC desc(static_cast<UINT>(size), bind_flags, usage, cpu_flags);
    HRESULT hr = ff::graphics::dx11_device()->CreateBuffer(&desc, data.pSysMem ? &data : nullptr, this->buffer_.GetAddressOf());
    assert(SUCCEEDED(hr));
}

ff::dx11_buffer::~dx11_buffer()
{
    this->unmap();

    ff::graphics::internal::remove_child(this);
}

ff::dx11_buffer::operator bool() const
{
    return this->buffer_;
}

D3D11_BIND_FLAG ff::dx11_buffer::type() const
{
    D3D11_BUFFER_DESC desc;
    this->buffer_->GetDesc(&desc);
    return static_cast<D3D11_BIND_FLAG>(desc.BindFlags);
}

size_t ff::dx11_buffer::size() const
{
    D3D11_BUFFER_DESC desc;
    this->buffer_->GetDesc(&desc);
    return static_cast<size_t>(desc.ByteWidth);
}

bool ff::dx11_buffer::writable() const
{
    D3D11_BUFFER_DESC desc;
    this->buffer_->GetDesc(&desc);
    return (desc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) != 0;
}

void* ff::dx11_buffer::map(size_t size)
{
    if (this->writable())
    {
        this->unmap();

        if (size > this->size())
        {
            size = ff::math::nearest_power_of_two(size);

            dx11_buffer new_buffer(this->type(), size);
            if (!new_buffer)
            {
                assert(false);
                return nullptr;
            }

            std::swap(*this, new_buffer);
        }

        this->mapped_device = ff::graphics::dx11_device();
        return ff::graphics::dx11_device_state().map(this->buffer_.Get(), D3D11_MAP_WRITE_DISCARD);
    }

    assert(false);
    return nullptr;
}

void ff::dx11_buffer::unmap()
{
    if (this->mapped_device)
    {
        ff::graphics::dx11_device_state().unmap(this->buffer_.Get());
        this->mapped_device.Reset();
    }
}

ID3D11Buffer* ff::dx11_buffer::buffer() const
{
    return this->buffer_.Get();
}

bool ff::dx11_buffer::reset()
{
    *this = this->initial_data
        ? dx11_buffer(this->type(), this->initial_data)
        : dx11_buffer(this->type());

    return *this;
}
