#include "pch.h"
#include "buffer.h"
#include "device_reset_priority.h"
#include "device_state.h"
#include "globals.h"

static const size_t MIN_BUFFER_SIZE = 16;

ff::dx11::buffer::buffer(ff::dxgi::buffer_type type)
    : buffer(type, 0)
{}

ff::dx11::buffer::buffer(ff::dxgi::buffer_type type, size_t size)
    : buffer(type, size, nullptr, true)
{}

ff::dx11::buffer::buffer(ff::dxgi::buffer_type type, std::shared_ptr<ff::data_base> initial_data, bool writable)
    : buffer(type, 0, initial_data, writable)
{}

ff::dx11::buffer::buffer(ff::dxgi::buffer_type type, size_t size, std::shared_ptr<ff::data_base> initial_data, bool writable)
    : initial_data(initial_data)
    , mapped_context(nullptr)
{
    ff::dx11::add_device_child(this, ff::dx11::device_reset_priority::normal);

    UINT bind_flags;
    switch (type)
    {
        default: assert(false); [[fallthrough]];
        case ff::dxgi::buffer_type::constant: bind_flags = D3D11_BIND_CONSTANT_BUFFER; break;
        case ff::dxgi::buffer_type::index: bind_flags = D3D11_BIND_INDEX_BUFFER; break;
        case ff::dxgi::buffer_type::vertex: bind_flags = D3D11_BIND_VERTEX_BUFFER; break;

    }

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

ff::dx11::buffer::~buffer()
{
    this->unmap();

    ff::dx11::remove_device_child(this);
}

ff::dx11::buffer& ff::dx11::buffer::get(ff::dxgi::buffer_base& obj)
{
    return static_cast<ff::dx11::buffer&>(obj);
}

const ff::dx11::buffer& ff::dx11::buffer::get(const ff::dxgi::buffer_base& obj)
{
    return static_cast<const ff::dx11::buffer&>(obj);
}

ff::dx11::buffer::operator bool() const
{
    return this->buffer_;
}

ID3D11Buffer* ff::dx11::buffer::dx_buffer() const
{
    return this->buffer_.Get();
}

ff::dxgi::buffer_type ff::dx11::buffer::type() const
{
    D3D11_BUFFER_DESC desc;
    this->buffer_->GetDesc(&desc);

    switch (static_cast<D3D11_BIND_FLAG>(desc.BindFlags))
    {
        default: assert(false); return ff::dxgi::buffer_type::none;
        case D3D11_BIND_VERTEX_BUFFER: return ff::dxgi::buffer_type::vertex;
        case D3D11_BIND_INDEX_BUFFER: return ff::dxgi::buffer_type::index;
        case D3D11_BIND_CONSTANT_BUFFER: return ff::dxgi::buffer_type::constant;
    }
}

size_t ff::dx11::buffer::size() const
{
    D3D11_BUFFER_DESC desc;
    this->buffer_->GetDesc(&desc);
    return static_cast<size_t>(desc.ByteWidth);
}

bool ff::dx11::buffer::writable() const
{
    D3D11_BUFFER_DESC desc;
    this->buffer_->GetDesc(&desc);
    return (desc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) != 0;
}

bool ff::dx11::buffer::update(ff::dxgi::command_context_base& context, const void* data, size_t data_size, size_t min_buffer_size)
{
    void* mapped_data = this->map(context, std::max(data_size, min_buffer_size));
    if (mapped_data)
    {
        std::memcpy(mapped_data, data, data_size);
        this->unmap();
        return true;
    }

    return false;
}

void* ff::dx11::buffer::map(ff::dxgi::command_context_base& context, size_t size)
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

        this->mapped_context = &context;
        return ff::dx11::device_state::get(context).map(this->buffer_.Get());
    }

    assert(false);
    return nullptr;
}

void ff::dx11::buffer::unmap()
{
    if (this->mapped_context)
    {
        ff::dx11::device_state::get(*this->mapped_context).unmap(this->buffer_.Get());
        this->mapped_context = nullptr;
    }
}

bool ff::dx11::buffer::reset()
{
    *this = this->initial_data
        ? buffer(this->type(), this->initial_data, this->writable())
        : buffer(this->type());

    return *this;
}
