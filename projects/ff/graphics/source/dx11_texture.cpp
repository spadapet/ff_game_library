#include "pch.h"
#include "dx11_texture.h"
#include "sprite_data.h"
#include "sprite_type.h"

ff::dx11_texture_o::dx11_texture_o(const std::filesystem::path& path, DXGI_FORMAT new_format, size_t new_mip_count)
{
}

ff::dx11_texture_o::dx11_texture_o(ff::point_int size, DXGI_FORMAT format, size_t mip_count, size_t array_size, size_t sample_count)
{
}

ff::dx11_texture_o::dx11_texture_o(ff::point_int size, const std::shared_ptr<ff::palette_base>& palette, size_t array_size, size_t sample_count)
{
}

ff::dx11_texture_o::dx11_texture_o(const std::shared_ptr<DirectX::ScratchImage>& data, const std::shared_ptr<ff::palette_base>& palette)
{
}

ff::dx11_texture_o::dx11_texture_o(const dx11_texture_o& other, DXGI_FORMAT new_format, size_t new_mip_count)
{
}

ff::dx11_texture_o::dx11_texture_o(const dx11_texture_o& other)
{}

ff::point_int ff::dx11_texture_o::size() const
{
    return ff::point_int();
}

size_t ff::dx11_texture_o::mip_count() const
{
    return size_t();
}

size_t ff::dx11_texture_o::array_size() const
{
    return size_t();
}

size_t ff::dx11_texture_o::sample_count() const
{
    return size_t();
}

DXGI_FORMAT ff::dx11_texture_o::format() const
{
    return DXGI_FORMAT();
}

ff::sprite_type ff::dx11_texture_o::sprite_type() const
{
    return ff::sprite_type();
}

const std::shared_ptr<ff::palette_base>& ff::dx11_texture_o::palette() const
{
    return this->palette_;
}

ID3D11Texture2D* ff::dx11_texture_o::texture() const
{
    return nullptr;
}

void ff::dx11_texture_o::update(
    size_t array_index,
    size_t mip_index,
    const ff::rect_int& rect,
    const void* data,
    DXGI_FORMAT data_format,
    bool update_local_cache)
{
}

std::shared_ptr<DirectX::ScratchImage> ff::dx11_texture_o::capture(bool use_local_cache)
{
    return std::shared_ptr<DirectX::ScratchImage>();
}

ff::dict ff::dx11_texture_o::resource_get_siblings(const std::shared_ptr<resource>& self) const
{
    return ff::dict();
}

bool ff::dx11_texture_o::resource_save_to_file(const std::filesystem::path& directory_path, std::string_view name) const
{
    return false;
}

bool ff::dx11_texture_o::reset()
{
    return false;
}

const ff::dx11_texture_o* ff::dx11_texture_o::view_texture() const
{
    return this;
}

ID3D11ShaderResourceView* ff::dx11_texture_o::view() const
{
    return nullptr;
}

const ff::sprite_data& ff::dx11_texture_o::sprite_data() const
{
    static ff::sprite_data empty_sprite_data("", nullptr, ff::rect_float::zeros(), ff::rect_float::zeros(), ff::sprite_type::unknown);
    return empty_sprite_data;
}

float ff::dx11_texture_o::frame_length() const
{
    return 0.0f;
}

float ff::dx11_texture_o::frames_per_second() const
{
    return 0.0f;
}

void ff::dx11_texture_o::frame_events(float start, float end, bool include_start, ff::push_base<ff::animation_event>& events)
{
}

void ff::dx11_texture_o::render_frame(ff::renderer_base& render, const ff::transform& transform, float frame, const ff::dict* params)
{
}

ff::value_ptr ff::dx11_texture_o::frame_value(size_t value_id, float frame, const ff::dict* params)
{
    return ff::value_ptr();
}

void ff::dx11_texture_o::advance_animation(ff::push_base<ff::animation_event>* events)
{
}

void ff::dx11_texture_o::render_animation(ff::renderer_base& render, const ff::transform& transform) const
{
}

float ff::dx11_texture_o::animation_frame() const
{
    return 0.0f;
}

const ff::animation_base* ff::dx11_texture_o::animation() const
{
    return this;
}

bool ff::dx11_texture_o::save_to_cache(ff::dict& dict, bool& allow_compress) const
{
    return false;
}

std::shared_ptr<ff::resource_object_base> ff::internal::texture_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    return nullptr;
}

std::shared_ptr<ff::resource_object_base> ff::internal::texture_factory::load_from_cache(const ff::dict& dict) const
{
    return nullptr;
}
