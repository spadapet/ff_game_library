#include "pch.h"
#include "data_blob.h"
#include "draw_base.h"
#include "graphics.h"
#include "png_image.h"
#include "sprite_data.h"
#include "sprite_type.h"
#include "texture.h"
#include "texture_metadata.h"
#include "texture_util.h"

#if DXVER == 11

ff::texture::texture(const ff::resource_file& resource_file, DXGI_FORMAT new_format, size_t new_mip_count)
{
    this->data_ = ff::internal::load_texture_data(resource_file, new_format, new_mip_count, this->palette_);
    this->fix_sprite_data(this->data_ ? ff::internal::get_sprite_type(*this->data_) : ff::sprite_type::unknown);

    ff_internal_dx::add_device_child(this, ff_internal_dx::device_reset_priority::normal);
}

ff::texture::texture(ff::point_int size, DXGI_FORMAT format, size_t mip_count, size_t array_size, size_t sample_count)
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
        desc.SampleDesc.Count = static_cast<UINT>(ff_internal_dx::fix_sample_count(format, sample_count));

        HRESULT hr = ff::dx11::device()->CreateTexture2D(&desc, nullptr, this->texture_.GetAddressOf());
        assert(SUCCEEDED(hr));
    }

    const ff::sprite_type sprite_type = ff::dxgi::palette_format(format)
        ? ff::sprite_type::opaque_palette
        : (ff::dxgi::has_alpha(format) ? ff::sprite_type::transparent : ff::sprite_type::opaque);

    this->fix_sprite_data(sprite_type);

    ff_internal_dx::add_device_child(this, ff_internal_dx::device_reset_priority::normal);
}

ff::texture::texture(const std::shared_ptr<DirectX::ScratchImage>& data, const std::shared_ptr<DirectX::ScratchImage>& palette, ff::sprite_type sprite_type)
    : data_(data)
    , palette_(palette)
{
    this->fix_sprite_data(sprite_type == ff::sprite_type::unknown && this->data_ ? ff::internal::get_sprite_type(*this->data_) : sprite_type);

    ff_internal_dx::add_device_child(this, ff_internal_dx::device_reset_priority::normal);
}

ff::texture::texture(const texture& other, DXGI_FORMAT new_format, size_t new_mip_count)
{
    ff::sprite_type sprite_type = ff::sprite_type::unknown;

    std::shared_ptr<DirectX::ScratchImage> data = other.data();
    if (data)
    {
        this->data_ = ff::internal::convert_texture_data(data, new_format, new_mip_count);
        if (this->data_)
        {
            sprite_type = (this->data_ == data) ? other.sprite_type() : ff::internal::get_sprite_type(*this->data_);
        }
    }

    if (ff::dxgi::palette_format(other.format()) && ff::dxgi::palette_format(new_format))
    {
        this->palette_ = other.palette_;
    }

    this->fix_sprite_data(sprite_type);

    ff_internal_dx::add_device_child(this, ff_internal_dx::device_reset_priority::normal);
}

ff::texture::texture(texture&& other) noexcept
    : texture_(std::move(other.texture_))
    , view_(std::move(other.view_))
    , data_(std::move(other.data_))
    , palette_(std::move(other.palette_))
{
    this->fix_sprite_data(other.sprite_data_.type());
    other.sprite_data_ = ff::sprite_data();

    ff_internal_dx::add_device_child(this, ff_internal_dx::device_reset_priority::normal);
}

ff::texture::~texture()
{
    ff_internal_dx::remove_device_child(this);
}

ff::texture& ff::texture::operator=(texture&& other) noexcept
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

ff::point_int ff::texture::size() const
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
    return ff::point_int{};
}

size_t ff::texture::mip_count() const
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

size_t ff::texture::array_size() const
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

size_t ff::texture::sample_count() const
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

DXGI_FORMAT ff::texture::format() const
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

ff::texture::operator bool() const
{
    return this->texture_ || (this->data_ && this->data_->GetImageCount());
}

ff::sprite_type ff::texture::sprite_type() const
{
    return this->sprite_data_.type();
}

std::shared_ptr<DirectX::ScratchImage> ff::texture::data() const
{
    if (!this->data_ && this->texture_)
    {
        DirectX::ScratchImage scratch;
        ID3D11Texture2D* texture = this->texture_.Get();

        // Can't use the device context on background threads
        ff::thread_dispatch::get_game()->send([&scratch, texture]()
            {
                DirectX::CaptureTexture(ff::dx11::device(), ff_dx::context(), texture, scratch);
            });

        return scratch.GetImageCount() ? std::make_shared<DirectX::ScratchImage>(std::move(scratch)) : nullptr;
    }

    return this->data_;
}

std::shared_ptr<DirectX::ScratchImage> ff::texture::palette() const
{
    return this->palette_;
}

ID3D11Texture2D* ff::texture::dx_texture() const
{
    if (!this->texture_ && this->data_)
    {
        this->texture_ = ff::internal::create_texture(*this->data_);
    }

    return this->texture_.Get();
}

bool ff::texture::update(
    size_t array_index,
    size_t mip_index,
    const ff::rect_int& rect,
    const void* data,
    DXGI_FORMAT data_format,
    bool update_local_cache)
{
    if (this->format() == data_format)
    {
        size_t width = static_cast<size_t>(rect.width());
        size_t height = static_cast<size_t>(rect.height());
        size_t row_pitch, slice_pitch;
        DirectX::ComputePitch(this->format(), width, height, row_pitch, slice_pitch);

        if (update_local_cache || !this->texture_)
        {
            if (!this->data_)
            {
                this->data_ = this->data();

                if (!this->data_)
                {
                    assert(false);
                    return false;
                }
            }

            DirectX::Image image{};
            image.width = width;
            image.height = height;
            image.format = this->format();
            image.rowPitch = row_pitch;
            image.slicePitch = slice_pitch;
            image.pixels = reinterpret_cast<uint8_t*>(const_cast<void*>(data));

            if (FAILED(DirectX::CopyRectangle(image,
                DirectX::Rect(0, 0, image.width, image.height),
                *this->data_->GetImage(mip_index, array_index, 0),
                DirectX::TEX_FILTER_DEFAULT,
                static_cast<size_t>(rect.left),
                static_cast<size_t>(rect.top))))
            {
                assert(false);
            }
        }

        if (this->texture_)
        {
            CD3D11_BOX box(static_cast<UINT>(rect.left), static_cast<UINT>(rect.top), 0, static_cast<UINT>(rect.right), static_cast<UINT>(rect.bottom), 1);
            UINT subresource = ::D3D11CalcSubresource(static_cast<UINT>(mip_index), static_cast<UINT>(array_index), static_cast<UINT>(this->mip_count()));
            ID3D11Texture2D* texture = this->texture_.Get();

            ff::thread_dispatch::get_game()->send([texture, subresource, box, data, row_pitch]()
                {
                    ff::dx11::get_device_state().update_subresource(
                        texture, subresource, &box, data, static_cast<UINT>(row_pitch), 0);
                });
        }

        return true;
    }

    return false;
}

ff::dict ff::texture::resource_get_siblings(const std::shared_ptr<resource>& self) const
{
    ff::value_ptr value;
    {
        std::shared_ptr<ff::resource_object_base> metadata = std::make_shared<ff::texture_metadata>(
            this->size(), this->mip_count(), this->array_size(), this->sample_count(), this->format());
        value = ff::value::create<ff::resource_object_base>(std::move(metadata));
    }

    std::ostringstream name;
    name << self->name() << ".metadata";

    ff::dict dict;
    dict.set(name.str(), value);
    return dict;
}

bool ff::texture::resource_save_to_file(const std::filesystem::path& directory_path, std::string_view name) const
{
    std::shared_ptr<DirectX::ScratchImage> data = this->data();
    if (data)
    {
        if (!ff::dxgi::palette_format(data->GetMetadata().format))
        {
            data = ff::internal::convert_texture_data(data, ff::dxgi::DEFAULT_FORMAT, this->mip_count());
        }

        for (size_t i = 0; i < data->GetImageCount(); i++)
        {
            std::ostringstream file_name;
            file_name << name << "." << i << ".png";

            ff::file_writer file_writer(directory_path / file_name.str());
            ff::png_image_writer png(file_writer);
            png.write(data->GetImages()[i], this->palette_ ? this->palette_->GetImages() : nullptr);
        }

        return true;
    }

    return false;
}

bool ff::texture::reset()
{
    *this = this->data_
        ? ff::texture(this->data_, this->palette_, this->sprite_type())
        : ff::texture(this->size(), this->format(), this->mip_count(), this->array_size(), this->sample_count());

    return *this;
}

const ff::texture* ff::texture::view_texture() const
{
    return this;
}

ID3D11ShaderResourceView* ff::texture::view() const
{
    if (!this->view_)
    {
        this->view_ = ff::internal::create_shader_view(this->dx_texture());
    }

    return this->view_.Get();
}

std::string_view ff::texture::name() const
{
    return "";
}

const ff::sprite_data& ff::texture::sprite_data() const
{
    return this->sprite_data_;
}

float ff::texture::frame_length() const
{
    return 0.0f;
}

float ff::texture::frames_per_second() const
{
    return 0.0f;
}

void ff::texture::frame_events(float start, float end, bool include_start, ff::push_base<ff::animation_event>& events)
{}

void ff::texture::draw_frame(ff::draw_base& draw, const ff::transform& transform, float frame, const ff::dict* params)
{
    draw.draw_sprite(this->sprite_data_, transform);
}

void ff::texture::draw_frame(ff::draw_base& draw, const ff::pixel_transform& transform, float frame, const ff::dict* params)
{
    draw.draw_sprite(this->sprite_data_, transform);
}

ff::value_ptr ff::texture::frame_value(size_t value_id, float frame, const ff::dict* params)
{
    return ff::value_ptr();
}

void ff::texture::advance_animation(ff::push_base<ff::animation_event>* events)
{}

void ff::texture::draw_animation(ff::draw_base& draw, const ff::transform& transform) const
{
    draw.draw_sprite(this->sprite_data_, transform);
}

void ff::texture::draw_animation(ff::draw_base& draw, const ff::pixel_transform& transform) const
{
    draw.draw_sprite(this->sprite_data_, transform);
}

float ff::texture::animation_frame() const
{
    return 0.0f;
}

const ff::animation_base* ff::texture::animation() const
{
    return this;
}

bool ff::texture::save_to_cache(ff::dict& dict, bool& allow_compress) const
{
    dict.set_enum<ff::sprite_type>("sprite_type", this->sprite_type());

    DirectX::Blob blob;
    std::shared_ptr<DirectX::ScratchImage> data = this->data();
    if (data && SUCCEEDED(DirectX::SaveToDDSMemory(
        data->GetImages(), data->GetImageCount(), data->GetMetadata(), DirectX::DDS_FLAGS_NONE, blob)))
    {
        std::shared_ptr<ff::data_base> blob_data = std::make_shared<ff::internal::data_blob_dxtex>(std::move(blob));
        dict.set<ff::data_base>("data", blob_data, ff::saved_data_type::zlib_compressed);
    }
    else
    {
        return false;
    }

    if (this->palette_)
    {
        if (SUCCEEDED(DirectX::SaveToDDSMemory(
            this->palette_->GetImages(), this->palette_->GetImageCount(), this->palette_->GetMetadata(), DirectX::DDS_FLAGS_NONE, blob)))
        {
            std::shared_ptr<ff::data_base> blob_data = std::make_shared<ff::internal::data_blob_dxtex>(std::move(blob));
            dict.set<ff::data_base>("palette", blob_data, ff::saved_data_type::zlib_compressed);
        }
        else
        {
            return false;
        }
    }

    return true;
}

void ff::texture::fix_sprite_data(ff::sprite_type sprite_type)
{
    this->sprite_data_ = ff::sprite_data(this,
        ff::rect_float(0, 0, 1, 1),
        ff::rect_float(ff::point_float{}, this->size().cast<float>()),
        sprite_type);
}

std::shared_ptr<ff::resource_object_base> ff::internal::texture_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    bool pma = dict.get<bool>("pma");
    size_t mip_count = dict.get<size_t>("mips", 1);
    std::filesystem::path full_file = dict.get<std::string>("file");
    DXGI_FORMAT format = ff::dxgi::parse_format(dict.get<std::string>("format", std::string("rgbs32")));

    if (format != DXGI_FORMAT_UNKNOWN)
    {
        std::shared_ptr<DirectX::ScratchImage> palette;
        std::shared_ptr<DirectX::ScratchImage> data = ff::internal::load_texture_data(full_file, format, mip_count, palette);

        for (size_t i = 0; pma && i < data->GetImageCount(); i++)
        {
            const DirectX::Image& image = data->GetImages()[i];

            DirectX::ScratchImage scratch;
            if (FAILED(DirectX::PremultiplyAlpha(image, DirectX::TEX_PMALPHA_DEFAULT, scratch)) ||
                FAILED(DirectX::CopyRectangle(*scratch.GetImages(), DirectX::Rect(0, 0, image.width, image.height), image, DirectX::TEX_FILTER_DEFAULT, 0, 0)))
            {
                assert(false);
                return nullptr;
            }
        }

        std::shared_ptr<ff::texture> texture = std::make_shared<ff::texture>(data, palette);
        return *texture ? texture : nullptr;
    }

    return nullptr;
}

std::shared_ptr<ff::resource_object_base> ff::internal::texture_factory::load_from_cache(const ff::dict& dict) const
{
    ff::sprite_type sprite_type = dict.get_enum<ff::sprite_type>("sprite_type");

    std::shared_ptr<ff::data_base> palette_data = dict.get<ff::data_base>("palette");
    std::shared_ptr<ff::data_base> data = dict.get<ff::data_base>("data");

    DirectX::ScratchImage palette_scratch;
    if (palette_data && FAILED(DirectX::LoadFromDDSMemory(
        palette_data->data(), palette_data->size(), DirectX::DDS_FLAGS_NONE, nullptr, palette_scratch)))
    {
        assert(false);
        return false;
    }

    DirectX::ScratchImage data_scratch;
    if (data && FAILED(DirectX::LoadFromDDSMemory(
        data->data(), data->size(), DirectX::DDS_FLAGS_NONE, nullptr, data_scratch)))
    {
        assert(false);
        return false;
    }

    std::shared_ptr<ff::texture> texture = std::make_shared<ff::texture>(
        data_scratch.GetImageCount() ? std::make_shared<DirectX::ScratchImage>(std::move(data_scratch)) : nullptr,
        palette_scratch.GetImageCount() ? std::make_shared<DirectX::ScratchImage>(std::move(palette_scratch)) : nullptr,
        sprite_type);

    return *texture ? texture : nullptr;
}

#endif
