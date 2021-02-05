#include "pch.h"
#include "dx11_texture.h"
#include "dxgi_util.h"
#include "graphics.h"
#include "renderer_base.h"
#include "sprite_data.h"
#include "sprite_type.h"
#include "texture_util.h"

ff::dx11_texture_o::dx11_texture_o(const std::filesystem::path& path, DXGI_FORMAT new_format, size_t new_mip_count)
{
    DirectX::ScratchImage palette;
    DirectX::ScratchImage data = ff::internal::load_texture_data(path, new_format, new_mip_count, &palette);
    ff::sprite_type sprite_type = ff::sprite_type::unknown;

    if (data.GetImageCount())
    {
        sprite_type = ff::internal::get_sprite_type(data);
        this->data_ = std::make_shared<DirectX::ScratchImage>(std::move(data));
    }

    if (palette.GetImageCount())
    {
        this->palette_ = std::make_shared<DirectX::ScratchImage>(std::move(palette));
    }

    this->fix_sprite_data(sprite_type);

    ff::graphics::internal::add_child(this);
}

ff::dx11_texture_o::dx11_texture_o(ff::point_int size, DXGI_FORMAT format, size_t mip_count, size_t array_size, size_t sample_count)
{
    if (format == DXGI_FORMAT_UNKNOWN)
    {
        format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    else if (ff::internal::compressed_format(format) && (size.x % 4 || size.y % 4))
    {
        // compressed textures must have sizes that are multiples of four
        format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    if (size.x > 0 && size.y > 0 && mip_count > 0 && array_size > 0 && sample_count > 0)
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(size.x);
        desc.Height = static_cast<UINT>(size.y);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (!ff::internal::compressed_format(format) ? D3D11_BIND_RENDER_TARGET : 0);
        desc.Format = format;
        desc.MipLevels = static_cast<UINT>(mip_count);
        desc.ArraySize = static_cast<UINT>(array_size);
        desc.SampleDesc.Count = static_cast<UINT>(ff::graphics::fix_sample_count(format, sample_count));

        HRESULT hr = ff::graphics::internal::dx11_device()->CreateTexture2D(&desc, nullptr, this->texture_.GetAddressOf());
        assert(SUCCEEDED(hr));
    }

    const ff::sprite_type sprite_type = ff::internal::palette_format(format)
        ? ff::sprite_type::opaque_palette
        : (DirectX::HasAlpha(format) ? ff::sprite_type::transparent : ff::sprite_type::opaque);

    this->fix_sprite_data(sprite_type);

    ff::graphics::internal::add_child(this);
}

ff::dx11_texture_o::dx11_texture_o(
    const std::shared_ptr<DirectX::ScratchImage>& data,
    const std::shared_ptr<DirectX::ScratchImage>& palette,
    ff::sprite_type sprite_type)
    : data_(data)
    , palette_(palette)
{
    this->fix_sprite_data(sprite_type == ff::sprite_type::unknown ? ff::internal::get_sprite_type(*this->data_) : sprite_type);

    ff::graphics::internal::add_child(this);
}

ff::dx11_texture_o::dx11_texture_o(const dx11_texture_o& other, DXGI_FORMAT new_format, size_t new_mip_count)
{
    std::shared_ptr<DirectX::ScratchImage> data = other.capture();
    ff::sprite_type sprite_type = ff::sprite_type::unknown;

    if (data && data->GetImageCount())
    {
        DirectX::ScratchImage new_data = ff::internal::convert_texture_data(*data, new_format, new_mip_count);
        if (new_data.GetImageCount())
        {
            sprite_type = ff::internal::get_sprite_type(new_data);
            this->data_ = std::make_shared<DirectX::ScratchImage>(std::move(new_data));
        }
    }

    if (ff::internal::palette_format(new_format))
    {
        this->palette_ = other.palette_;
    }

    this->fix_sprite_data(sprite_type);

    ff::graphics::internal::add_child(this);
}

ff::dx11_texture_o::dx11_texture_o(dx11_texture_o&& other) noexcept
    : texture_(std::move(other.texture_))
    , view_(std::move(other.view_))
    , data_(std::move(other.data_))
    , palette_(std::move(other.palette_))
{
    this->fix_sprite_data(other.sprite_data_.type());
    other.sprite_data_ = ff::sprite_data();

    ff::graphics::internal::add_child(this);
}

ff::dx11_texture_o::~dx11_texture_o()
{
    ff::graphics::internal::remove_child(this);
}

ff::dx11_texture_o& ff::dx11_texture_o::operator=(dx11_texture_o&& other) noexcept
{
    if (this != &other)
    {
        this->texture_ = std::move(other.texture_);
        this->view_ = std::move(other.view_);
        this->data_ = std::move(other.data_);
        this->palette_ = std::move(other.palette_);

        this->fix_sprite_data(other.sprite_data_.type());
        other.sprite_data_ = ff::sprite_data();
    }

    return *this;
}

ff::point_int ff::dx11_texture_o::size() const
{
    if (this->data_)
    {
        const DirectX::TexMetadata& md = this->data_->GetMetadata();
        return ff::point_int(static_cast<int>(md.width), static_cast<int>(md.height));
    }

    if (this->texture_)
    {
        D3D11_TEXTURE2D_DESC desc;
        this->texture_->GetDesc(&desc);
        return ff::point_int(static_cast<int>(desc.Width), static_cast<int>(desc.Height));
    }

    assert(false);
    return ff::point_int::zeros();
}

size_t ff::dx11_texture_o::mip_count() const
{
    if (this->data_)
    {
        const DirectX::TexMetadata& md = this->data_->GetMetadata();
        return md.mipLevels;
    }

    if (this->texture_)
    {
        D3D11_TEXTURE2D_DESC desc;
        this->texture_->GetDesc(&desc);
        return static_cast<size_t>(desc.MipLevels);
    }

    assert(false);
    return 0;
}

size_t ff::dx11_texture_o::array_size() const
{
    if (this->data_)
    {
        const DirectX::TexMetadata& md = this->data_->GetMetadata();
        return md.arraySize;
    }

    if (this->texture_)
    {
        D3D11_TEXTURE2D_DESC desc;
        this->texture_->GetDesc(&desc);
        return static_cast<size_t>(desc.ArraySize);
    }

    assert(false);
    return 0;
}

size_t ff::dx11_texture_o::sample_count() const
{
    if (this->data_)
    {
        return 1;
    }

    if (this->texture_)
    {
        D3D11_TEXTURE2D_DESC desc;
        this->texture_->GetDesc(&desc);
        return static_cast<size_t>(desc.SampleDesc.Count);
    }

    assert(false);
    return 1;
}

DXGI_FORMAT ff::dx11_texture_o::format() const
{
    if (this->data_)
    {
        const DirectX::TexMetadata& md = this->data_->GetMetadata();
        return md.format;
    }

    if (this->texture_)
    {
        D3D11_TEXTURE2D_DESC desc;
        this->texture_->GetDesc(&desc);
        return desc.Format;
    }

    assert(false);
    return DXGI_FORMAT_UNKNOWN;
}

ff::dx11_texture_o::operator bool() const
{
    return this->texture_ || (this->data_ && this->data_->GetImageCount());
}

ff::sprite_type ff::dx11_texture_o::sprite_type() const
{
    return this->sprite_data_.type();
}

ID3D11Texture2D* ff::dx11_texture_o::texture()
{
    if (!this->texture_ && this->data_)
    {
        this->texture_ = ff::internal::create_texture(*this->data_);
    }

    return this->texture_.Get();
}

void ff::dx11_texture_o::update(
    size_t array_index,
    size_t mip_index,
    const ff::rect_int& rect,
    const void* data,
    DXGI_FORMAT data_format) const
{
    // TODO
}

std::shared_ptr<DirectX::ScratchImage> ff::dx11_texture_o::capture() const
{
    // TODO
    return std::shared_ptr<DirectX::ScratchImage>();
}

ff::dict ff::dx11_texture_o::resource_get_siblings(const std::shared_ptr<resource>& self) const
{
    // TODO
    return ff::dict();
}

bool ff::dx11_texture_o::resource_save_to_file(const std::filesystem::path& directory_path, std::string_view name) const
{
    // TODO
    return false;
}

bool ff::dx11_texture_o::reset()
{
    *this = this->data_
        ? ff::dx11_texture_o(this->data_, this->palette_, this->sprite_type())
        : ff::dx11_texture_o(this->size(), this->format(), this->mip_count(), this->array_size(), this->sample_count());

    return *this;
}

const ff::dx11_texture_o* ff::dx11_texture_o::view_texture() const
{
    return this;
}

ID3D11ShaderResourceView* ff::dx11_texture_o::view()
{
    if (!this->view_)
    {
        this->view_ = ff::internal::create_default_texture_view(this->texture());
    }

    return this->view_.Get();
}

const ff::sprite_data& ff::dx11_texture_o::sprite_data() const
{
    return this->sprite_data_;
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
{}

void ff::dx11_texture_o::render_frame(ff::renderer_base& render, const ff::transform& transform, float frame, const ff::dict* params)
{
    render.draw_sprite(this->sprite_data_, transform);
}

ff::value_ptr ff::dx11_texture_o::frame_value(size_t value_id, float frame, const ff::dict* params)
{
    return ff::value_ptr();
}

void ff::dx11_texture_o::advance_animation(ff::push_base<ff::animation_event>* events)
{}

void ff::dx11_texture_o::render_animation(ff::renderer_base& render, const ff::transform& transform) const
{
    render.draw_sprite(this->sprite_data_, transform);
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
    // TODO
    return false;
}

void ff::dx11_texture_o::fix_sprite_data(ff::sprite_type sprite_type)
{
    this->sprite_data_ = ff::sprite_data("", this,
        ff::rect_float(0, 0, 1, 1),
        ff::rect_float(ff::point_float::zeros(), this->size().cast<float>()),
        sprite_type);
}

std::shared_ptr<ff::resource_object_base> ff::internal::texture_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    // TODO
    return nullptr;
}

std::shared_ptr<ff::resource_object_base> ff::internal::texture_factory::load_from_cache(const ff::dict& dict) const
{
    // TODO
    return nullptr;
}
