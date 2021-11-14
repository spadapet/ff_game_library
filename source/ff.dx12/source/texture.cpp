#include "pch.h"
#include "device_reset_priority.h"
#include "globals.h"
#include "resource.h"
#include "texture.h"
#include "texture_util.h"

ff::dx12::texture::texture()
    : sprite_type_(ff::dxgi::sprite_type::unknown)
{
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::texture::texture(ff::point_size size, DXGI_FORMAT format, size_t mip_count, size_t array_size, size_t sample_count)
    : sprite_type_(ff::dxgi::sprite_type::unknown)
{
    format = ff::dxgi::fix_format(format, static_cast<size_t>(size.x), static_cast<size_t>(size.y), mip_count);

    if (size.x > 0 && size.y > 0 && mip_count > 0 && array_size > 0 && sample_count > 0)
    {
        this->resource_ = std::make_unique<ff::dx12::resource>(CD3DX12_RESOURCE_DESC::Tex2D(format,
            static_cast<UINT64>(size.x),
            static_cast<UINT>(size.y),
            static_cast<UINT16>(array_size),
            static_cast<UINT16>(mip_count),
            static_cast<UINT>(ff::dx12::fix_sample_count(format, sample_count)), 0, // quality
            !ff::dxgi::compressed_format(format) ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE));
    }

    this->sprite_type_ = ff::dxgi::palette_format(format)
        ? ff::dxgi::sprite_type::opaque_palette
        : (ff::dxgi::has_alpha(format) ? ff::dxgi::sprite_type::transparent : ff::dxgi::sprite_type::opaque);

    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::texture::texture(const std::shared_ptr<DirectX::ScratchImage>& data, ff::dxgi::sprite_type sprite_type)
    : data_(data)
{
    this->sprite_type_ = (sprite_type == ff::dxgi::sprite_type::unknown && this->data_)
        ? ff::dxgi::get_sprite_type(*this->data_)
        : sprite_type;

    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::texture::texture(texture&& other) noexcept
    : resource_(std::move(other.resource_))
    , data_(std::move(other.data_))
    , view_(std::move(other.view_))
    , sprite_type_(std::move(other.sprite_type_))
{
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::texture::~texture()
{
    ff::dx12::remove_device_child(this);
}

ff::dx12::texture& ff::dx12::texture::operator=(texture&& other) noexcept
{
    return this->assign(std::move(other));
}

ff::dx12::texture& ff::dx12::texture::assign(texture&& other) noexcept
{
    if (this != &other)
    {
        this->resource_ = std::move(other.resource_);
        this->view_ = std::move(other.view_);
        this->data_ = std::move(other.data_);
        this->sprite_type_ = std::move(other.sprite_type_);

        this->on_reset();
    }

    return *this;
}

void ff::dx12::texture::on_reset()
{
    // override this
}

ff::point_size ff::dx12::texture::size() const
{
    if (this->data_)
    {
        const DirectX::TexMetadata& md = this->data_->GetMetadata();
        return ff::point_size(md.width, md.height);
    }

    if (this->resource_)
    {
        const D3D12_RESOURCE_DESC& desc = this->resource_->desc();
        return ff::point_t<uint64_t>(desc.Width, desc.Height).cast<size_t>();
    }

    assert(false);
    return {};
}

size_t ff::dx12::texture::mip_count() const
{
    if (this->data_)
    {
        const DirectX::TexMetadata& md = this->data_->GetMetadata();
        return md.mipLevels;
    }

    if (this->resource_)
    {
        const D3D12_RESOURCE_DESC& desc = this->resource_->desc();
        return static_cast<size_t>(desc.MipLevels);
    }

    assert(false);
    return 0;
}

size_t ff::dx12::texture::array_size() const
{
    if (this->data_)
    {
        const DirectX::TexMetadata& md = this->data_->GetMetadata();
        return md.arraySize;
    }

    if (this->resource_)
    {
        const D3D12_RESOURCE_DESC& desc = this->resource_->desc();
        return static_cast<size_t>(desc.DepthOrArraySize);
    }

    assert(false);
    return 0;
}

size_t ff::dx12::texture::sample_count() const
{
    if (this->data_)
    {
        return 1;
    }

    if (this->resource_)
    {
        const D3D12_RESOURCE_DESC& desc = this->resource_->desc();
        return static_cast<size_t>(desc.SampleDesc.Count);
    }

    assert(false);
    return 1;
}

DXGI_FORMAT ff::dx12::texture::format() const
{
    if (this->data_)
    {
        const DirectX::TexMetadata& md = this->data_->GetMetadata();
        return md.format;
    }

    if (this->resource_)
    {
        const D3D12_RESOURCE_DESC& desc = this->resource_->desc();
        return desc.Format;
    }

    assert(false);
    return DXGI_FORMAT_UNKNOWN;
}

ff::dx12::texture::operator bool() const
{
    return this->resource_ || (this->data_ && this->data_->GetImageCount());
}

ff::dxgi::sprite_type ff::dx12::texture::sprite_type() const
{
    return this->sprite_type_;
}

std::shared_ptr<DirectX::ScratchImage> ff::dx12::texture::data() const
{
    if (!this->data_ && this->resource_)
    {
        DirectX::ScratchImage scratch;
        ff::dx12::resource* resource = this->resource_.get();

        // Can't use the device context on background threads
        ff::thread_dispatch::get_game()->send([&scratch, resource]()
            {
                size_t image_count = resource->desc().MipLevels * resource->desc().DepthOrArraySize;
                scratch = resource->capture_texture(nullptr, 0, image_count, nullptr);
            });

        return scratch.GetImageCount() ? std::make_shared<DirectX::ScratchImage>(std::move(scratch)) : nullptr;
    }

    return this->data_;
}

const ff::dx12::resource* ff::dx12::texture::resource() const
{
    if (!this->resource_ && this->data_)
    {
        const DirectX::TexMetadata& md = this->data_->GetMetadata();
        this->resource_ = std::make_unique<ff::dx12::resource>(CD3DX12_RESOURCE_DESC::Tex2D(md.format,
            static_cast<UINT64>(md.width),
            static_cast<UINT>(md.height),
            static_cast<UINT16>(md.arraySize),
            static_cast<UINT16>(md.mipLevels),
            1, 0, // quality
            !ff::dxgi::compressed_format(md.format) ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE),
            D3D12_RESOURCE_STATE_COPY_DEST);

        this->resource_->update_texture(nullptr, this->data_->GetImages(), 0, this->data_->GetImageCount(), ff::point_size{});
    }

    return this->resource_.get();
}

bool ff::dx12::texture::update(
    ff::dxgi::command_context_base& context,
    size_t array_index,
    size_t mip_index,
    const ff::point_size& pos,
    const DirectX::Image& data)
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

    if (this->resource_)
    {
        UINT sub_index = ::D3D12CalcSubresource(static_cast<UINT>(mip_index), static_cast<UINT>(array_index), 0, static_cast<UINT>(this->mip_count()), static_cast<UINT>(this->array_size()));
        ff::dx12::resource* resource = this->resource_.get();

        ff::thread_dispatch::get_game()->send([resource, sub_index, pos, &data, &context]()
            {
                resource->update_texture(nullptr, &data, sub_index, 1, pos.cast<size_t>());
            });
    }

    return true;
}

bool ff::dx12::texture::reset()
{
    *this = this->data_
        ? ff::dx12::texture(this->data_, this->sprite_type())
        : ff::dx12::texture(this->size(), this->format(), this->mip_count(), this->array_size(), this->sample_count());

    return *this;
}

const ff::dxgi::texture_view_access_base& ff::dx12::texture::view_access() const
{
    return *this;
}

const ff::dxgi::texture_base* ff::dx12::texture::view_texture() const
{
    return this;
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::dx12::texture::dx12_texture_view() const
{
    if (!this->view_)
    {
        ff::dx12::create_shader_view(this->resource(), this->view_.cpu_handle(0));
    }

    return this->view_.cpu_handle(0);
}
