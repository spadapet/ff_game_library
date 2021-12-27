#include "pch.h"
#include "device_reset_priority.h"
#include "device_state.h"
#include "globals.h"
#include "texture.h"
#include "texture_util.h"

ff::dx11::texture::texture()
    : sprite_type_(ff::dxgi::sprite_type::unknown)
{
    ff::dx11::add_device_child(this, ff::dx11::device_reset_priority::normal);
}

ff::dx11::texture::texture(ff::point_size size, DXGI_FORMAT format, size_t mip_count, size_t array_size, size_t sample_count, const DirectX::XMFLOAT4* optimized_clear_color)
    : sprite_type_(ff::dxgi::sprite_type::unknown)
{
    format = ff::dxgi::fix_format(format, static_cast<size_t>(size.x), static_cast<size_t>(size.y), mip_count);

    if (size.x > 0 && size.y > 0 && mip_count > 0 && array_size > 0 && sample_count > 0)
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(size.x);
        desc.Height = static_cast<UINT>(size.y);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (!ff::dxgi::compressed_format(format) ? D3D11_BIND_RENDER_TARGET : 0);
        desc.Format = format;
        desc.MipLevels = static_cast<UINT>(mip_count);
        desc.ArraySize = static_cast<UINT>(array_size);
        desc.SampleDesc.Count = static_cast<UINT>(ff::dx11::fix_sample_count(format, sample_count));

        HRESULT hr = ff::dx11::device()->CreateTexture2D(&desc, nullptr, this->texture_.GetAddressOf());
        assert(SUCCEEDED(hr));
    }

    this->sprite_type_ = ff::dxgi::palette_format(format)
        ? ff::dxgi::sprite_type::opaque_palette
        : (ff::dxgi::has_alpha(format) ? ff::dxgi::sprite_type::transparent : ff::dxgi::sprite_type::opaque);

    ff::dx11::add_device_child(this, ff::dx11::device_reset_priority::normal);
}

ff::dx11::texture::texture(const std::shared_ptr<DirectX::ScratchImage>& data, ff::dxgi::sprite_type sprite_type)
    : data_(data)
{
    this->sprite_type_ = (sprite_type == ff::dxgi::sprite_type::unknown && this->data_)
        ? ff::dxgi::get_sprite_type(*this->data_)
        : sprite_type;

    ff::dx11::add_device_child(this, ff::dx11::device_reset_priority::normal);
}

ff::dx11::texture::texture(texture&& other) noexcept
    : texture_(std::move(other.texture_))
    , view_(std::move(other.view_))
    , data_(std::move(other.data_))
    , sprite_type_(std::move(other.sprite_type_))
{
    ff::dx11::add_device_child(this, ff::dx11::device_reset_priority::normal);
}

ff::dx11::texture::~texture()
{
    ff::dx11::remove_device_child(this);
}

ff::dx11::texture& ff::dx11::texture::operator=(texture&& other) noexcept
{
    return this->assign(std::move(other));
}

ff::dx11::texture& ff::dx11::texture::assign(texture&& other) noexcept
{
    if (this != &other)
    {
        this->texture_ = std::move(other.texture_);
        this->view_ = std::move(other.view_);
        this->data_ = std::move(other.data_);
        this->sprite_type_ = std::move(other.sprite_type_);

        this->on_reset();
    }

    return *this;
}

void ff::dx11::texture::on_reset()
{
    // override this
}

ff::point_size ff::dx11::texture::size() const
{
    if (this->data_)
    {
        const DirectX::TexMetadata& md = this->data_->GetMetadata();
        return ff::point_size(md.width, md.height);
    }

    if (this->texture_)
    {
        D3D11_TEXTURE2D_DESC desc;
        this->texture_->GetDesc(&desc);
        return ff::point_size(desc.Width, desc.Height);
    }

    assert(false);
    return {};
}

size_t ff::dx11::texture::mip_count() const
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

size_t ff::dx11::texture::array_size() const
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

size_t ff::dx11::texture::sample_count() const
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

DXGI_FORMAT ff::dx11::texture::format() const
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

ff::dx11::texture::operator bool() const
{
    return this->texture_ || (this->data_ && this->data_->GetImageCount());
}

ff::dxgi::sprite_type ff::dx11::texture::sprite_type() const
{
    return this->sprite_type_;
}

std::shared_ptr<DirectX::ScratchImage> ff::dx11::texture::data() const
{
    if (!this->data_ && this->texture_)
    {
        DirectX::ScratchImage scratch;
        ID3D11Texture2D* texture = this->texture_.Get();

        // Can't use the device context on background threads
        ff::thread_dispatch::get_game()->send([&scratch, texture]()
            {
                DirectX::CaptureTexture(ff::dx11::device(), ff::dx11::context(), texture, scratch);
            });

        return scratch.GetImageCount() ? std::make_shared<DirectX::ScratchImage>(std::move(scratch)) : nullptr;
    }

    return this->data_;
}

ff::dx11::texture& ff::dx11::texture::get(ff::dxgi::texture_base& obj)
{
    return static_cast<ff::dx11::texture&>(obj);
}

ID3D11Texture2D* ff::dx11::texture::dx11_texture() const
{
    if (!this->texture_ && this->data_)
    {
        this->texture_ = ff::dx11::create_texture(*this->data_);
    }

    return this->texture_.Get();
}

bool ff::dx11::texture::update(size_t array_index, size_t mip_index, const ff::point_size& pos, const DirectX::Image& data)
{
    if (this->format() != data.format)
    {
        assert(false);
        return false;
    }

    if (!this->data_)
    {
        this->data_ = this->data();

        if (!this->data_)
        {
            assert(false);
            return false;
        }
    }

    if (FAILED(DirectX::CopyRectangle(data,
        DirectX::Rect(0, 0, data.width, data.height),
        *this->data_->GetImage(mip_index, array_index, 0),
        DirectX::TEX_FILTER_DEFAULT,
        static_cast<size_t>(pos.x),
        static_cast<size_t>(pos.y))))
    {
        assert(false);
    }

    if (this->texture_)
    {
        CD3D11_BOX box(static_cast<UINT>(pos.x), static_cast<UINT>(pos.y), 0, static_cast<UINT>(pos.x + data.width), static_cast<UINT>(pos.y + data.height), 1);
        UINT subresource = ::D3D11CalcSubresource(static_cast<UINT>(mip_index), static_cast<UINT>(array_index), static_cast<UINT>(this->mip_count()));
        ID3D11Texture2D* texture = this->texture_.Get();

        ff::thread_dispatch::get_game()->send([texture, subresource, box, &data]()
            {
                ff::dx11::get_device_state().update_subresource(texture, subresource, &box, data.pixels, static_cast<UINT>(data.rowPitch), 0);
            });
    }

    return true;
}

bool ff::dx11::texture::reset()
{
    *this = this->data_
        ? ff::dx11::texture(this->data_, this->sprite_type())
        : ff::dx11::texture(this->size(), this->format(), this->mip_count(), this->array_size(), this->sample_count());

    return *this;
}

ff::dxgi::texture_view_access_base& ff::dx11::texture::view_access()
{
    return *this;
}

ff::dxgi::texture_base* ff::dx11::texture::view_texture()
{
    return this;
}

size_t ff::dx11::texture::view_array_start() const
{
    return 0;
}

size_t ff::dx11::texture::view_array_size() const
{
    return this->array_size();
}

size_t ff::dx11::texture::view_mip_start() const
{
    return 0;
}

size_t ff::dx11::texture::view_mip_size() const
{
    return this->mip_count();
}

ID3D11ShaderResourceView* ff::dx11::texture::dx11_texture_view() const
{
    if (!this->view_)
    {
        this->view_ = ff::dx11::create_shader_view(this->dx11_texture());
    }

    return this->view_.Get();
}
