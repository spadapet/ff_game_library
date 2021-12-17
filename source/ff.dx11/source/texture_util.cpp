#include "pch.h"
#include "globals.h"
#include "texture_util.h"

size_t ff::dx11::fix_sample_count(DXGI_FORMAT format, size_t sample_count)
{
    size_t fixed_sample_count = ff::math::nearest_power_of_two(sample_count);
    assert(fixed_sample_count == sample_count);

    UINT levels = 0;
    while (fixed_sample_count > 1 && (FAILED(ff::dx11::device()->CheckMultisampleQualityLevels(
        format, static_cast<UINT>(fixed_sample_count), &levels)) || !levels))
    {
        fixed_sample_count /= 2;
    }

    return std::max<size_t>(fixed_sample_count, 1);
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> ff::dx11::create_texture(const DirectX::ScratchImage& data)
{
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;

    if (data.GetImageCount() &&
        SUCCEEDED(DirectX::CreateTexture(ff::dx11::device(), data.GetImages(), data.GetImageCount(), data.GetMetadata(), &resource)) &&
        SUCCEEDED(resource.As(&texture)))
    {
        return texture;
    }

    assert(false);
    return nullptr;
}

D3D_SRV_DIMENSION ff::dx11::default_shader_dimension(const D3D11_TEXTURE2D_DESC& desc)
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

D3D11_RTV_DIMENSION ff::dx11::default_target_dimension(const D3D11_TEXTURE2D_DESC& desc)
{
    if (desc.ArraySize > 1)
    {
        return desc.SampleDesc.Count > 1
            ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY
            : D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    }
    else
    {
        return desc.SampleDesc.Count > 1
            ? D3D11_RTV_DIMENSION_TEXTURE2DMS
            : D3D11_RTV_DIMENSION_TEXTURE2D;
    }
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ff::dx11::create_shader_view(
    ID3D11Texture2D* texture,
    size_t array_start,
    size_t array_count,
    size_t mip_start,
    size_t mip_count)
{
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    if (texture)
    {
        D3D11_TEXTURE2D_DESC texture_desc;
        texture->GetDesc(&texture_desc);

        D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
        view_desc.Format = texture_desc.Format;
        view_desc.ViewDimension = ff::dx11::default_shader_dimension(texture_desc);

        switch (view_desc.ViewDimension)
        {
            case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY:
                view_desc.Texture2DMSArray.FirstArraySlice = static_cast<UINT>(array_start);
                view_desc.Texture2DMSArray.ArraySize = array_count ? static_cast<UINT>(array_count) : texture_desc.ArraySize - view_desc.Texture2DMSArray.FirstArraySlice;
                break;

            case D3D_SRV_DIMENSION_TEXTURE2DARRAY:
                view_desc.Texture2DArray.FirstArraySlice = static_cast<UINT>(array_start);
                view_desc.Texture2DArray.ArraySize = array_count ? static_cast<UINT>(array_count) : texture_desc.ArraySize - view_desc.Texture2DMSArray.FirstArraySlice;
                view_desc.Texture2DArray.MostDetailedMip = static_cast<UINT>(mip_start);
                view_desc.Texture2DArray.MipLevels = mip_count ? static_cast<UINT>(mip_count) : texture_desc.MipLevels - view_desc.Texture2DArray.MostDetailedMip;
                break;

            case D3D_SRV_DIMENSION_TEXTURE2D:
                view_desc.Texture2D.MostDetailedMip = static_cast<UINT>(mip_start);
                view_desc.Texture2D.MipLevels = mip_count ? static_cast<UINT>(mip_count) : texture_desc.MipLevels - view_desc.Texture2DArray.MostDetailedMip;
                break;
        }

        if (FAILED(ff::dx11::device()->CreateShaderResourceView(texture, &view_desc, view.GetAddressOf())))
        {
            assert(false);
            return nullptr;
        }
    }

    return view;
}

Microsoft::WRL::ComPtr<ID3D11RenderTargetView> ff::dx11::create_target_view(ID3D11Texture2D* texture, size_t array_start, size_t array_count, size_t mip_level)
{
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view;
    if (texture)
    {
        D3D11_TEXTURE2D_DESC texture_desc;
        texture->GetDesc(&texture_desc);

        D3D11_RENDER_TARGET_VIEW_DESC view_desc{};
        view_desc.Format = texture_desc.Format;
        view_desc.ViewDimension = ff::dx11::default_target_dimension(texture_desc);

        switch (view_desc.ViewDimension)
        {
            case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY:
                view_desc.Texture2DMSArray.FirstArraySlice = static_cast<UINT>(array_start);
                view_desc.Texture2DMSArray.ArraySize = array_count ? static_cast<UINT>(array_count) : texture_desc.ArraySize - view_desc.Texture2DMSArray.FirstArraySlice;
                break;

            case D3D_SRV_DIMENSION_TEXTURE2DARRAY:
                view_desc.Texture2DArray.FirstArraySlice = static_cast<UINT>(array_start);
                view_desc.Texture2DArray.ArraySize = array_count ? static_cast<UINT>(array_count) : texture_desc.ArraySize - view_desc.Texture2DMSArray.FirstArraySlice;
                view_desc.Texture2DArray.MipSlice = static_cast<UINT>(mip_level);
                break;

            case D3D_SRV_DIMENSION_TEXTURE2D:
                view_desc.Texture2D.MipSlice = static_cast<UINT>(mip_level);
                break;
        }

        if (FAILED(ff::dx11::device()->CreateRenderTargetView(texture, &view_desc, view.GetAddressOf())))
        {
            assert(false);
            return nullptr;
        }
    }

    return view;
}
