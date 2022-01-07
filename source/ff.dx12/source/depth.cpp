#include "pch.h"
#include "access.h"
#include "commands.h"
#include "depth.h"
#include "descriptor_allocator.h"
#include "device_reset_priority.h"
#include "globals.h"
#include "resource.h"
#include "texture_util.h"

ff::dx12::depth::depth(size_t sample_count)
    : depth(ff::point_size(1, 1), sample_count)
{}

ff::dx12::depth::depth(const ff::point_size& size, size_t sample_count)
    : view_(ff::dx12::cpu_depth_descriptors().alloc_range(1))
{
    D3D12_CLEAR_VALUE clear_value{ ff::dx12::depth::FORMAT };

    this->resource_ = std::make_unique<ff::dx12::resource>(
        CD3DX12_RESOURCE_DESC::Tex2D(
            ff::dx12::depth::FORMAT,
            static_cast<UINT64>(std::max<size_t>(size.x, 1)),
            static_cast<UINT>(std::max<size_t>(size.y, 1)),
            1, 1, // array, mips
            static_cast<UINT>(ff::dx12::fix_sample_count(ff::dx12::depth::FORMAT, sample_count)), 0,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
        clear_value);

    bool status = this->reset();
    assert(status);

    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::depth::depth(depth&& other) noexcept
{
    *this = std::move(other);
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
        *this = ff::dx12::depth(size, this->sample_count());
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

    ff::dx12::device()->CreateDepthStencilView(ff::dx12::get_resource(*this->resource_), &desc, this->view());

    return true;
}
