#include "pch.h"
#include "dx12/access.h"
#include "dx12/commands.h"
#include "dx12/depth.h"
#include "dx12/descriptor_allocator.h"
#include "dx12/device_reset_priority.h"
#include "dx12/globals.h"
#include "dx12/resource.h"
#include "dx12/texture_util.h"

static CD3DX12_RESOURCE_DESC depth_desc(const ff::point_size& size, size_t sample_count)
{
    return CD3DX12_RESOURCE_DESC::Tex2D(
        ff::dx12::depth::FORMAT,
        static_cast<UINT64>(std::max<size_t>(size.x, 1)),
        static_cast<UINT>(std::max<size_t>(size.y, 1)),
        1, 1, // array, mips
        static_cast<UINT>(ff::dx12::fix_sample_count(ff::dx12::depth::FORMAT, sample_count)), 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
}

static std::unique_ptr<ff::dx12::resource> depth_resource(const ff::point_size& size, size_t sample_count, std::shared_ptr<ff::dx12::mem_range> mem_range)
{
    static std::atomic_int depth_counter;

    return std::make_unique<ff::dx12::resource>(ff::string::concat("Depth buffer ", depth_counter.fetch_add(1)),
        mem_range, ::depth_desc(size, sample_count), D3D12_CLEAR_VALUE{ ff::dx12::depth::FORMAT });
}

ff::dx12::depth::depth(size_t sample_count)
    : depth(ff::point_size(1, 1), sample_count)
{}

ff::dx12::depth::depth(const ff::point_size& size, size_t sample_count)
    : view_(ff::dx12::cpu_depth_descriptors().alloc_range(1))
    , resource_(::depth_resource(size, sample_count, {}))
{
    this->reset();
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

ff::point_size ff::dx12::depth::physical_size() const
{
    const D3D12_RESOURCE_DESC& desc = this->resource_->desc();
    return ff::point_size(static_cast<size_t>(desc.Width), desc.Height);
}

bool ff::dx12::depth::physical_size(ff::dxgi::command_context_base& context, const ff::point_size& size)
{
    if (*this && this->physical_size() != size)
    {
        size_t sample_count = this->sample_count();
        auto old_resource = std::move(this->resource_);
        this->resource_ = ::depth_resource(size, sample_count, old_resource->mem_range());
        this->reset();

        if (this->resource_->mem_range() == old_resource->mem_range())
        {
            ff::dx12::commands& commands = ff::dx12::commands::get(context);
            commands.resource_alias(old_resource.get(), this->resource_.get());
        }
    }

    return *this;
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
