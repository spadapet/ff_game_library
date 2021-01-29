#include "pch.h"
#include "dx11_device_state.h"
#include "dx11_fixed_state.h"

ff::dx11_fixed_state::dx11_fixed_state()
    : blend_factor(1, 1, 1, 1)
    , sample_mask(0xFFFFFFFF)
    , stencil(0)
{}

void ff::dx11_fixed_state::apply(dx11_device_state& context) const
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
