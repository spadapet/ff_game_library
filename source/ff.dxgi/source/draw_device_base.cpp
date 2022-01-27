#include "pch.h"
#include "draw_device_base.h"

ff::dxgi::draw_ptr ff::dxgi::draw_device_base::begin_draw(
    ff::dxgi::command_context_base& context,
    ff::dxgi::target_base& target,
    ff::dxgi::depth_base* depth,
    const ff::rect_fixed& view_rect,
    const ff::rect_fixed& world_rect,
    ff::dxgi::draw_options options)
{
    return this->begin_draw(context, target, depth, std::floor(view_rect).cast<float>(), std::floor(world_rect).cast<float>(), options);
}
