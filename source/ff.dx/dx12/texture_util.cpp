#include "pch.h"
#include "dx12/access.h"
#include "dx12/globals.h"
#include "dx12/resource.h"
#include "dx12/texture_util.h"

static D3D12_SRV_DIMENSION default_shader_dimension(const D3D12_RESOURCE_DESC& desc)
{
    if (desc.DepthOrArraySize > 1)
    {
        return desc.SampleDesc.Count > 1
            ? D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY
            : D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    }
    else
    {
        return desc.SampleDesc.Count > 1
            ? D3D12_SRV_DIMENSION_TEXTURE2DMS
            : D3D12_SRV_DIMENSION_TEXTURE2D;
    }
}

static D3D12_RTV_DIMENSION default_target_dimension(const D3D12_RESOURCE_DESC& desc)
{
    if (desc.DepthOrArraySize > 1)
    {
        return desc.SampleDesc.Count > 1
            ? D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY
            : D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
    }
    else
    {
        return desc.SampleDesc.Count > 1
            ? D3D12_RTV_DIMENSION_TEXTURE2DMS
            : D3D12_RTV_DIMENSION_TEXTURE2D;
    }
}

size_t ff::dx12::fix_sample_count(DXGI_FORMAT format, size_t sample_count)
{
    size_t fixed_sample_count = ff::math::nearest_power_of_two(sample_count);
    assert(fixed_sample_count == sample_count);

    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS levels{};
    levels.Format = format;
    levels.SampleCount = static_cast<UINT>(fixed_sample_count);

    while (fixed_sample_count > 1 && (FAILED(ff::dx12::device()->CheckFeatureSupport(
        D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &levels, sizeof(levels))) || !levels.NumQualityLevels))
    {
        fixed_sample_count /= 2;
    }

    return std::max<size_t>(fixed_sample_count, 1);
}

void ff::dx12::create_shader_view(const ff::dx12::resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE view, size_t array_start, size_t array_count, size_t mip_start, size_t mip_count)
{
    const D3D12_RESOURCE_DESC& texture_desc = resource->desc();
    D3D12_SHADER_RESOURCE_VIEW_DESC view_desc{};
    view_desc.Format = texture_desc.Format;
    view_desc.ViewDimension = ::default_shader_dimension(texture_desc);
    view_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    switch (view_desc.ViewDimension)
    {
        case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
            view_desc.Texture2DMSArray.FirstArraySlice = static_cast<UINT>(array_start);
            view_desc.Texture2DMSArray.ArraySize = array_count ? static_cast<UINT>(array_count) : texture_desc.DepthOrArraySize - view_desc.Texture2DMSArray.FirstArraySlice;
            break;

        case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
            view_desc.Texture2DArray.FirstArraySlice = static_cast<UINT>(array_start);
            view_desc.Texture2DArray.ArraySize = array_count ? static_cast<UINT>(array_count) : texture_desc.DepthOrArraySize - view_desc.Texture2DMSArray.FirstArraySlice;
            view_desc.Texture2DArray.MostDetailedMip = static_cast<UINT>(mip_start);
            view_desc.Texture2DArray.MipLevels = mip_count ? static_cast<UINT>(mip_count) : texture_desc.MipLevels - view_desc.Texture2DArray.MostDetailedMip;
            break;

        case D3D12_SRV_DIMENSION_TEXTURE2D:
            view_desc.Texture2D.MostDetailedMip = static_cast<UINT>(mip_start);
            view_desc.Texture2D.MipLevels = mip_count ? static_cast<UINT>(mip_count) : texture_desc.MipLevels - view_desc.Texture2DArray.MostDetailedMip;
            break;
    }

    ff::dx12::device()->CreateShaderResourceView(ff::dx12::get_resource(*resource), &view_desc, view);
}

void ff::dx12::create_target_view(const ff::dx12::resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE view, size_t array_start, size_t array_count, size_t mip_level)
{
    const D3D12_RESOURCE_DESC& texture_desc = resource->desc();
    D3D12_RENDER_TARGET_VIEW_DESC view_desc{};
    view_desc.Format = texture_desc.Format;
    view_desc.ViewDimension = ::default_target_dimension(texture_desc);

    switch (view_desc.ViewDimension)
    {
        case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY:
            view_desc.Texture2DMSArray.FirstArraySlice = static_cast<UINT>(array_start);
            view_desc.Texture2DMSArray.ArraySize = array_count ? static_cast<UINT>(array_count) : texture_desc.DepthOrArraySize - view_desc.Texture2DMSArray.FirstArraySlice;
            break;

        case D3D_SRV_DIMENSION_TEXTURE2DARRAY:
            view_desc.Texture2DArray.FirstArraySlice = static_cast<UINT>(array_start);
            view_desc.Texture2DArray.ArraySize = array_count ? static_cast<UINT>(array_count) : texture_desc.DepthOrArraySize - view_desc.Texture2DMSArray.FirstArraySlice;
            view_desc.Texture2DArray.MipSlice = static_cast<UINT>(mip_level);
            break;

        case D3D_SRV_DIMENSION_TEXTURE2D:
            view_desc.Texture2D.MipSlice = static_cast<UINT>(mip_level);
            break;
    }

    ff::dx12::device()->CreateRenderTargetView(ff::dx12::get_resource(*resource), &view_desc, view);
}
