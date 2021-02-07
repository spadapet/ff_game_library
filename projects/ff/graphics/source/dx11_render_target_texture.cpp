#include "pch.h"
#include "dx11_render_target_texture.h"
#include "dx11_texture.h"
#include "graphics.h"
#include "texture_util.h"

ff::dx11_render_target_texture::dx11_render_target_texture(
    const std::shared_ptr<ff::dx11_texture>& texture,
    size_t array_start,
    size_t array_count,
    size_t mip_level)
    : texture_(texture)
    , array_start(array_start)
    , array_count(array_count)
    , mip_level(mip_level)
{
    ID3D11Texture2D* texture_2d = texture->texture();
    if (texture_2d)
    {
        D3D11_TEXTURE2D_DESC texture_desc;
        texture_2d->GetDesc(&texture_desc);
        array_count = array_count ? array_count : texture_desc.ArraySize - array_count;

        D3D11_RENDER_TARGET_VIEW_DESC view_desc{};
        view_desc.Format = texture_desc.Format;
        view_desc.ViewDimension = ff::internal::default_render_dimension(texture_desc);

        switch (view_desc.ViewDimension)
        {
            case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY:
                view_desc.Texture2DMSArray.FirstArraySlice = static_cast<UINT>(array_start);
                view_desc.Texture2DMSArray.ArraySize = static_cast<UINT>(array_count);
                break;

            case D3D_SRV_DIMENSION_TEXTURE2DARRAY:
                view_desc.Texture2DArray.FirstArraySlice = static_cast<UINT>(array_start);
                view_desc.Texture2DArray.ArraySize = static_cast<UINT>(array_count);
                view_desc.Texture2DArray.MipSlice = static_cast<UINT>(mip_level);
                break;

            case D3D_SRV_DIMENSION_TEXTURE2D:
                view_desc.Texture2D.MipSlice = static_cast<UINT>(mip_level);
                break;
        }

        HRESULT hr = ff::graphics::internal::dx11_device()->CreateRenderTargetView(texture_2d, &view_desc, this->view_.GetAddressOf());
        assert(SUCCEEDED(hr));
    }
}

ff::dx11_render_target_texture::operator bool() const
{
    return this->view_;
}

DXGI_FORMAT ff::dx11_render_target_texture::format() const
{
    return this->texture_->format();
}

ff::window_size ff::dx11_render_target_texture::size() const
{
    return ff::window_size{ this->texture_->size(), 1.0, DMDO_DEFAULT, DMDO_DEFAULT };
}

ID3D11Texture2D* ff::dx11_render_target_texture::texture()
{
    return this->texture_->texture();
}

ID3D11RenderTargetView* ff::dx11_render_target_texture::view()
{
    return this->view_.Get();
}

bool ff::dx11_render_target_texture::reset()
{
    *this = dx11_render_target_texture(this->texture_, this->array_start, this->array_count, this->mip_level);
    return *this;
}
