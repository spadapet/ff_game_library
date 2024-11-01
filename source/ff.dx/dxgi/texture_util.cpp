#include "pch.h"
#include "dxgi/format_util.h"
#include "dxgi/sprite_type.h"
#include "dxgi/texture_util.h"

std::shared_ptr<DirectX::ScratchImage> ff::dxgi::convert_texture_data(const std::shared_ptr<DirectX::ScratchImage>& data, DXGI_FORMAT new_format, size_t new_mip_count)
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

ff::dxgi::sprite_type ff::dxgi::get_sprite_type(const DirectX::ScratchImage& scratch, const ff::rect_size* rect)
{
    DirectX::ScratchImage alpha_scratch;
    const DirectX::Image* alpha_image = nullptr;
    size_t alpha_gap = 1;
    DXGI_FORMAT format = scratch.GetMetadata().format;

    if (ff::dxgi::palette_format(format))
    {
        return ff::dxgi::sprite_type::opaque_palette;
    }
    else if (!ff::dxgi::has_alpha(format))
    {
        return ff::dxgi::sprite_type::opaque;
    }
    else if (format == DXGI_FORMAT_A8_UNORM)
    {
        alpha_image = scratch.GetImages();
    }
    else if (format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM)
    {
        alpha_image = scratch.GetImages();
        alpha_gap = 4;
    }
    else if (ff::dxgi::compressed_format(format))
    {
        if (FAILED(DirectX::Decompress(
            scratch.GetImages(),
            scratch.GetImageCount(),
            scratch.GetMetadata(),
            DXGI_FORMAT_A8_UNORM,
            alpha_scratch)))
        {
            assert(false);
            return ff::dxgi::sprite_type::unknown;
        }

        alpha_image = alpha_scratch.GetImages();
    }
    else
    {
        if (FAILED(DirectX::Convert(
            scratch.GetImages(), 1,
            scratch.GetMetadata(),
            DXGI_FORMAT_A8_UNORM,
            DirectX::TEX_FILTER_DEFAULT,
            0, alpha_scratch)))
        {
            assert(false);
            return ff::dxgi::sprite_type::unknown;
        }

        alpha_image = alpha_scratch.GetImages();
    }

    ff::dxgi::sprite_type newType = ff::dxgi::sprite_type::opaque;
    ff::rect_size size(0, 0, scratch.GetMetadata().width, scratch.GetMetadata().height);
    rect = rect ? rect : &size;

    for (size_t y = rect->top; y < rect->bottom && newType == ff::dxgi::sprite_type::opaque; y++)
    {
        const uint8_t* alpha = alpha_image->pixels + y * alpha_image->rowPitch + rect->left + alpha_gap - 1;
        for (size_t x = rect->left; x < rect->right; x++, alpha += alpha_gap)
        {
            if (*alpha && *alpha != 0xFF)
            {
                newType = ff::dxgi::sprite_type::transparent;
                break;
            }
        }
    }

    return newType;
}
