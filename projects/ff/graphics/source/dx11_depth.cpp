#include "pch.h"
#include "dx11_depth.h"
#include "dx11_device_state.h"
#include "graphics.h"

static const DXGI_FORMAT DEPTH_STENCIL_FORMAT = DXGI_FORMAT_D24_UNORM_S8_UINT;

ff::dx11_depth::dx11_depth(size_t sample_count)
    : dx11_depth(ff::point_int::ones(), sample_count)
{}

ff::dx11_depth::dx11_depth(const ff::point_int& size, size_t sample_count)
{
    ff::graphics::internal::add_child(this);

    D3D11_TEXTURE2D_DESC texture_desc{};
    texture_desc.ArraySize = 1;
    texture_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    texture_desc.Format = ::DEPTH_STENCIL_FORMAT;
    texture_desc.Width = static_cast<UINT>(std::max(size.x, 1));
    texture_desc.Height = static_cast<UINT>(std::max(size.y, 1));
    texture_desc.MipLevels = 1;
    texture_desc.SampleDesc.Count = static_cast<UINT>(ff::graphics::fix_sample_count(::DEPTH_STENCIL_FORMAT, sample_count));
    texture_desc.Usage = D3D11_USAGE_DEFAULT;

    D3D11_DEPTH_STENCIL_VIEW_DESC view_desc{};
    view_desc.Format = texture_desc.Format;
    view_desc.ViewDimension = (texture_desc.SampleDesc.Count > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;

    bool status =
        SUCCEEDED(ff::graphics::internal::dx11_device()->CreateTexture2D(&texture_desc, nullptr, this->texture_.GetAddressOf())) &&
        SUCCEEDED(ff::graphics::internal::dx11_device()->CreateDepthStencilView(this->texture(), &view_desc, this->view_.GetAddressOf()));
    assert(status);
}

ff::dx11_depth::~dx11_depth()
{
    ff::graphics::internal::remove_child(this);
}

ff::dx11_depth::operator bool() const
{
    return this->texture_ && this->view_;
}

ff::point_int ff::dx11_depth::size() const
{
    D3D11_TEXTURE2D_DESC desc;
    this->texture_->GetDesc(&desc);
    return ff::point_t<UINT>(desc.Width, desc.Height).cast<int>();
}

bool ff::dx11_depth::size(const ff::point_int& size)
{
    if (this->size() != size)
    {
        dx11_depth new_depth(size, this->sample_count());
        if (!new_depth)
        {
            assert(false);
            return false;
        }

        *this = std::move(new_depth);
    }

    return true;
}

size_t ff::dx11_depth::sample_count() const
{
    D3D11_TEXTURE2D_DESC desc;
    this->texture_->GetDesc(&desc);
    return static_cast<size_t>(desc.SampleDesc.Count);
}

void ff::dx11_depth::clear(float depth, BYTE stencil) const
{
    ff::graphics::internal::dx11_device_state().clear_depth_stencil(this->view(), true, true, depth, stencil);
}

void ff::dx11_depth::clear_depth(float depth) const
{
    ff::graphics::internal::dx11_device_state().clear_depth_stencil(this->view(), true, false, depth, 0);
}

void ff::dx11_depth::clear_stencil(BYTE stencil) const
{
    ff::graphics::internal::dx11_device_state().clear_depth_stencil(this->view(), false, true, 0, stencil);
}

void ff::dx11_depth::discard() const
{
    ff::graphics::internal::dx11_device_context()->DiscardView1(this->view(), nullptr, 0);
}

ID3D11Texture2D* ff::dx11_depth::texture() const
{
    return this->texture_.Get();
}

ID3D11DepthStencilView* ff::dx11_depth::view() const
{
    return this->view_.Get();
}

bool ff::dx11_depth::reset()
{
    D3D11_TEXTURE2D_DESC desc;
    this->texture_->GetDesc(&desc);
    *this = dx11_depth(this->size(), this->sample_count());

    return *this;
}
