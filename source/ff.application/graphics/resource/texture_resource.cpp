#include "pch.h"
#include "graphics/dxgi/draw_base.h"
#include "graphics/dxgi/dxgi_globals.h"
#include "graphics/dxgi/format_util.h"
#include "graphics/dxgi/sprite_data.h"
#include "graphics/resource/png_image.h"
#include "graphics/resource/texture_data.h"
#include "graphics/resource/texture_resource.h"
#include "graphics/resource/texture_metadata.h"
#include "graphics/types/blob.h"

static std::shared_ptr<DirectX::ScratchImage> convert_texture_data(const std::shared_ptr<DirectX::ScratchImage>& data, DXGI_FORMAT new_format, size_t new_mip_count)
{
    if (!data || !data->GetImageCount())
    {
        return nullptr;
    }

    new_format = ff::dxgi::fix_format(new_format, data->GetMetadata().width, data->GetMetadata().height, new_mip_count);

    if (data->GetMetadata().format == new_format && data->GetMetadata().mipLevels == new_mip_count)
    {
        return data;
    }

    DirectX::ScratchImage scratch_final;
    if (FAILED(scratch_final.InitializeFromImage(*data->GetImages())))
    {
        assert(false);
        return nullptr;
    }

    if (ff::dxgi::compressed_format(scratch_final.GetMetadata().format))
    {
        DirectX::ScratchImage scratch_rgb;
        if (FAILED(DirectX::Decompress(
            scratch_final.GetImages(),
            scratch_final.GetImageCount(),
            scratch_final.GetMetadata(),
            DXGI_FORMAT_R8G8B8A8_UNORM,
            scratch_rgb)))
        {
            assert(false);
            return nullptr;
        }

        scratch_final = std::move(scratch_rgb);
    }
    else if (scratch_final.GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM)
    {
        DirectX::ScratchImage scratch_rgb;
        if (FAILED(DirectX::Convert(
            scratch_final.GetImages(),
            scratch_final.GetImageCount(),
            scratch_final.GetMetadata(),
            DXGI_FORMAT_R8G8B8A8_UNORM,
            DirectX::TEX_FILTER_DEFAULT,
            0, // threshold
            scratch_rgb)))
        {
            assert(false);
            return nullptr;
        }

        scratch_final = std::move(scratch_rgb);
    }

    if (new_mip_count != 1)
    {
        DirectX::ScratchImage scratch_mips;
        if (FAILED(DirectX::GenerateMipMaps(
            scratch_final.GetImages(),
            scratch_final.GetImageCount(),
            scratch_final.GetMetadata(),
            DirectX::TEX_FILTER_DEFAULT,
            new_mip_count,
            scratch_mips)))
        {
            assert(false);
            return nullptr;
        }

        scratch_final = std::move(scratch_mips);
    }

    if (ff::dxgi::compressed_format(new_format))
    {
        DirectX::ScratchImage scratch_new;
        if (FAILED(DirectX::Compress(
            scratch_final.GetImages(),
            scratch_final.GetImageCount(),
            scratch_final.GetMetadata(),
            new_format,
            DirectX::TEX_COMPRESS_DEFAULT,
            0, // alpharef
            scratch_new)))
        {
            assert(false);
            return nullptr;
        }

        scratch_final = std::move(scratch_new);
    }
    else if (new_format != scratch_final.GetMetadata().format)
    {
        DirectX::ScratchImage scratch_new;
        if (FAILED(DirectX::Convert(
            scratch_final.GetImages(),
            scratch_final.GetImageCount(),
            scratch_final.GetMetadata(),
            new_format,
            DirectX::TEX_FILTER_DEFAULT,
            0, // threshold
            scratch_new)))
        {
            assert(false);
            return nullptr;
        }

        scratch_final = std::move(scratch_new);
    }

    return scratch_final.GetImageCount() ? std::make_shared<DirectX::ScratchImage>(std::move(scratch_final)) : nullptr;
}

ff::texture::texture(const ff::resource_file& resource_file, DXGI_FORMAT new_format, size_t new_mip_count)
{
    auto data = ff::internal::load_texture_data(resource_file, new_format, new_mip_count, this->palette_);
    this->assign(ff::dxgi::create_static_texture(data, ff::dxgi::sprite_type::unknown));
}

ff::texture::texture(const std::shared_ptr<ff::dxgi::texture_base>& dxgi_texture, const std::shared_ptr<DirectX::ScratchImage>& palette)
    : palette_(palette)
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
        new_data = ::convert_texture_data(other_data, new_format, new_mip_count);
        if (new_data)
        {
            sprite_type = (new_data == other_data) ? other.dxgi_texture_->sprite_type() : ff::dxgi::get_sprite_type(*new_data);
            this->assign(ff::dxgi::create_static_texture(new_data, sprite_type));
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
            data = ::convert_texture_data(data, DXGI_FORMAT_R8G8B8A8_UNORM, this->dxgi_texture_->mip_count());
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

void ff::texture::draw_frame(ff::dxgi::draw_base& draw, const ff::transform& transform, float frame, const ff::dict* params)
{
    this->draw_animation(draw, transform);
}

void ff::texture::draw_animation(ff::dxgi::draw_base& draw, const ff::transform& transform) const
{
    draw.draw_sprite(this->sprite_data_, transform);
}

bool ff::texture::save_to_cache(ff::dict& dict) const
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

        std::shared_ptr<ff::data_base> blob_data = std::make_shared<ff::data_blob_dxtex>(std::move(blob));
        dict.set<ff::data_base>("data", blob_data, ff::saved_data_type::none);
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

        std::shared_ptr<ff::data_base> blob_data = std::make_shared<ff::data_blob_dxtex>(std::move(blob));
        dict.set<ff::data_base>("palette", blob_data, ff::saved_data_type::none);
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

        auto dxgi_texture = ff::dxgi::create_static_texture(data, ff::dxgi::sprite_type::unknown);
        std::shared_ptr<ff::texture> texture = std::make_shared<ff::texture>(dxgi_texture, palette);
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
        return {};
    }

    DirectX::ScratchImage data_scratch;
    if (data && FAILED(DirectX::LoadFromDDSMemory(
        data->data(), data->size(), DirectX::DDS_FLAGS_NONE, nullptr, data_scratch)))
    {
        assert(false);
        return {};
    }

    auto dxgi_texture = ff::dxgi::create_static_texture(data_scratch.GetImageCount() ? std::make_shared<DirectX::ScratchImage>(std::move(data_scratch)) : nullptr, sprite_type);
    auto texture = std::make_shared<ff::texture>(dxgi_texture, palette_scratch.GetImageCount() ? std::make_shared<DirectX::ScratchImage>(std::move(palette_scratch)) : nullptr);
    return *texture ? texture : nullptr;
}
