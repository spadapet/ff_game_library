#include "pch.h"
#include "device_state.h"
#include "fixed_state.h"
#include "globals.h"

ff::dx11::fixed_state::fixed_state()
    : blend_factor(1, 1, 1, 1)
    , sample_mask(0xFFFFFFFF)
    , stencil(0)
{}

void ff::dx11::fixed_state::apply() const
{
    this->apply(ff::dx11::get_device_state());
}

void ff::dx11::fixed_state::apply(ff::dx11::device_state& context) const
{
    if (this->raster)
    {
        context.set_raster(this->raster.Get());
    }

    if (this->blend)
    {
        context.set_blend(this->blend.Get(), this->blend_factor, this->sample_mask);
    }

    if (this->disabled_depth && !context.depth_view())
    {
        context.set_depth(this->disabled_depth.Get(), this->stencil);
    }
    else if (this->depth)
    {
        context.set_depth(this->depth.Get(), this->stencil);
    }
}
