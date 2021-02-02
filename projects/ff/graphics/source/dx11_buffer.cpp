#include "pch.h"
#include "dx11_buffer.h"
#include "dx11_device_state.h"
#include "graphics.h"

ff::dx11_buffer::dx11_buffer(D3D11_BIND_FLAG type)
    : dx11_buffer(type, 32)
{}

ff::dx11_buffer::dx11_buffer(D3D11_BIND_FLAG type, size_t size)
{
    ff::graphics::internal::add_child(this);

    bool status = this->init(type, size, nullptr);
    assert(status);
}

ff::dx11_buffer::dx11_buffer(D3D11_BIND_FLAG type, std::shared_ptr<ff::data_base> read_only_data)
{
    ff::graphics::internal::add_child(this);

    bool status = this->init(type, read_only_data->size(), read_only_data);
    assert(status);
}

ff::dx11_buffer::~dx11_buffer()
{
    this->unmap();

    ff::graphics::internal::remove_child(this);
}

ID3D11Buffer* ff::dx11_buffer::buffer() const
{
    return this->buffer_.Get();
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
    this->unmap();

    if (this->writable())
    {
        if (size > this->size())
        {
            size = ff::math::nearest_power_of_two(size);

            dx11_buffer new_buffer(this->type(), size);
            if (!new_buffer.buffer())
            {
                assert(false);
                return nullptr;
            }

            *this = std::move(new_buffer);
        }

        this->mapped_device = ff::graphics::internal::dx11_device();
        return ff::graphics::internal::dx11_device_state().map(this->buffer_.Get(), D3D11_MAP_WRITE_DISCARD);
    }

    assert(false);
    return nullptr;
}

void ff::dx11_buffer::unmap()
{
    if (this->mapped_device)
    {
        ff::graphics::internal::dx11_device_state().unmap(this->buffer_.Get());
        this->mapped_device.Reset();
    }
}

bool ff::dx11_buffer::reset()
{
    return this->init(this->type(), this->size(), this->initial_data);
}

bool ff::dx11_buffer::init(D3D11_BIND_FLAG type, size_t size, std::shared_ptr<ff::data_base> read_only_data)
{
    this->unmap();

    this->initial_data = read_only_data;
    this->buffer_.Reset();

    UINT bind_flags = type;
    UINT cpu_flags = !read_only_data ? D3D11_CPU_ACCESS_WRITE : 0;
    D3D11_USAGE usage = !read_only_data ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;

    D3D11_SUBRESOURCE_DATA data{};
    if (read_only_data)
    {
        data.pSysMem = read_only_data->data();
        data.SysMemPitch = static_cast<UINT>(read_only_data->size());
        data.SysMemSlicePitch = 0;
    }

    CD3D11_BUFFER_DESC desc(static_cast<UINT>(size), bind_flags, usage, cpu_flags);
    return SUCCEEDED(ff::graphics::internal::dx11_device()->CreateBuffer(&desc, data.pSysMem ? &data : nullptr, &this->buffer_));
}
