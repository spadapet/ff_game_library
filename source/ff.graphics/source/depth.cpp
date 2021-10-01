#include "pch.h"
#include "depth.h"

#if DXVER == 11

static const DXGI_FORMAT DEPTH_STENCIL_FORMAT = DXGI_FORMAT_D24_UNORM_S8_UINT;

ff::depth::depth(size_t sample_count)
    : depth(ff::point_int(1, 1), sample_count)
{}

ff::depth::depth(const ff::point_int& size, size_t sample_count)
{
    ff::internal::dx11::add_device_child(this, ff::internal::dx11::device_reset_priority::normal);

    D3D11_TEXTURE2D_DESC texture_desc{};
    texture_desc.ArraySize = 1;
    texture_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    texture_desc.Format = ::DEPTH_STENCIL_FORMAT;
    texture_desc.Width = static_cast<UINT>(std::max(size.x, 1));
    texture_desc.Height = static_cast<UINT>(std::max(size.y, 1));
    texture_desc.MipLevels = 1;
    texture_desc.SampleDesc.Count = static_cast<UINT>(ff::internal::dx11::fix_sample_count(::DEPTH_STENCIL_FORMAT, sample_count));
    texture_desc.Usage = D3D11_USAGE_DEFAULT;

    D3D11_DEPTH_STENCIL_VIEW_DESC view_desc{};
    view_desc.Format = texture_desc.Format;
    view_desc.ViewDimension = (texture_desc.SampleDesc.Count > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;

    bool status =
        SUCCEEDED(ff::dx11::device()->CreateTexture2D(&texture_desc, nullptr, this->texture_.GetAddressOf())) &&
        SUCCEEDED(ff::dx11::device()->CreateDepthStencilView(this->texture(), &view_desc, this->view_.GetAddressOf()));
    assert(status);
}

ff::depth::~depth()
{
    ff::internal::dx11::remove_device_child(this);
}

ff::depth::operator bool() const
{
    return this->texture_ && this->view_;
}

ff::point_int ff::depth::size() const
{
    D3D11_TEXTURE2D_DESC desc;
    this->texture_->GetDesc(&desc);
    return ff::point_t<UINT>(desc.Width, desc.Height).cast<int>();
}

bool ff::depth::size(const ff::point_int& size)
{
    if (this->size() != size)
    {
        depth new_depth(size, this->sample_count());
        if (!new_depth)
        {
            assert(false);
            return false;
        }

        *this = std::move(new_depth);
    }

    return true;
}

size_t ff::depth::sample_count() const
{
    D3D11_TEXTURE2D_DESC desc;
    this->texture_->GetDesc(&desc);
    return static_cast<size_t>(desc.SampleDesc.Count);
}

void ff::depth::clear(float depth, BYTE stencil) const
{
    ff::dx11::get_device_state().clear_depth_stencil(this->view(), true, true, depth, stencil);
}

void ff::depth::clear_depth(float depth) const
{
    ff::dx11::get_device_state().clear_depth_stencil(this->view(), true, false, depth, 0);
}

void ff::depth::clear_stencil(BYTE stencil) const
{
    ff::dx11::get_device_state().clear_depth_stencil(this->view(), false, true, 0, stencil);
}

ID3D11Texture2D* ff::depth::texture() const
{
    return this->texture_.Get();
}

ID3D11DepthStencilView* ff::depth::view() const
{
    return this->view_.Get();
}

bool ff::depth::reset()
{
    D3D11_TEXTURE2D_DESC desc;
    this->texture_->GetDesc(&desc);
    *this = depth(this->size(), this->sample_count());

    return *this;
}

#endif
