#include "pch.h"
#include "dxgi_util.h"
#include "graphics.h"
#include "palette_data.h"
#include "png_image.h"
#include "texture_util.h"

Microsoft::WRL::ComPtr<ID3D11Texture2D> ff::internal::create_texture(const DirectX::ScratchImage& data)
{
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;

    if (data.GetImageCount() &&
        SUCCEEDED(DirectX::CreateTexture(ff::graphics::internal::dx11_device(), data.GetImages(), data.GetImageCount(), data.GetMetadata(), &resource)) &&
        SUCCEEDED(resource.As(&texture)))
    {
        return texture;
    }

    assert(false);
    return nullptr;
}

static DirectX::ScratchImage load_texture_png(const std::filesystem::path& path, DXGI_FORMAT new_format, size_t new_mip_count, DirectX::ScratchImage* palette_scratch)
{
    DirectX::ScratchImage scratch_final;

    ff::file_mem_mapped png_file(path);
    if (!png_file)
    {
        assert(false);
        return scratch_final;
    }

    ff::png_image_reader png(png_file.data(), png_file.size());
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

    if (ff::internal::palette_format(new_format) && palette_scratch)
    {
        std::unique_ptr<DirectX::ScratchImage> palette_scratch_2 = png.pallete();
        if (palette_scratch_2)
        {
            *palette_scratch = std::move(*palette_scratch_2);
        }
    }

    new_format = ff::internal::fix_texture_format(new_format, scratch_final.GetMetadata().width, scratch_final.GetMetadata().height, new_mip_count);

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

    if (ff::internal::compressed_format(new_format))
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

static DirectX::ScratchImage load_texture_pal(const std::filesystem::path& path, DXGI_FORMAT new_format, size_t new_mip_count)
{
    DirectX::ScratchImage scratch_final;
    if (new_format != ff::internal::PALETTE_FORMAT || new_mip_count > 1)
    {
        assert(false);
        return scratch_final;
    }

    ff::file_mem_mapped pal_file(path);
    if (!pal_file || !pal_file.size() || (pal_file.size() % 3) != 0)
    {
        assert(false);
        return scratch_final;
    }

    if (FAILED(scratch_final.Initialize2D(ff::internal::PALETTE_FORMAT, ff::constants::palette_size, 1, 1, 1)))
    {
        assert(false);
        return scratch_final;
    }

    const DirectX::Image* image = scratch_final.GetImages();
    uint8_t* dest = image->pixels;
    std::memset(dest, 0, ff::constants::palette_row_bytes);

    for (const uint8_t* start = pal_file.data(), *end = start + pal_file.size(), *cur = start; cur < end; cur += 3, dest += 4)
    {
        dest[0] = cur[0]; // R
        dest[1] = cur[1]; // G
        dest[2] = cur[2]; // B
        dest[3] = 0xFF; // A
    }

    return scratch_final;
}

D3D_SRV_DIMENSION ff::internal::default_dimension(const D3D11_TEXTURE2D_DESC& desc)
{
    if (desc.ArraySize > 1)
    {
        return desc.SampleDesc.Count > 1
            ? D3D_SRV_DIMENSION_TEXTURE2DMSARRAY
            : D3D_SRV_DIMENSION_TEXTURE2DARRAY;
    }
    else
    {
        return desc.SampleDesc.Count > 1
            ? D3D_SRV_DIMENSION_TEXTURE2DMS
            : D3D_SRV_DIMENSION_TEXTURE2D;
    }
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ff::internal::create_default_texture_view(ID3D11Texture2D* texture)
{
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    if (texture)
    {
        D3D11_TEXTURE2D_DESC texture_desc;
        texture->GetDesc(&texture_desc);

        D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
        view_desc.Format = texture_desc.Format;
        view_desc.ViewDimension = ff::internal::default_dimension(texture_desc);

        switch (view_desc.ViewDimension)
        {
            case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY:
                view_desc.Texture2DMSArray.ArraySize = texture_desc.ArraySize;
                break;

            case D3D_SRV_DIMENSION_TEXTURE2DARRAY:
                view_desc.Texture2DArray.ArraySize = texture_desc.ArraySize;
                view_desc.Texture2DArray.MipLevels = texture_desc.MipLevels;
                break;

            case D3D_SRV_DIMENSION_TEXTURE2DMS:
                // nothing to define
                break;

            case D3D_SRV_DIMENSION_TEXTURE2D:
                view_desc.Texture2D.MipLevels = texture_desc.MipLevels;
                break;
        }

        if (FAILED(ff::graphics::internal::dx11_device()->CreateShaderResourceView(texture, &view_desc, &view)))
        {
            assert(false);
            return nullptr;
        }
    }

    return view;
}

std::shared_ptr<DirectX::ScratchImage> ff::internal::load_texture_data(
    const std::filesystem::path& path,
    DXGI_FORMAT new_format,
    size_t new_mip_count,
    std::shared_ptr<DirectX::ScratchImage>& palette)
{
    DirectX::ScratchImage scratch_data;
    DirectX::ScratchImage scratch_palette;

    std::string path_ext = ff::filesystem::to_lower(path.extension()).string();
    if (path_ext == ".pal")
    {
        scratch_data = ::load_texture_pal(path, new_format, new_mip_count);
    }
    else if (path_ext == ".png")
    {
        scratch_data = ::load_texture_png(path, new_format, new_mip_count, &scratch_palette);
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

std::shared_ptr<DirectX::ScratchImage> ff::internal::convert_texture_data(const std::shared_ptr<DirectX::ScratchImage>& data, DXGI_FORMAT new_format, size_t new_mip_count)
{
    if (!data || !data->GetImageCount())
    {
        return nullptr;
    }

    new_format = ff::internal::fix_texture_format(new_format, data->GetMetadata().width, data->GetMetadata().height, new_mip_count);

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

    if (ff::internal::compressed_format(scratch_final.GetMetadata().format))
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

    if (ff::internal::compressed_format(new_format))
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

DXGI_FORMAT ff::internal::fix_texture_format(DXGI_FORMAT format, size_t texture_width, size_t texture_height, size_t mip_count)
{
    if (format == DXGI_FORMAT_UNKNOWN)
    {
        format = ff::internal::DEFAULT_FORMAT;
    }
    else if (ff::internal::compressed_format(format))
    {
        // Compressed images have size restrictions. Upon failure, just use RGB
        if (texture_width % 4 || texture_height % 4)
        {
            format = ff::internal::DEFAULT_FORMAT;
        }
        else if (mip_count > 1 && (ff::math::nearest_power_of_two(texture_width) != texture_width || ff::math::nearest_power_of_two(texture_height) != texture_height))
        {
            format = ff::internal::DEFAULT_FORMAT;
        }
    }

    return format;
}

ff::sprite_type ff::internal::get_sprite_type(const DirectX::ScratchImage& scratch, const ff::rect_size* rect)
{
    DirectX::ScratchImage alpha_scratch;
    const DirectX::Image* alpha_image = nullptr;
    size_t alpha_gap = 1;
    DXGI_FORMAT format = scratch.GetMetadata().format;

    if (ff::internal::palette_format(format))
    {
        return ff::sprite_type::opaque_palette;
    }
    else if (!ff::internal::has_alpha(format))
    {
        return ff::sprite_type::opaque;
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
    else if (ff::internal::compressed_format(format))
    {
        if (FAILED(DirectX::Decompress(
            scratch.GetImages(),
            scratch.GetImageCount(),
            scratch.GetMetadata(),
            DXGI_FORMAT_A8_UNORM,
            alpha_scratch)))
        {
            assert(false);
            return ff::sprite_type::unknown;
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
            return ff::sprite_type::unknown;
        }

        alpha_image = alpha_scratch.GetImages();
    }

    ff::sprite_type newType = ff::sprite_type::opaque;
    ff::rect_size size(0, 0, scratch.GetMetadata().width, scratch.GetMetadata().height);
    rect = rect ? rect : &size;

    for (size_t y = rect->top; y < rect->bottom && newType == ff::sprite_type::opaque; y++)
    {
        const uint8_t* alpha = alpha_image->pixels + y * alpha_image->rowPitch + rect->left + alpha_gap - 1;
        for (size_t x = rect->left; x < rect->right; x++, alpha += alpha_gap)
        {
            if (*alpha && *alpha != 0xFF)
            {
                newType = ff::sprite_type::transparent;
                break;
            }
        }
    }

    return newType;
}
