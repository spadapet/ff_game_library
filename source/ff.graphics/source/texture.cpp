#include "pch.h"
#include "graphics.h"
#include "png_image.h"
#include "texture.h"
#include "texture_metadata.h"
#include "texture_util.h"

ff::texture::texture(const ff::resource_file& resource_file, DXGI_FORMAT new_format, size_t new_mip_count)
{
    auto data = ff::internal::load_texture_data(resource_file, new_format, new_mip_count, this->palette_);
    this->assign(ff::graphics::client_functions().create_static_texture(data, ff::dxgi::sprite_type::unknown));
}

ff::texture::texture(ff::point_size size, DXGI_FORMAT format, size_t mip_count, size_t array_size, size_t sample_count, const DirectX::XMFLOAT4* optimized_clear_color)
{
    this->assign(ff::graphics::client_functions().create_render_texture(size, format, mip_count, array_size, sample_count, optimized_clear_color));
}

ff::texture::texture(const std::shared_ptr<DirectX::ScratchImage>& data, const std::shared_ptr<DirectX::ScratchImage>& palette, ff::dxgi::sprite_type sprite_type)
    : palette_(palette)
{
    this->assign(ff::graphics::client_functions().create_static_texture(data, sprite_type));
}

ff::texture::texture(const std::shared_ptr<ff::dxgi::texture_base>& dxgi_texture)
{
    this->assign(dxgi_texture);
}

ff::texture::texture(const texture& other, DXGI_FORMAT new_format, size_t new_mip_count)
{
    ff::dxgi::sprite_type sprite_type = ff::dxgi::sprite_type::unknown;

    std::shared_ptr<DirectX::ScratchImage> other_data = other.dxgi_texture_->data();
    std::shared_ptr<DirectX::ScratchImage> new_data;
    if (other_data)
    {
        new_data = ff::dxgi::convert_texture_data(other_data, new_format, new_mip_count);
        if (new_data)
        {
            sprite_type = (new_data == other_data) ? other.dxgi_texture_->sprite_type() : ff::dxgi::get_sprite_type(*new_data);
            this->assign(ff::graphics::client_functions().create_static_texture(new_data, sprite_type));
        }
    }

    if (ff::dxgi::palette_format(other.dxgi_texture_->format()) && ff::dxgi::palette_format(new_format))
    {
        this->palette_ = other.palette_;
    }
}

ff::texture::operator bool() const
{
    return this->dxgi_texture_ != nullptr;
}

const std::shared_ptr<DirectX::ScratchImage>& ff::texture::palette() const
{
    return this->palette_;
}

const std::shared_ptr<ff::dxgi::texture_base>& ff::texture::dxgi_texture() const
{
    return this->dxgi_texture_;
}

ff::dict ff::texture::resource_get_siblings(const std::shared_ptr<ff::resource>& self) const
{
    ff::value_ptr value;
    {
        ff::dxgi::texture_base* tb = this->dxgi_texture_.get();
        std::shared_ptr<ff::resource_object_base> metadata = std::make_shared<ff::texture_metadata>(
            tb->size(), tb->mip_count(), tb->array_size(), tb->sample_count(), tb->format());
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
    std::shared_ptr<DirectX::ScratchImage> data = this->dxgi_texture_->data();
    if (data)
    {
        if (!ff::dxgi::palette_format(data->GetMetadata().format))
        {
            data = ff::dxgi::convert_texture_data(data, ff::dxgi::DEFAULT_FORMAT, this->dxgi_texture_->mip_count());
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

std::string_view ff::texture::name() const
{
    return "";
}

const ff::dxgi::sprite_data& ff::texture::sprite_data() const
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

void ff::texture::draw_frame(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform, float frame, const ff::dict* params)
{
    draw.draw_sprite(this->sprite_data_, transform);
}

void ff::texture::draw_frame(ff::dxgi::draw_base& draw, const ff::dxgi::pixel_transform& transform, float frame, const ff::dict* params)
{
    draw.draw_sprite(this->sprite_data_, transform);
}

ff::value_ptr ff::texture::frame_value(size_t value_id, float frame, const ff::dict* params)
{
    return ff::value_ptr();
}

void ff::texture::advance_animation(ff::push_base<ff::animation_event>* events)
{}

void ff::texture::draw_animation(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform) const
{
    draw.draw_sprite(this->sprite_data_, transform);
}

void ff::texture::draw_animation(ff::dxgi::draw_base& draw, const ff::dxgi::pixel_transform& transform) const
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
    dict.set_enum<ff::dxgi::sprite_type>("sprite_type", this->dxgi_texture_->sprite_type());

    std::shared_ptr<DirectX::ScratchImage> data = this->dxgi_texture_->data();
    if (data)
    {
        DirectX::Blob blob;
        if (FAILED(DirectX::SaveToDDSMemory(
            data->GetImages(), data->GetImageCount(), data->GetMetadata(), DirectX::DDS_FLAGS_NONE, blob)))
        {
            return false;
        }

        std::shared_ptr<ff::data_base> blob_data = std::make_shared<ff::dxgi::data_blob_dxtex>(std::move(blob));
        dict.set<ff::data_base>("data", blob_data, ff::saved_data_type::zlib_compressed);
    }
    else
    {
        return false;
    }

    if (this->palette_)
    {
        DirectX::Blob blob;
        if (FAILED(DirectX::SaveToDDSMemory(
            this->palette_->GetImages(), this->palette_->GetImageCount(), this->palette_->GetMetadata(), DirectX::DDS_FLAGS_NONE, blob)))
        {
            return false;
        }

        std::shared_ptr<ff::data_base> blob_data = std::make_shared<ff::dxgi::data_blob_dxtex>(std::move(blob));
        dict.set<ff::data_base>("palette", blob_data, ff::saved_data_type::zlib_compressed);
    }

    return true;
}

void ff::texture::assign(const std::shared_ptr<ff::dxgi::texture_base>& dxgi_texture)
{
    assert(dxgi_texture);
    this->dxgi_texture_ = dxgi_texture;

    this->sprite_data_ = ff::dxgi::sprite_data(this->dxgi_texture_.get(),
        ff::rect_float(0, 0, 1, 1),
        ff::rect_float(ff::point_float{}, this->dxgi_texture_->size().cast<float>()),
        this->dxgi_texture_->sprite_type());
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
    ff::dxgi::sprite_type sprite_type = dict.get_enum<ff::dxgi::sprite_type>("sprite_type");

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
