#include "pch.h"
#include "depth.h"
#include "device_reset_priority.h"
#include "device_state.h"
#include "globals.h"

static const DXGI_FORMAT DEPTH_STENCIL_FORMAT = DXGI_FORMAT_D24_UNORM_S8_UINT;

ff::dx11::depth::depth(size_t sample_count)
    : depth(ff::point_int(1, 1), sample_count)
{}

ff::dx11::depth::depth(const ff::point_int& size, size_t sample_count)
{
    ff::dx11::add_device_child(this, ff::dx11::device_reset_priority::normal);

    D3D11_TEXTURE2D_DESC texture_desc{};
    texture_desc.ArraySize = 1;
    texture_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    texture_desc.Format = ::DEPTH_STENCIL_FORMAT;
    texture_desc.Width = static_cast<UINT>(std::max(size.x, 1));
    texture_desc.Height = static_cast<UINT>(std::max(size.y, 1));
    texture_desc.MipLevels = 1;
    texture_desc.SampleDesc.Count = static_cast<UINT>(ff::dx11::fix_sample_count(::DEPTH_STENCIL_FORMAT, sample_count));
    texture_desc.Usage = D3D11_USAGE_DEFAULT;

    D3D11_DEPTH_STENCIL_VIEW_DESC view_desc{};
    view_desc.Format = texture_desc.Format;
    view_desc.ViewDimension = (texture_desc.SampleDesc.Count > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;

    bool status =
        SUCCEEDED(ff::dx11::device()->CreateTexture2D(&texture_desc, nullptr, this->texture_.GetAddressOf())) &&
        SUCCEEDED(ff::dx11::device()->CreateDepthStencilView(this->texture(), &view_desc, this->view_.GetAddressOf()));
    assert(status);
}

ff::dx11::depth::~depth()
{
    ff::dx11::remove_device_child(this);
}

ff::dx11::depth& ff::dx11::depth::get(ff::dxgi::depth_base& obj)
{
    return static_cast<ff::dx11::depth&>(obj);
}

const ff::dx11::depth& ff::dx11::depth::get(const ff::dxgi::depth_base& obj)
{
    return static_cast<const ff::dx11::depth&>(obj);
}

ff::dx11::depth::operator bool() const
{
    return this->texture_ && this->view_;
}

ff::point_int ff::dx11::depth::size() const
{
    D3D11_TEXTURE2D_DESC desc;
    this->texture_->GetDesc(&desc);
    return ff::point_t<UINT>(desc.Width, desc.Height).cast<int>();
}

bool ff::dx11::depth::size(const ff::point_int& size)
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

size_t ff::dx11::depth::sample_count() const
{
    D3D11_TEXTURE2D_DESC desc;
    this->texture_->GetDesc(&desc);
    return static_cast<size_t>(desc.SampleDesc.Count);
}

void ff::dx11::depth::clear(ff::dxgi::command_context_base& context, float depth, BYTE stencil) const
{
    ff::dx11::device_state::get(context).clear_depth_stencil(this->view(), true, true, depth, stencil);
}

void ff::dx11::depth::clear_depth(ff::dxgi::command_context_base& context, float depth) const
{
    ff::dx11::device_state::get(context).clear_depth_stencil(this->view(), true, false, depth, 0);
}

void ff::dx11::depth::clear_stencil(ff::dxgi::command_context_base& context, BYTE stencil) const
{
    ff::dx11::device_state::get(context).clear_depth_stencil(this->view(), false, true, 0, stencil);
}

ID3D11Texture2D* ff::dx11::depth::texture() const
{
    return this->texture_.Get();
}

ID3D11DepthStencilView* ff::dx11::depth::view() const
{
    return this->view_.Get();
}

bool ff::dx11::depth::reset()
{
    D3D11_TEXTURE2D_DESC desc;
    this->texture_->GetDesc(&desc);
    *this = ff::dx11::depth(this->size(), this->sample_count());

    return *this;
}
