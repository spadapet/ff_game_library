#include "pch.h"
#include "access.h"
#include "commands.h"
#include "depth.h"
#include "device_reset_priority.h"
#include "globals.h"
#include "resource.h"
#include "texture_util.h"

static const DXGI_FORMAT DEPTH_STENCIL_FORMAT = DXGI_FORMAT_D24_UNORM_S8_UINT;

ff::dx12::depth::depth(size_t sample_count)
    : depth(ff::point_size(1, 1), sample_count)
{}

ff::dx12::depth::depth(const ff::point_size& size, size_t sample_count)
{
    D3D12_CLEAR_VALUE clear_value{ ::DEPTH_STENCIL_FORMAT };

    this->resource_ = std::make_unique<ff::dx12::resource>(
        CD3DX12_RESOURCE_DESC::Tex2D(
            ::DEPTH_STENCIL_FORMAT,
            static_cast<UINT64>(std::max<size_t>(size.x, 1)),
            static_cast<UINT>(std::max<size_t>(size.y, 1)),
            1, 1, // array, mips
            static_cast<UINT>(ff::dx12::fix_sample_count(::DEPTH_STENCIL_FORMAT, sample_count))),
        clear_value);

    bool status = this->reset();
    assert(status);

    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::depth::~depth()
{
    ff::dx12::remove_device_child(this);
}

ff::dx12::depth& ff::dx12::depth::get(ff::dxgi::depth_base& obj)
{
    return static_cast<ff::dx12::depth&>(obj);
}

const ff::dx12::depth& ff::dx12::depth::get(const ff::dxgi::depth_base& obj)
{
    return static_cast<const ff::dx12::depth&>(obj);
}

ff::dx12::depth::operator bool() const
{
    return this->resource_ && this->view_;
}

ff::point_size ff::dx12::depth::size() const
{
    const D3D12_RESOURCE_DESC& desc = this->resource_->desc();
    return ff::point_size(desc.Width, desc.Height);
}

bool ff::dx12::depth::size(const ff::point_size& size)
{
    if (this->size() != size)
    {
        this->view_.free_range();
        this->resource_.reset();

        ff::dx12::depth new_depth(size, this->sample_count());
        if (!new_depth)
        {
            assert(false);
            return false;
        }

        *this = std::move(new_depth);
    }

    return true;
}

size_t ff::dx12::depth::sample_count() const
{
    return this->resource_->desc().SampleDesc.Count;
}

void ff::dx12::depth::clear(ff::dxgi::command_context_base& context, float depth, BYTE stencil) const
{
    this->clear(ff::dx12::commands::get(context), &depth, &stencil);
}

void ff::dx12::depth::clear_depth(ff::dxgi::command_context_base& context, float depth) const
{
    this->clear(ff::dx12::commands::get(context), &depth, nullptr);
}

void ff::dx12::depth::clear_stencil(ff::dxgi::command_context_base& context, BYTE stencil) const
{
    this->clear(ff::dx12::commands::get(context), nullptr, &stencil);
}

void ff::dx12::depth::clear(ff::dx12::commands& commands, const float* depth, const BYTE* stencil) const
{
    commands.clear(*this, depth, stencil);
}

ff::dx12::resource* ff::dx12::depth::resource() const
{
    return this->resource_.get();
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::dx12::depth::view() const
{
    return this->view_.cpu_handle(0);
}

bool ff::dx12::depth::reset()
{
    const D3D12_RESOURCE_DESC& res_desc = this->resource_->desc();
    D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
    desc.Format = res_desc.Format;
    desc.ViewDimension = (res_desc.SampleDesc.Count > 1) ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;

    ff::dx12::device()->CreateDepthStencilView(ff::dx12::get_resource(*this->resource_), &desc, this->view_.cpu_handle(0));

    return true;
}
