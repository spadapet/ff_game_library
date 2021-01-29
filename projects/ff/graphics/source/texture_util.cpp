#include "pch.h"
#include "palette_data.h"
#include "png_image.h"
#include "texture_util.h"

static DirectX::ScratchImage load_texture_png(const std::filesystem::path& path, DXGI_FORMAT format, size_t mips, DirectX::ScratchImage* palette_scratch)
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
        std::unique_ptr<DirectX::ScratchImage> scratch_temp = png.read(format);
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

    if (format == DXGI_FORMAT_UNKNOWN)
    {
        format = scratch_final.GetMetadata().format;
    }

    if (format == DXGI_FORMAT_R8_UINT && palette_scratch)
    {
        std::unique_ptr<DirectX::ScratchImage> palette_scratch_2 = png.pallete();
        if (palette_scratch_2)
        {
            *palette_scratch = std::move(*palette_scratch_2);
        }
    }

    if (DirectX::IsCompressed(format))
    {
        // Compressed images have size restrictions. Upon failure, just use RGB
        size_t width = scratch_final.GetMetadata().width;
        size_t height = scratch_final.GetMetadata().height;

        if (width % 4 || height % 4)
        {
            format = DXGI_FORMAT_R8G8B8A8_UNORM;
        }
        else if (mips != 1 && (ff::math::nearest_power_of_two(width) != width || ff::math::nearest_power_of_two(height) != height))
        {
            format = DXGI_FORMAT_R8G8B8A8_UNORM;
        }
    }

    if (mips != 1)
    {
        DirectX::ScratchImage scratch_mips;
        if (FAILED(DirectX::GenerateMipMaps(
            scratch_final.GetImages(),
            scratch_final.GetImageCount(),
            scratch_final.GetMetadata(),
            DirectX::TEX_FILTER_DEFAULT,
            mips,
            scratch_mips)))
        {
            assert(false);
            return DirectX::ScratchImage();
        }

        scratch_final = std::move(scratch_mips);
    }

    if (DirectX::IsCompressed(format))
    {
        DirectX::ScratchImage scratch_new;
        if (FAILED(DirectX::Compress(
            scratch_final.GetImages(),
            scratch_final.GetImageCount(),
            scratch_final.GetMetadata(),
            format,
            DirectX::TEX_COMPRESS_DEFAULT,
            0, // alpharef
            scratch_new)))
        {
            assert(false);
            return DirectX::ScratchImage();
        }

        scratch_final = std::move(scratch_new);
    }
    else if (format != scratch_final.GetMetadata().format)
    {
        DirectX::ScratchImage scratch_new;
        if (FAILED(DirectX::Convert(
            scratch_final.GetImages(),
            scratch_final.GetImageCount(),
            scratch_final.GetMetadata(),
            format,
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

static DirectX::ScratchImage load_texture_pal(const std::filesystem::path& path, DXGI_FORMAT format, size_t mips)
{
    DirectX::ScratchImage scratch_final;
    if (format != DXGI_FORMAT_R8G8B8A8_UNORM || mips > 1)
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

    if (FAILED(scratch_final.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, ff::constants::palette_size, 1, 1, 1)))
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

static D3D_SRV_DIMENSION default_dimension(const D3D11_TEXTURE2D_DESC& desc)
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

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ff::internal::create_default_texture_view(ID3D11DeviceX* device, ID3D11Texture2D* texture)
{
    D3D11_TEXTURE2D_DESC texture_desc;
    texture->GetDesc(&texture_desc);

    D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
    view_desc.Format = texture_desc.Format;
    view_desc.ViewDimension = ::default_dimension(texture_desc);

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

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    if (FAILED(device->CreateShaderResourceView(texture, &view_desc, &view)))
    {
        assert(false);
        return nullptr;
    }

    return view;
}

DirectX::ScratchImage ff::internal::load_texture_data(const std::filesystem::path& path, DXGI_FORMAT format, size_t mips, DirectX::ScratchImage* palette_scratch)
{
    std::string path_ext = ff::filesystem::to_lower(path.extension()).string();
    if (path_ext == ".pal")
    {
        return ::load_texture_pal(path, format, mips);
    }
    else if (path_ext == ".png")
    {
        return ::load_texture_png(path, format, mips, palette_scratch);
    }
    else
    {
        assert(false);
        return DirectX::ScratchImage();
    }
}

DirectX::ScratchImage ff::internal::convert_texture_data(const DirectX::ScratchImage& data, DXGI_FORMAT format, size_t mips)
{
    DirectX::ScratchImage scratch_final;
    if (!data.GetImageCount() || FAILED(scratch_final.InitializeFromImage(*data.GetImages())))
    {
        assert(false);
        return DirectX::ScratchImage();
    }

    if (DirectX::IsCompressed(format))
    {
        // Compressed images have size restrictions. Upon failure, just use RGB
        size_t width = scratch_final.GetMetadata().width;
        size_t height = scratch_final.GetMetadata().height;

        if (width % 4 || height % 4)
        {
            format = DXGI_FORMAT_R8G8B8A8_UNORM;
        }
        else if (mips != 1 && (ff::math::nearest_power_of_two(width) != width || ff::math::nearest_power_of_two(height) != height))
        {
            format = DXGI_FORMAT_R8G8B8A8_UNORM;
        }
    }

    if (scratch_final.GetMetadata().format != format || scratch_final.GetMetadata().mipLevels != mips)
    {
        if (DirectX::IsCompressed(scratch_final.GetMetadata().format))
        {
            DirectX::ScratchImage scratchRgb;
            if (FAILED(DirectX::Decompress(
                scratch_final.GetImages(),
                scratch_final.GetImageCount(),
                scratch_final.GetMetadata(),
                DXGI_FORMAT_R8G8B8A8_UNORM,
                scratchRgb)))
            {
                assert(false);
                return DirectX::ScratchImage();
            }

            scratch_final = std::move(scratchRgb);
        }
        else if (scratch_final.GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM)
        {
            DirectX::ScratchImage scratchRgb;
            if (FAILED(DirectX::Convert(
                scratch_final.GetImages(),
                scratch_final.GetImageCount(),
                scratch_final.GetMetadata(),
                DXGI_FORMAT_R8G8B8A8_UNORM,
                DirectX::TEX_FILTER_DEFAULT,
                0, // threshold
                scratchRgb)))
            {
                assert(false);
                return DirectX::ScratchImage();
            }

            scratch_final = std::move(scratchRgb);
        }

        if (mips != 1)
        {
            DirectX::ScratchImage scratch_mips;
            if (FAILED(DirectX::GenerateMipMaps(
                scratch_final.GetImages(),
                scratch_final.GetImageCount(),
                scratch_final.GetMetadata(),
                DirectX::TEX_FILTER_DEFAULT,
                mips,
                scratch_mips)))
            {
                assert(false);
                return DirectX::ScratchImage();
            }

            scratch_final = std::move(scratch_mips);
        }

        if (DirectX::IsCompressed(format))
        {
            DirectX::ScratchImage scratch_new;
            if (FAILED(DirectX::Compress(
                scratch_final.GetImages(),
                scratch_final.GetImageCount(),
                scratch_final.GetMetadata(),
                format,
                DirectX::TEX_COMPRESS_DEFAULT,
                0, // alpharef
                scratch_new)))
            {
                assert(false);
                return DirectX::ScratchImage();
            }

            scratch_final = std::move(scratch_new);
        }
        else if (format != scratch_final.GetMetadata().format)
        {
            DirectX::ScratchImage scratch_new;
            if (FAILED(DirectX::Convert(
                scratch_final.GetImages(),
                scratch_final.GetImageCount(),
                scratch_final.GetMetadata(),
                format,
                DirectX::TEX_FILTER_DEFAULT,
                0, // threshold
                scratch_new)))
            {
                assert(false);
                return DirectX::ScratchImage();
            }

            scratch_final = std::move(scratch_new);
        }
    }

    return scratch_final;
}

ff::sprite_type ff::internal::get_sprite_type(const DirectX::ScratchImage& scratch, const ff::rect_size* rect)
{
    DirectX::ScratchImage alphaScratch;
    const DirectX::Image* alphaImage = nullptr;
    size_t alphaGap = 1;
    DXGI_FORMAT format = scratch.GetMetadata().format;

    if (format == DXGI_FORMAT_R8_UINT)
    {
        return ff::sprite_type::opaque_palette;
    }
    else if (!DirectX::HasAlpha(format))
    {
        return ff::sprite_type::opaque;
    }
    else if (format == DXGI_FORMAT_A8_UNORM)
    {
        alphaImage = scratch.GetImages();
    }
    else if (format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM)
    {
        alphaImage = scratch.GetImages();
        alphaGap = 4;
    }
    else if (DirectX::IsCompressed(format))
    {
        if (FAILED(DirectX::Decompress(
            scratch.GetImages(),
            scratch.GetImageCount(),
            scratch.GetMetadata(),
            DXGI_FORMAT_A8_UNORM,
            alphaScratch)))
        {
            assert(false);
            return ff::sprite_type::unknown;
        }

        alphaImage = alphaScratch.GetImages();
    }
    else
    {
        if (FAILED(DirectX::Convert(
            scratch.GetImages(), 1,
            scratch.GetMetadata(),
            DXGI_FORMAT_A8_UNORM,
            DirectX::TEX_FILTER_DEFAULT,
            0, alphaScratch)))
        {
            assert(false);
            return ff::sprite_type::unknown;
        }

        alphaImage = alphaScratch.GetImages();
    }

    ff::sprite_type newType = ff::sprite_type::opaque;
    ff::rect_size size(0, 0, scratch.GetMetadata().width, scratch.GetMetadata().height);
    rect = rect ? rect : &size;

    for (size_t y = rect->top; y < rect->bottom && newType == ff::sprite_type::opaque; y++)
    {
        const uint8_t* alpha = alphaImage->pixels + y * alphaImage->rowPitch + rect->left + alphaGap - 1;
        for (size_t x = rect->left; x < rect->right; x++, alpha += alphaGap)
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
