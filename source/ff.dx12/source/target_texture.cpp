#include "pch.h"
#include "commands.h"
#include "descriptor_allocator.h"
#include "device_reset_priority.h"
#include "globals.h"
#include "target_texture.h"
#include "texture.h"
#include "texture_util.h"
#include "queue.h"

ff::dx12::target_texture::target_texture(
    const std::shared_ptr<ff::dx12::texture>& texture,
    size_t array_start,
    size_t array_count,
    size_t mip_level)
    : texture_(texture)
    , view_(ff::dx12::cpu_target_descriptors().alloc_range(1))
    , array_start(array_start)
    , array_count(array_count ? array_count : texture->array_size() - array_start)
    , mip_level(mip_level)
{
    this->reset();
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::target_texture::target_texture(target_texture&& other) noexcept
{
    *this = std::move(other);
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::target_texture::~target_texture()
{
    ff::dx12::remove_device_child(this);
}

ff::dx12::target_texture::operator bool() const
{
    return this->view_;
}

const std::shared_ptr<ff::dx12::texture>& ff::dx12::target_texture::shared_texture() const
{
    return this->texture_;
}

void ff::dx12::target_texture::clear(ff::dxgi::command_context_base& context, const DirectX::XMFLOAT4& clear_color)
{
    ff::dx12::commands::get(context).clear(*this, clear_color);
}

bool ff::dx12::target_texture::pre_render(const DirectX::XMFLOAT4* clear_color)
{
    if (*this)
    {
        if (clear_color)
        {
            this->clear(ff::dx12::frame_commands(), *clear_color);
        }
        else
        {
            ff::dx12::frame_commands().discard(*this);
        }

        return true;
    }

    return false;
}

bool ff::dx12::target_texture::present()
{
    ff::dx12::frame_commands().resource_state(*this->texture_->resource(), D3D12_RESOURCE_STATE_PRESENT);

    this->render_presented_.notify(this);

    return true;
}

ff::signal_sink<ff::dxgi::target_base*>& ff::dx12::target_texture::render_presented()
{
    return this->render_presented_;
}

ff::dxgi::target_access_base& ff::dx12::target_texture::target_access()
{
    return *this;
}

size_t ff::dx12::target_texture::target_array_start() const
{
    return this->array_start;
}

size_t ff::dx12::target_texture::target_array_size() const
{
    return this->array_count;
}

size_t ff::dx12::target_texture::target_mip_start() const
{
    return this->mip_level;
}

size_t ff::dx12::target_texture::target_mip_size() const
{
    return 1;
}

DXGI_FORMAT ff::dx12::target_texture::format() const
{
    return this->texture_->format();
}

ff::window_size ff::dx12::target_texture::size() const
{
    return ff::window_size{ this->texture_->size(), 1.0, DMDO_DEFAULT, DMDO_DEFAULT };
}

ff::dx12::resource& ff::dx12::target_texture::dx12_target_texture()
{
    return *this->texture_->resource();
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::dx12::target_texture::dx12_target_view()
{
    return this->view_.cpu_handle(0);
}

bool ff::dx12::target_texture::reset()
{
    ff::dx12::create_target_view(&this->dx12_target_texture(), this->dx12_target_view(), this->array_start, this->array_count, this->mip_level);
    return *this;
}
