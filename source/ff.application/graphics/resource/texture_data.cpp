#include "pch.h"
#include "graphics/dxgi/format_util.h"
#include "graphics/resource/palette_data.h"
#include "graphics/resource/png_image.h"
#include "graphics/resource/texture_data.h"

static DirectX::ScratchImage load_texture_png(
    const ff::resource_file& resource_file,
    DXGI_FORMAT new_format,
    size_t new_mip_count,
    DirectX::ScratchImage& palette_scratch)
{
    DirectX::ScratchImage scratch_final;

    std::shared_ptr<ff::data_base> data = resource_file.loaded_data();
    if (!data || !data->size())
    {
        assert(false);
        return scratch_final;
    }

    ff::png_image_reader png(data->data(), data->size());
    {
        std::unique_ptr<DirectX::ScratchImage> scratch_temp = png.read(new_format);
        if (scratch_temp)
        {
            scratch_final = std::move(*scratch_temp);
        }
        else
        {
            assert(false);
            return scratch_final;
        }
    }

    if (new_format == DXGI_FORMAT_UNKNOWN)
    {
        new_format = scratch_final.GetMetadata().format;
    }

    if (ff::dxgi::palette_format(new_format))
    {
        std::unique_ptr<DirectX::ScratchImage> palette_scratch_2 = png.palette();
        if (palette_scratch_2)
        {
            palette_scratch = std::move(*palette_scratch_2);
        }
    }

    new_format = ff::dxgi::fix_format(new_format, scratch_final.GetMetadata().width, scratch_final.GetMetadata().height, new_mip_count);

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
            return DirectX::ScratchImage();
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
            return DirectX::ScratchImage();
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
            return DirectX::ScratchImage();
        }

        scratch_final = std::move(scratch_new);
    }

    return scratch_final;
}

static DirectX::ScratchImage load_texture_pal(const ff::resource_file& resource_file, DXGI_FORMAT new_format, size_t new_mip_count)
{
    DirectX::ScratchImage scratch_final;
    if (new_format != DXGI_FORMAT_R8G8B8A8_UNORM || new_mip_count > 1)
    {
        assert(false);
        return scratch_final;
    }

    std::shared_ptr<ff::data_base> data = resource_file.loaded_data();
    if (!data || !data->size() || (data->size() % 3) != 0)
    {
        assert(false);
        return scratch_final;
    }

    if (FAILED(scratch_final.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, ff::dxgi::palette_size, 1, 1, 1)))
    {
        assert(false);
        return scratch_final;
    }

    const DirectX::Image* image = scratch_final.GetImages();
    uint8_t* dest = image->pixels;
    std::memset(dest, 0, ff::dxgi::palette_row_bytes);

    for (const uint8_t* start = data->data(), *end = start + data->size(), *cur = start; cur < end; cur += 3, dest += 4)
    {
        dest[0] = cur[0]; // R
        dest[1] = cur[1]; // G
        dest[2] = cur[2]; // B
        dest[3] = 0xFF; // A
    }

    return scratch_final;
}

std::shared_ptr<DirectX::ScratchImage> ff::internal::load_texture_data(
    const ff::resource_file& resource_file,
    DXGI_FORMAT new_format,
    size_t new_mip_count,
    std::shared_ptr<DirectX::ScratchImage>& palette)
{
    DirectX::ScratchImage scratch_data;
    DirectX::ScratchImage scratch_palette;

    std::string path_ext = resource_file.file_extension();
    if (path_ext == ".pal")
    {
        scratch_data = ::load_texture_pal(resource_file, new_format, new_mip_count);
    }
    else if (path_ext == ".png")
    {
        scratch_data = ::load_texture_png(resource_file, new_format, new_mip_count, scratch_palette);
    }

    if (scratch_palette.GetImageCount())
    {
        palette = std::make_shared<DirectX::ScratchImage>(std::move(scratch_palette));
    }

    if (scratch_data.GetImageCount())
    {
        return std::make_shared<DirectX::ScratchImage>(std::move(scratch_data));
    }

    assert(false);
    return nullptr;
}
